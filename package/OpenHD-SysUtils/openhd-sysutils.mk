###################################################################
# OpenHD
#
# Licensed under the GNU General Public License (GPL) Version 3.
#
# This software is provided "as-is," without warranty of any kind, express or
# implied, including but not limited to the warranties of merchantability,
# fitness for a particular purpose, and non-infringement. For details, see the
# full license in the LICENSE file provided with this source code.
#
# Non-Military Use Only:
# This software and its associated components are explicitly intended for
# civilian and non-military purposes. Use in any military or defense
# applications is strictly prohibited unless explicitly and individually
# licensed otherwise by the OpenHD Team.
#
# Contributors:
# A full list of contributors can be found at the OpenHD GitHub repository:
# https://github.com/OpenHD/OpenHD-SysUtils
#
# © OpenHD, All Rights Reserved.
################################################################################
$(info Building the OpenHD-SysUtils package...)

# The Git repository from which to clone the source code
OPENHD_SYSUTILS_SITE = https://github.com/OpenHD/OpenHD-SysUtils.git
OPENHD_SYSUTILS_SITE_METHOD = git
OPENHD_SYSUTILS_GIT_SUBMODULES = YES

# Set the version to the latest commit of the default branch
OPENHD_SYSUTILS_VERSION = main

# Enable Git submodules if the project requires them
OPENHD_SYSUTILS_GIT_SUBMODULES = YES

# Install to the target system
OPENHD_SYSUTILS_INSTALL_TARGET = YES

# List of dependencies that must be built before OpenHD
OPENHD_SYSUTILS_DEPENDENCIES = poco

# Install init.d services to target
define OPENHD_SYSUTILS_INSTALL_TARGET_CMDS
    $(info OpenHD-SysUtils Build Directory: $(OPENHD_SYSUTILS_BUILDDIR))
    $(INSTALL) -D -m 0755 $(OPENHD_SYSUTILS_BUILDDIR)/openhd_sys_utils $(TARGET_DIR)/usr/bin/openhd_sys_utils
    $(INSTALL) -d $(TARGET_DIR)/etc/init.d
    cp -r $(OPENHD_SYSUTILS_PKGDIR)/start.sh  $(TARGET_DIR)/etc/init.d/S98openhd-sysutils
    chmod +x $(TARGET_DIR)/etc/init.d/*
endef

# Use Buildroot's CMake package infrastructure to handle the build
$(eval $(cmake-package))
