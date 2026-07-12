################################################################################
#
# x21b-led-helper
#
################################################################################

X21B_LED_HELPER_VERSION = 1.0
X21B_LED_HELPER_SITE = $(TOPDIR)/package/x21b-led-helper/src
X21B_LED_HELPER_SITE_METHOD = local
X21B_LED_HELPER_DEPENDENCIES = libgpiod

define X21B_LED_HELPER_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -Wall -Wextra -O2 \
		-o $(@D)/x21b-led-helper \
		$(@D)/x21b-led-helper.c $(@D)/ohdled.c \
		-lgpiod

	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -Wall -Wextra -O2 \
		-o $(@D)/ohdledctl \
		$(@D)/ohdledctl.c $(@D)/ohdled.c
endef

define X21B_LED_HELPER_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/x21b-led-helper \
		$(TARGET_DIR)/usr/bin/x21b-led-helper

	$(INSTALL) -D -m 0755 $(@D)/ohdledctl \
		$(TARGET_DIR)/usr/bin/ohdledctl

	$(INSTALL) -D -m 0755 $(X21B_LED_HELPER_PKGDIR)/start.sh \
		$(TARGET_DIR)/etc/init.d/S25x21bled
endef

$(eval $(generic-package))
