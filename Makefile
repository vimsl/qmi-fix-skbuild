# Makefile for qmi_fix_skb.ko — OpenWrt kernel module
#
# Usage (on router with build environment):
#   make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
#
# Usage (cross-compile in OpenWrt SDK):
#   export STAGING_DIR=/path/to/openwrt/staging_dir
#   export TOOLCHAIN=$STAGING_DIR/toolchain-aarch64_cortex-a55_gcc-13.3.0_musl
#   export KERNEL_BUILD=$STAGING_DIR/target-aarch64_cortex-a55_musl/linux-mediatek_filogic/linux-6.6.94
#   make ARCH=arm64 CROSS_COMPILE=aarch64-openwrt-linux-musl- \
#        -C $KERNEL_BUILD M=$(pwd) modules

obj-m := qmi_fix_skb.o

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f *.order *.symvers
