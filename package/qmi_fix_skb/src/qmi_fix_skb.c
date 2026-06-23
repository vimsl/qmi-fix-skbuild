/*
 * qmi_fix_skb.c - Kprobe hotfix for qmi_wwan_f skb headroom bug
 *
 * Problem:
 *   qmi_wwan_f (Fibocom closed-source QMAP driver) calls __netdev_alloc_skb
 *   / __alloc_skb without reserving LL_MAX_HEADER. The driver's own tailroom
 *   check then prints "skb_tailroom small!" and drops the packet.
 *
 * Fix:
 *   Intercept __netdev_alloc_skb and __alloc_skb via kprobe.
 *   When the caller is qmi_wwan_f, add LL_MAX_HEADER to the requested size.
 *   Extra 176 bytes become tailroom headroom so tailroom check passes.
 *
 * Upstream reference:
 *   commit 2e4233870557 ("qmi_wwan: Increase headroom for QMAP SKBs")
 *
 * Usage on router:
 *   insmod qmi_fix_skb.ko
 *   cat /sys/module/qmi_fix_skb/parameters/count
 *   dmesg | grep qmi_fix_skb
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/skbuff.h>
#include <linux/kallsyms.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvis");
MODULE_DESCRIPTION("Kprobe hotfix for qmi_wwan_f skb headroom deficiency");

#define LL_MAX_HEADER   176

static int fix_count;
module_param_named(count, fix_count, int, 0444);
MODULE_PARM_DESC(count, "Total fix applications");

/* Cached qmi_wwan_f init function address for fast caller check.
 * Resolved lazily on first kprobe hit using kallsyms.
 */
static unsigned long qmi_init_addr;

/* Check if caller is from qmi_wwan_f module.
 * Strategy: resolve qmi_wwan_f's init function once, then check if caller
 * is within a reasonable range (within 1MB of init).
 */
static inline bool caller_is_qmi(struct pt_regs *regs)
{
	unsigned long caller = regs->regs[30]; /* ARM64 link register */

	if (qmi_init_addr != 0)
		return (caller >= qmi_init_addr) && (caller < qmi_init_addr + 0x100000);

	/* First hit: try to resolve qmi_wwan_f init function */
	{
		unsigned long addr = kallsyms_lookup_name("qmi_wwan_f_probe");
		if (addr == 0)
			addr = kallsyms_lookup_name("qmi_wwan_probe");
		if (addr != 0) {
			qmi_init_addr = addr;
			pr_info("qmi_fix_skb: resolved qmi_wwan_f at 0x%lx\n", addr);
			return (caller >= addr) && (caller < addr + 0x100000);
		}
		return false;
	}
}

/* kprobe pre_handler: __netdev_alloc_skb
 * __netdev_alloc_skb(dev, length, gfp)
 * ARM64:  x0=dev,  x1=length,  x2=gfp
 */
static int fix_netdev_alloc_pre(struct kprobe *kp, struct pt_regs *regs)
{
	if (!caller_is_qmi(regs))
		return 0;

	regs->regs[1] += LL_MAX_HEADER;  /* x1 = length */
	fix_count++;
	return 0;
}

/* kprobe pre_handler: __alloc_skb
 * __alloc_skb(size, gfp, flags, node)
 * ARM64:  x0=size,  x1=gfp,  x2=flags,  x3=node
 */
static int fix_alloc_skb_pre(struct kprobe *kp, struct pt_regs *regs)
{
	if (!caller_is_qmi(regs))
		return 0;

	regs->regs[0] += LL_MAX_HEADER;  /* x0 = size */
	fix_count++;
	return 0;
}

static struct kprobe kp_netdev_alloc = {
	.symbol_name = "__netdev_alloc_skb",
	.pre_handler = fix_netdev_alloc_pre,
};

static struct kprobe kp_alloc_skb = {
	.symbol_name = "__alloc_skb",
	.pre_handler = fix_alloc_skb_pre,
};

/* Module init/exit */

static int __init qmi_fix_init(void)
{
	int ret;

	ret = register_kprobe(&kp_netdev_alloc);
	if (ret) {
		pr_err("qmi_fix_skb: register_kprobe(__netdev_alloc_skb) = %d\n", ret);
		return ret;
	}

	ret = register_kprobe(&kp_alloc_skb);
	if (ret) {
		pr_err("qmi_fix_skb: register_kprobe(__alloc_skb) = %d\n", ret);
		unregister_kprobe(&kp_netdev_alloc);
		return ret;
	}

	pr_info("qmi_fix_skb: installed (caller detection via kallsyms)\n");
	return 0;
}

static void __exit qmi_fix_exit(void)
{
	unregister_kprobe(&kp_alloc_skb);
	unregister_kprobe(&kp_netdev_alloc);
	pr_info("qmi_fix_skb: removed, total fixes=%d\n", fix_count);
}

module_init(qmi_fix_init);
module_exit(qmi_fix_exit);
