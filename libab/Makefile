

include $(TOPDIR)/rules.mk

PKG_NAME:=libab
PKG_VERSION:=1.2
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/libab
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=libab
  DEPENDS:=+kmod-ltq-tapi +kmod-ltq-vmmc @TARGET_lantiq
endef  

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/drv_tapi -I$(STAGING_DIR)/usr/include/drv_vmmc

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./libab/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/Compile
	@echo
	@echo "BUILD LIBAB..."
	cd $(PKG_BUILD_DIR) && rm -f *.o *.a
	cd $(PKG_BUILD_DIR) && $(TARGET_CONFIGURE_OPTS) \
		$(TARGET_CC) $(TARGET_CFLAGS) \
		-Wall \
		ab_basic.c \
		ab_line.c \
		ab_events.c \
		ab_media.c \
		-c
	cd $(PKG_BUILD_DIR) && $(AR) cr libab.a *.o
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/libab/
	$(INSTALL_DIR) $(1)/usr/lib/
	cp $(PKG_BUILD_DIR)/libab.a $(1)/usr/lib/
	cp $(PKG_BUILD_DIR)/*.h $(1)/usr/include/libab/
endef
	
define Package/libab/install
	echo "INSTALL"
endef

$(eval $(call BuildPackage,libab))
