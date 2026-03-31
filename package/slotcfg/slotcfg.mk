################################################################################
#
# slotcfg
#
################################################################################

SLOTCFG_VERSION = 1.0
SLOTCFG_SITE = $(TOPDIR)/package/slotcfg/src
SLOTCFG_SITE_METHOD = local

define SLOTCFG_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -Wall -Wextra -O2 \
		-o $(@D)/slotcfg $(@D)/slotcfg.c
endef

define SLOTCFG_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/slotcfg $(TARGET_DIR)/usr/bin/slotcfg
endef

$(eval $(generic-package))
