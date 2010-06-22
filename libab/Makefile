include $(TOPDIR)/rules.mk

PKG_NAME        = libab
PKG_VERSION     = 1
PKG_RELEASE     = 1

        PKG_DIR = libab
   PKG_CONF_DIR = $(TOPDIR)/package/$(PKG_NAME)
    PKG_SRC_DIR = $(PKG_CONF_DIR)/$(PKG_DIR)
  PKG_BUILD_DIR =    $(BUILD_DIR)/$(PKG_DIR)

include $(TOPDIR)/package/target-configure-staging-flags.mk
TARGET_CFLAGS += -I$(BUILD_DIR)/tapi/include
TARGET_CFLAGS += -I$(BUILD_DIR)/vinetic/include
TARGET_CFLAGS += -I$(BUILD_DIR)/sgatab

include $(TOPDIR)/package/rules.mk


$(eval $(call PKG_template,LIBAB,libab,$(PKG_VERSION)-$(PKG_RELEASE),$(ARCH)))

$(PKG_BUILD_DIR)/.prepared:
	rm -rf               $(PKG_BUILD_DIR)
	cp -a $(PKG_SRC_DIR) $(PKG_BUILD_DIR)
	touch $@

$(PKG_BUILD_DIR)/.configured:
	touch $@

$(PKG_BUILD_DIR)/.built:
	@echo
	@echo "BUILD LIBAB..."
	cd $(PKG_BUILD_DIR) && rm -f *.o *.a
	cd $(PKG_BUILD_DIR) && $(TARGET_CONFIGURE_OPTS) \
		$(TARGET_CC) $(TARGET_CFLAGS) \
		-Wall \
		ab_hwinit.c \
		ab_basic.c \
		ab_line.c \
		ab_events.c \
		ab_media.c \
		-c
	cd $(PKG_BUILD_DIR) && $(AR) cr libab.a *.o
	touch $@

