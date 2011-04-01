include $(TOPDIR)/rules.mk

PKG_NAME        = svd
PKG_VERSION     = 1
PKG_RELEASE     = 1

PKG_DIR         = svd

   PKG_CONF_DIR = $(TOPDIR)/package/$(PKG_NAME)
    PKG_SRC_DIR = $(PKG_CONF_DIR)/$(PKG_DIR)
  PKG_BUILD_DIR =    $(BUILD_DIR)/$(PKG_DIR)
PKG_INSTALL_DIR = $(PKG_BUILD_DIR)/ipkg-install

include $(TOPDIR)/package/target-configure-staging-flags.mk
TARGET_CFLAGS += -I$(BUILD_DIR)/tapi/include
TARGET_CFLAGS += -I$(BUILD_DIR)/libab
###FIXME: TARGET_CFLAGS += -DDONT_BIND_TO_DEVICE=1

include $(TOPDIR)/package/rules.mk



$(eval $(call PKG_template,SVD,svd,$(PKG_VERSION)-$(PKG_RELEASE),$(ARCH)))


$(PKG_BUILD_DIR)/.prepared:
	rm -rf               $(PKG_BUILD_DIR)
	cp -a $(PKG_SRC_DIR) $(PKG_BUILD_DIR)
	mkdir -p $(PKG_INSTALL_DIR)
	touch $@

$(PKG_BUILD_DIR)/.configured:  $(PKG_BUILD_DIR)/.prepared
	@echo
	@echo "CONFIGURE SVD..."
	cd $(PKG_BUILD_DIR) && aclocal
	cd $(PKG_BUILD_DIR) && autoheader
	cd $(PKG_BUILD_DIR) && automake -a --foreign
	cd $(PKG_BUILD_DIR) && autoconf
	cd $(PKG_BUILD_DIR) && $(TARGET_CONFIGURE_OPTS) \
		./configure \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=$(PKG_INSTALL_DIR)
	touch $@

$(PKG_BUILD_DIR)/.built:  $(PKG_BUILD_DIR)/.configured
	@echo
	@echo "BUILD SVD..."
	cd $(PKG_BUILD_DIR) && $(TARGET_CONFIGURE_OPTS) make
	cd $(PKG_BUILD_DIR) && $(TARGET_CONFIGURE_OPTS) make install
	touch $@

