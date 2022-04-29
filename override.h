#ifndef OVERRIDE_H_
#define OVERRIDE_H_

#include "linux/export.h"

struct symbol_alias_args {
	const char *symbol;
	unsigned long alias;
};

#define DECLARE_SYMBOL_ALIAS(symbol, alias) \
	struct symbol_alias_args SYMBOL_ALIAS_##symbol \
		= { #symbol, (unsigned long) &alias }; \
	EXPORT_SYMBOL(SYMBOL_ALIAS_##symbol)

/**
 * struct init_args - Struct used to call init functions
 * @name:	Arbitrary name
 * @init:	Init function
 * @exit:	Exit function
 */
struct init_args {
	const char *name;
	int (*init)(void);
	void (*exit)(void);
};

#define __DECLARE_INIT(type, name, init, exit) \
	struct init_args type##INIT_##name \
		= { name, init, exit }; \
	EXPORT_SYMBOL(type##INIT_##name)

#define DECLARE_INIT(n, i, e) \
	__DECLARE_INIT("NORMAL_", n, i, e)

#define DECLARE_EARLY_INIT(n, i, e) \
	__DECLARE_INIT("EARLY_", n, i, e)

#define DECLARE_FINAL_INIT(n, i, e) \
	__DECLARE_INIT("FINAL_", n, i, e)

/**
 * struct updater_args - Struct used to update current data structures
 * @name:		Arbitrary name
 * @update:		Updater function
 * @update_private:	Updater function's private data member
 * @restore:		Updater restore function
 * @restore_private:	Updater restore function's private data member
 */
struct updater_args {
	const char *name;
	int (*update)(void *);
	void *update_private;
	void (*restore)(void *);
	void *restore_private;
};

#define __DECLARE_UPDATER(type, name, updater, updater_private, restorer, restorer_private) \
	struct updater_args type##UPDATER_##name \
		= { name, updater, updater_private, restorer, restorer_private }; \
	EXPORT_SYMBOL(type##UPDATER_##name)

#define DECLARE_UPDATER(n, u, up, r, rp) \
	__DECLARE_UPDATER("NORMAL_", n, u, up, r, rp)

#define DECLARE_EARLY_UPDATER(n, u, up, r, rp) \
	__DECLARE_UPDATER("EARLY_", n, u, up, r, rp)

/**
 * struct function_override_args - Struct used to override current functions
 * @name:	Kernel function's name
 * @addr:	Kernel function's current address, or 0 to auto-find
 *              Useful when duplicates are present in symbol table
 * @override:	Override function
 */
struct function_override_args {
	const char *name;
	unsigned long addr;
	unsigned long override;
};

#define DECLARE_FUNCTION_OVERRIDE(name, addr, override) \
	struct function_override_args FUNCTION_OVERRIDE_##name \
		= { #name, (unsigned long) addr, (unsigned long) &override }; \
	EXPORT_SYMBOL(FUNCTION_OVERRIDE_##name)

#endif /* OVERRIDE_H_ */
