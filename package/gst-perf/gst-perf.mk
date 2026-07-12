################################################################################
#
# gst-perf
#
################################################################################

GST_PERF_VERSION = 0.3.1
GST_PERF_SITE = $(call github,RidgeRun,gst-perf,v$(GST_PERF_VERSION))
GST_PERF_LICENSE = LGPL-2.0-or-later
GST_PERF_LICENSE_FILES = LICENSE
GST_PERF_AUTORECONF = YES
GST_PERF_DEPENDENCIES = host-pkgconf gstreamer1

$(eval $(autotools-package))
