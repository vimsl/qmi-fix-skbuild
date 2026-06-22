include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qmi_fix_skb
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/qmi_fix_skb
  SUBMENU:=Other modules
  TITLE:=QMI WWAN SKB Tailroom Fix via Kprobe
  FILES:=$(PKG_BUILD_DIR)/qmi_fix_skb.ko
  AUTOLOAD:=$(call AutoLoad, 99, qmi_fix_skb)
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		M="$(PKG_BUILD_DIR)" \
		modules
endef

$(eval $(call KernelPackage,qmi_fix_skb))
