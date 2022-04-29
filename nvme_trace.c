#include "linux/module.h"
#include "linux/blkdev.h"
#include "linux/compat.h"
#include "linux/printk.h"
#include "linux/nvme.h"
#include "linux/nvme_ioctl.h"
#include "override.h"

// Partial definition of structs needed for this patch
// Valid for 5.17 but may not be correct in future kernels
struct nvme_ns {
	struct list_head list;

	struct nvme_ctrl *ctrl;
	struct request_queue *queue;
};

enum nvme_ctrl_state { dummy };
struct nvme_ctrl {
	bool comp_seen;
	enum nvme_ctrl_state state;
	bool identified;
	spinlock_t lock;
	struct mutex scan_lock;
	const struct nvme_ctrl_ops *ops;
	struct request_queue *admin_q;
};

static int (*nvme_submit_user_cmd_p)(struct request_queue *q,
		struct nvme_command *cmd, void __user *ubuffer,
		unsigned bufflen, void __user *meta_buffer, unsigned meta_len,
		u32 meta_seed, u64 *result, unsigned timeout, bool vec);
DECLARE_SYMBOL_ALIAS(nvme_submit_user_cmd, nvme_submit_user_cmd_p);

static void __user *__nvme_to_user_ptr(uintptr_t ptrval)
{
	if (in_compat_syscall())
		ptrval = (compat_uptr_t)ptrval;
	return (void __user *)ptrval;
}

#define SET_FEATURES 0x9
static int __nvme_user_cmd(struct nvme_ctrl *ctrl, struct nvme_ns *ns,
			struct nvme_passthru_cmd __user *ucmd)
{
	struct nvme_passthru_cmd cmd;
	struct nvme_command c;
	unsigned timeout = 0;
	u64 result;
	int status;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;
	if (copy_from_user(&cmd, ucmd, sizeof(cmd)))
		return -EFAULT;
	if (cmd.flags)
		return -EINVAL;

	memset(&c, 0, sizeof(c));
	c.common.opcode = cmd.opcode;
	c.common.flags = cmd.flags;
	c.common.nsid = cpu_to_le32(cmd.nsid);
	c.common.cdw2[0] = cpu_to_le32(cmd.cdw2);
	c.common.cdw2[1] = cpu_to_le32(cmd.cdw3);
	c.common.cdw10 = cpu_to_le32(cmd.cdw10);
	c.common.cdw11 = cpu_to_le32(cmd.cdw11);
	c.common.cdw12 = cpu_to_le32(cmd.cdw12);
	c.common.cdw13 = cpu_to_le32(cmd.cdw13);
	c.common.cdw14 = cpu_to_le32(cmd.cdw14);
	c.common.cdw15 = cpu_to_le32(cmd.cdw15);

	if (cmd.opcode == SET_FEATURES) {
		pr_info("Set Features %#x\n", c.common.cdw10);
		pr_info("User Pointer %#llx Length %#x\n",
			(unsigned long long) __nvme_to_user_ptr(cmd.addr),
			cmd.data_len);
		pr_info("CDW2 %#x\n", c.common.cdw2[0]);
		pr_info("CDW3 %#x\n", c.common.cdw2[1]);
		pr_info("CDW11 %#x\n", c.common.cdw11);
		pr_info("CDW12 %#x\n", c.common.cdw12);
		pr_info("CDW13 %#x\n", c.common.cdw13);
		pr_info("CDW14 %#x\n", c.common.cdw14);
		pr_info("CDW15 %#x\n", c.common.cdw15);
	}

	if (cmd.timeout_ms)
		timeout = msecs_to_jiffies(cmd.timeout_ms);

	status = nvme_submit_user_cmd_p(ns ? ns->queue : ctrl->admin_q, &c,
			__nvme_to_user_ptr(cmd.addr), cmd.data_len,
			__nvme_to_user_ptr(cmd.metadata), cmd.metadata_len,
			0, &result, timeout, false);

	if (status >= 0) {
		if (put_user(result, &ucmd->result))
			return -EFAULT;
	}

	return status;
}
DECLARE_FUNCTION_OVERRIDE(nvme_user_cmd, 0, __nvme_user_cmd);

static int __nvme_user_cmd64(struct nvme_ctrl *ctrl, struct nvme_ns *ns,
			struct nvme_passthru_cmd64 __user *ucmd, bool vec)
{
	struct nvme_passthru_cmd64 cmd;
	struct nvme_command c;
	unsigned timeout = 0;
	int status;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;
	if (copy_from_user(&cmd, ucmd, sizeof(cmd)))
		return -EFAULT;
	if (cmd.flags)
		return -EINVAL;

	memset(&c, 0, sizeof(c));
	c.common.opcode = cmd.opcode;
	c.common.flags = cmd.flags;
	c.common.nsid = cpu_to_le32(cmd.nsid);
	c.common.cdw2[0] = cpu_to_le32(cmd.cdw2);
	c.common.cdw2[1] = cpu_to_le32(cmd.cdw3);
	c.common.cdw10 = cpu_to_le32(cmd.cdw10);
	c.common.cdw11 = cpu_to_le32(cmd.cdw11);
	c.common.cdw12 = cpu_to_le32(cmd.cdw12);
	c.common.cdw13 = cpu_to_le32(cmd.cdw13);
	c.common.cdw14 = cpu_to_le32(cmd.cdw14);
	c.common.cdw15 = cpu_to_le32(cmd.cdw15);

	if (cmd.opcode == SET_FEATURES) {
		pr_info("Set Features %#x\n", c.common.cdw10);
		pr_info("User Pointer %#llx Length %#x\n",
			(unsigned long long) __nvme_to_user_ptr(cmd.addr),
			cmd.data_len);
		pr_info("CDW2 %#x\n", c.common.cdw2[0]);
		pr_info("CDW3 %#x\n", c.common.cdw2[1]);
		pr_info("CDW11 %#x\n", c.common.cdw11);
		pr_info("CDW12 %#x\n", c.common.cdw12);
		pr_info("CDW13 %#x\n", c.common.cdw13);
		pr_info("CDW14 %#x\n", c.common.cdw14);
		pr_info("CDW15 %#x\n", c.common.cdw15);
	}

	if (cmd.timeout_ms)
		timeout = msecs_to_jiffies(cmd.timeout_ms);

	status = nvme_submit_user_cmd_p(ns ? ns->queue : ctrl->admin_q, &c,
			__nvme_to_user_ptr(cmd.addr), cmd.data_len,
			__nvme_to_user_ptr(cmd.metadata), cmd.metadata_len,
			0, &cmd.result, timeout, vec);

	if (status >= 0) {
		if (put_user(cmd.result, &ucmd->result))
			return -EFAULT;
	}

	return status;
}
DECLARE_FUNCTION_OVERRIDE(nvme_user_cmd64, 0, __nvme_user_cmd64);

