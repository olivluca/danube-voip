#
# Copyright (C) 2011 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=sofia-sip
PKG_VERSION:=1.12.11
PKG_RELEASE:=1

PKG_SOURCE:=sofia-sip-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=@SF/$(PKG_NAME)
PKG_MD5SUM:=f3582c62080eeecd3fa4cd5d4ccb4225
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/sofia-sip
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=sofia-sip
  URL:=http://sofia-sip.sourceforge.net
  DEPENDS:=+libpthread
endef

define Package/sofia-sip/description
Open source SIP User-Agent library
endef

TARGET_CFLAGS += $(FPIC)

define Build/Compile
	$(call Build/Compile/Default)
	make -C $(PKG_BUILD_DIR) \
		DESTDIR="$(PKG_INSTALL_DIR)" \
		install
endef		

define Build/InstallDev
	$(INSTALL_DIR) $(1)
	$(CP) $(PKG_INSTALL_DIR)/* $(1)/
endef

define Package/sofia-sip/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libsofia-sip-ua.so.* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,sofia-sip))
