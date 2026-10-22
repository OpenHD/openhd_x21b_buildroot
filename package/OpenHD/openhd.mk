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
# https://github.com/OpenHD
#
# © OpenHD, All Rights Reserved.
################################################################################
$(info Building the OpenHD package...)

# The Git repository from which to clone the source code
OPENHD_SITE = https://github.com/OpenHD/OpenHD.git
OPENHD_SITE_METHOD = git
OPENHD_GIT_SUBMODULES = YES

# Immutable application source; the manifest pins this Buildroot revision.
OPENHD_VERSION = eba46fe47961875aa9aba8f25b685c256c3643b3

# Enable Git submodules if the project requires them
OPENHD_GIT_SUBMODULES = YES

# Subdirectory inside the Git repo, if needed (if OpenHD is not in the root)
OPENHD_SUBDIR = OpenHD

# Install to the target system
OPENHD_INSTALL_TARGET = YES

# List of dependencies that must be built before OpenHD
OPENHD_DEPENDENCIES = poco libsodium gstreamer1 gst1-plugins-base libpcap libusb librtlsdr rockchip-mpp host-pkgconf

# Additional configuration options for the CMake build
OPENHD_CONF_OPTS = \
    -DENABLE_USB_CAMERAS=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DOPENHD_ENABLE_DEVOURER=ON \
    -DDEVOURER_JAGUAR3_8822E=ON \
    -DCMAKE_EXE_LINKER_FLAGS="-lstdc++fs"

# Install init.d services to target
define OPENHD_INSTALL_TARGET_CMDS
    $(info OpenHD Build Directory: $(OPENHD_BUILDDIR))
    $(INSTALL) -D -m 0755 $(OPENHD_BUILDDIR)/openhd $(TARGET_DIR)/usr/bin/openhd
    $(INSTALL) -d $(TARGET_DIR)/etc/init.d
    cp -r $(OPENHD_PKGDIR)/start.sh  $(TARGET_DIR)/etc/init.d/S99openhd
    chmod +x $(TARGET_DIR)/etc/init.d/*
endef

# Use Buildroot's CMake package infrastructure to handle the build
$(eval $(cmake-package))
