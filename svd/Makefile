include $(TOPDIR)/rules.mk

PKG_NAME:=svd
PKG_VERSION:=1.4
PKG_RELEASE:=1
PKG_BUILD_DEPENDS:=libab

PKG_FIXUP:=libtool
PKG_INSTALL:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/svd
  SECTION:=net
  CATEGORY:=Network
  TITLE:=svd
  SUBMENU:=Telephony
  DEPENDS:=+sofia-sip +libuci +kmod-ltq-tapi +kmod-ltq-vmmc @TARGET_lantiq
endef

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/drv_tapi -I$(STAGING_DIR)/usr/include/libab -I$(STAGING_DIR)/usr/include/libconfig
TARGET_CFLAGS += -DDONT_BIND_TO_DEVICE=1

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./svd/* $(PKG_BUILD_DIR)/
endef

define Build/InstallDev
  echo "INSTALLDEV"
endef

define Package/svd/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/svd $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/svd_if $(1)/usr/bin
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) ./files/svd.default $(1)/etc/config/svd
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/svd.init $(1)/etc/init.d/svd
endef

$(eval $(call BuildPackage,svd))

