#    Copyright (C) 2014 Luca Olivetti
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
#    The full GNU General Public License is included in this distribution in
#    the file called "COPYING".

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-svd
PKG_RELEASE:=1.3

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/host-build.mk

define Package/luci-svd
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:= Svd (voip) configuration and status module
  DEPENDS:=+luci-lib-nixio +luci-mod-admin-full +svd
  MAINTAINER:= Luca Olivetti <luca@ventoso.org>
endef

define Package/luci-svd/description
	svd (voip) web module for LuCi web interface
endef

define Build/Prepare
endef

define Build/Configure
endef

define Build/Compile
endef

define Package/luci-svd/install
	${INSTALL_DIR} ${1}/usr/lib/lua/luci
	$(CP) ./files/luasrc/* $(1)/usr/lib/lua/luci/
	${INSTALL_DIR} ${1}/www
	$(CP) ./files/htdocs/* ${1}/www/
	./files/i18n-po2lua.pl ${HOST_BUILD_DIR}/po2lmo ./files/po ${1}/usr/lib/lua/luci/i18n
endef

define Host/Prepare
	$(CP) ./files/po-src/* $(HOST_BUILD_DIR)
endef

define Host/Compile
	+$(MAKE) $(HOST_JOBS) -C $(HOST_BUILD_DIR) CC="$(HOSTCC)" compile
endef

define Host/Install
endef

$(eval $(call HostBuild))
$(eval $(call BuildPackage,luci-svd))
