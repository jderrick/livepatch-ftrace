#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/ftrace.h>
#include <linux/version.h>
#include "override.h"

static LIST_HEAD(override_list);

struct ftrace_arg {
	struct list_head list;
	char name[KSYM_NAME_LEN];
	unsigned long addr;
	unsigned long override;
	struct ftrace_ops fops;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0)
static void notrace ftrace_func(unsigned long ip, unsigned long parent_ip,
				struct ftrace_ops *op, struct pt_regs *regs)
{
	struct ftrace_arg *args = container_of(op, struct ftrace_arg, fops);
	pr_debug("%s() name: %s override: %lx\n", __func__, args->name, args->override);
	regs->ip = args->override;
}
#else
static void notrace ftrace_func(unsigned long ip, unsigned long parent_ip,
				struct ftrace_ops *op, struct ftrace_regs *regs)
{
	struct ftrace_arg *args = container_of(op, struct ftrace_arg, fops);
	pr_debug("%s() name: %s override: %lx\n", __func__, args->name, args->override);
	regs->regs.ip = args->override;
}
#endif

int ftrace_override(const char *name, unsigned long addr, unsigned long override)
{
	int ret;

	struct ftrace_arg *args = kzalloc(sizeof(*args), GFP_KERNEL);
	if (!args)
		return -ENOMEM;

	args->fops.func = ftrace_func;
	args->fops.flags = FTRACE_OPS_FL_SAVE_REGS |
		  FTRACE_OPS_FL_DYNAMIC |
		  FTRACE_OPS_FL_IPMODIFY;
	args->addr = addr;
	args->override = override;
	strlcpy(args->name, name, sizeof(args->name)) ;


	ret = ftrace_set_filter_ip(&args->fops, args->addr, 0, 0);
	if (ret) {
		printk("ftrace_set_filter_ip failed\n");
		goto out;
	}

	ret = register_ftrace_function(&args->fops);
	if (ret) {
		printk("register_ftrace_function failed\n");
		ftrace_set_filter_ip(&args->fops, args->addr, 1, 0);
		goto out;
	}

	list_add_tail(&args->list, &override_list);
out:
	if (ret)
		kfree(args);

	return ret;
}

static void override_unregister(void)
{
	struct ftrace_arg *args, *tmp;

	list_for_each_entry_safe(args, tmp, &override_list, list) {
		unregister_ftrace_function(&args->fops);
		ftrace_set_filter_ip(&args->fops, args->addr, 1, 0);
		kfree(args);
	}
}

static int call_inits(void *data)
{
	struct init_args *args = data;

	pr_info("Calling init %s\n", args->name);
	return args->init();
}

static int call_exits(void *data)
{
	struct init_args *args = data;

	pr_info("Calling exit %s\n", args->name);
	args->exit();
	return 0;
}

static void restore_function_overrides(void)
{
	override_unregister();
}

static int override_built_in(void *data)
{
	struct function_override_args *args = data;
	unsigned long cur_addr;
	int ret = 0;

	pr_info("Installing override for %s\n", args->name);
	if (args->addr)
		cur_addr = args->addr;
	else
		cur_addr = kallsyms_lookup_name(args->name);

	if (!cur_addr) {
		pr_err("Couldn't find symbol %s\n", args->name);
		return -EINVAL;
	}

	ret = ftrace_override(args->name, cur_addr, args->override);
	if (ret)
		restore_function_overrides();

	return ret;
}

static int do_updaters(void *data)
{
	struct updater_args *updater = data;
	int ret = 0;

	if (updater->update) {
		pr_info("Calling updater %s\n", updater->name);
		ret = updater->update(updater->update_private);
	}

	return ret;
}

static int revert_updaters(void *data)
{
	struct updater_args *updater = (struct updater_args *)data;

	if (updater->restore) {
		pr_info("Reverting updater %s\n", updater->name);
		updater->restore(updater->restore_private);
	}

	return 0;
}

static int initialize_symbol_references(void *data)
{
	struct symbol_alias_args *args = data;
	unsigned long symbol_addr;

	symbol_addr = kallsyms_lookup_name(args->symbol);
	if (symbol_addr && args->alias) {
		pr_info("Assigning alias for symbol '%s'\n", args->symbol);
		*((unsigned long *)args->alias) = symbol_addr;
		return 0;
	}

	pr_err("Couldn't find symbol %s\n", args->symbol);
	return -EINVAL;
}

static const char *symbol_name(struct mod_kallsyms *kallsyms, unsigned int symnum)
{
	return kallsyms->strtab + kallsyms->symtab[symnum].st_name;
}

static inline unsigned long symbol_value(const Elf_Sym *sym)
{
	return sym->st_value;
}

static int on_each_override(int (*fn)(void *), const char *prefix)
{
	int i;
	struct mod_kallsyms *kallsyms = (THIS_MODULE)->kallsyms;

	for (i=0; i< kallsyms->num_symtab; i++) {
		if (!strncmp(prefix, symbol_name(kallsyms, i), strlen(prefix)))
			fn((void *)symbol_value(&kallsyms->symtab[i]));
	}
	return 0;
}

int override_init(void)
{
	int ret;

	ret = on_each_override(call_inits, "EARLY_INIT");
	if (ret)
		return ret;

	ret = on_each_override(initialize_symbol_references, "SYMBOL_ALIAS");
	if (ret)
		goto call_early_exits;

	ret = on_each_override(call_inits, "NORMAL_INIT");
	if (ret)
		goto call_early_exits;

	ret = on_each_override(do_updaters, "EARLY_UPDATER");
	if (ret)
		goto call_normal_exits;

	ret = on_each_override(override_built_in, "FUNCTION_OVERRIDE");
	if (ret)
		goto restore_early_updates;

	ret = on_each_override(do_updaters, "NORMAL_UPDATER");
	if (ret)
		goto restore_built_ins;

	ret = on_each_override(call_inits, "FINAL_INIT");
	if (ret)
		goto restore_updates;

	return 0;

restore_updates:
	on_each_override(revert_updaters, "NORMAL_UPDATER");
restore_built_ins:
	restore_function_overrides();
restore_early_updates:
	on_each_override(revert_updaters, "EARLY_UPDATER");
call_normal_exits:
	on_each_override(call_exits, "NORMAL_INIT");
call_early_exits:
	on_each_override(call_exits, "EARLY_INIT");
	return ret;
}

void override_exit(void)
{
	on_each_override(call_exits, "FINAL_INIT");
	on_each_override(revert_updaters, "NORMAL_UPDATER");
	restore_function_overrides();
	on_each_override(revert_updaters, "EARLY_UPDATER");
	on_each_override(call_exits, "NORMAL_INIT");
	on_each_override(call_exits, "EARLY_INIT");
}

MODULE_AUTHOR("Jonathan Derrick");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");

module_init(override_init);
module_exit(override_exit);
