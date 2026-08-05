# qmi-fix-skbuild

Build environment for `qmi_fix_skb.ko` — a kprobe-based kernel module that patches QMI WWAN skb headroom issues on MediaTek MT798x platforms.

## Purpose

The `qmi_wwan_f` driver can trigger skb headroom/tailroom warnings when the 5G modem sends packets requiring additional LL_MAX_HEADER space. This module uses kprobes to intercept and fix the skb allocation at runtime.

## CI Build

GitHub Actions workflow automatically downloads Linux kernel 6.6.148, prepares ARM64 cross-compilation, compiles the module, and creates a release.

## Manual Build

```bash
sudo apt install gcc-aarch64-linux-gnu make libelf-dev
wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.148.tar.xz
tar -xf linux-6.6.148.tar.xz && cd linux-6.6.148
cp ../config-6.6.148.txt .config
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- olddefconfig
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules_prepare
KBUILD_MODPOST_WARN=1 make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C . M=$PWD/.. modules
```

## Files

- `qmi_fix_skb.c` — Kernel module source
- `Makefile` — Out-of-tree build Makefile
- `config-6.6.148.txt` — Minimal kernel config for module compilation

## Related

- [qmi-headroom-fix](https://github.com/vimsl/qmi-headroom-fix)
- [Airpi-AP5000M-CloseWRT](https://github.com/vimsl/Airpi-AP5000M-CloseWRT)

## License

GPL-2.0