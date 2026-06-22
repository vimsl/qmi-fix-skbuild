/*
 * qmi_fix_skb.c — Kprobe hotfix for qmi_wwan_f skb headroom bug
 *
 * Problem:
 *   qmi_wwan_f (Fibocom closed-source QMAP driver) calls __netdev_alloc_skb
 *   / __alloc_skb without reserving LL_MAX_HEADER. The driver's own tailroom
 *   check then prints "skb_tailroom small!" and drops the packet.
 *
 * Fix:
 *   Intercept __netdev_alloc_skb and __alloc_skb via kprobe.
 *   When the caller is qmi_wwan_f, add LL_MAX_HEADER to the requested size.
 *   Extra 176 bytes become tailroom headroom → tailroom check passes.
 *
 * Upstream reference:
 *   commit 2e4233870557 ("qmi_wwan: Increase headroom for QMAP SKBs")
 *   commit 61356088acdd ("qmi_wwan: Add support for QMAP padding")
 *
 * Compile (cross-compile via OpenWrt SDK):
 *   make ARCH=arm64 CROSS_COMPILE=aarch64-openwrt-linux-musl- \
 *        -C $STAGING_DIR/target-*/linux-mediatek_filogic/linux-6.6.94 \
 *        M=$(pwd) modules
 *
 * Usage on router:
 *   insmod qmi_fix_skb.ko
 *   cat /sys/module/qmi_fix_skb/parameters/count  # check fix count
 *   dmesg | grep qmi_fix_skb
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/skbuff.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvis");
MODULE_DESCRIPTION("Kprobe hotfix for qmi_wwan_f skb headroom deficiency");

#define LL_MAX_HEADER   176
#define TARGET_NAME     "qmi_wwan_f"

static int fix_count;
module_param_named(count, fix_count, int, 0444);
MODULE_PARM_DESC(count, "Total fix applications");

/* Cached qmi_wwan_f .text range for fast caller check.
 * Resolved lazily on first kprobe hit, using __module_text_address().
 * After resolution, handler only does a range comparison (single-cycle).
 */
static unsigned long qmi_text_start, qmi_text_end;

/* Called on every kprobe hit. On first qmi_wwan_f call, resolves and caches
 * the module text range via __module_text_address(). Subsequent calls use
 * fast range comparison only.
 */
static inline bool caller_is_qmi(struct pt_regs *regs)
{
	unsigned long caller = regs->regs[30]; /* ARM64 link register */

	if (qmi_text_end != 0)
		return caller >= qmi_text_start && caller < qmi_text_end;

	/* First hit: resolve qmi_wwan_f text range once */
	{
		struct module *mod = __module_text_address(caller);
		if (!mod || strcmp(mod->name, TARGET_NAME))
			return false;

		qmi_text_start = (unsigned long)mod->core_layout.base;
		qmi_text_end   = qmi_text_start + mod->core_layout.text_size;

		pr_info("qmi_fix_skb: resolved %s .text [0x%lx - 0x%lx]\n",
			TARGET_NAME, qmi_text_start, qmi_text_end);
		return true;
	}
}

/* ── kprobe pre_handler: __netdev_alloc_skb ────────────────────── */
/* __netdev_alloc_skb(dev, length, gfp)
 * ARM64:  x0=dev,  x1=length,  x2=gfp
 * Return: lr (x30) = address in caller after BL instruction
 */
static int fix_netdev_alloc_pre(struct kprobe *kp, struct pt_regs *regs)
{
	if (!caller_is_qmi(regs))
		return 0;

	regs->regs[1] += LL_MAX_HEADER;  /* x1 = length */
	fix_count++;
	return 0;
}

/* ── kprobe pre_handler: __alloc_skb ───────────────────────────── */
/* __alloc_skb(size, gfp, flags, node)
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

/* ── Module init/exit ──────────────────────────────────────────── */

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

	pr_info("qmi_fix_skb: installed (range resolved lazily on first hit)\n");
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
