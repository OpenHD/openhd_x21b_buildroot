################################################################################
#
# stm32flash
#
################################################################################

STM32FLASH_VERSION = 16547fd08e13555a87c1d5c7160e690de0c577e9
STM32FLASH_SITE = https://git.code.sf.net/p/stm32flash/code
STM32FLASH_SITE_METHOD = git
STM32FLASH_LICENSE = GPL-2.0+1
STM32FLASH_LICENSE_FILES = gpl-2.0.txt

define STM32FLASH_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D)
endef

define STM32FLASH_INSTALL_TARGET_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) DESTDIR="$(TARGET_DIR)" PREFIX="/usr" \
		-C $(@D) install
endef

$(eval $(generic-package))
