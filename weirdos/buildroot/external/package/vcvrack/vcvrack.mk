################################################################################
#
# vcvrack — VCV Rack 2 open-source virtual modular synthesizer
#
################################################################################

VCVRACK_VERSION = 2.5.2
VCVRACK_SITE = https://github.com/VCVRack/Rack
VCVRACK_SITE_METHOD = git
VCVRACK_GIT_SUBMODULES = YES
VCVRACK_LICENSE = GPL-3.0-or-later
VCVRACK_LICENSE_FILES = LICENSE.md

VCVRACK_DEPENDENCIES = \
	host-pkgconf \
	glew \
	glfw \
	jansson \
	libcurl \
	libsamplerate \
	libsndfile \
	libzip \
	speexdsp \
	zstd \
	rtaudio \
	alsa-lib \
	jack2

# VCV Rack uses its own Makefile
define VCVRACK_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		STRIP="$(TARGET_STRIP)" \
		PKG_CONFIG="$(PKG_CONFIG_HOST_BINARY)" \
		ARCH=lin-aarch64 \
		all
endef

define VCVRACK_INSTALL_TARGET_CMDS
	# Install Rack binary
	$(INSTALL) -D -m 0755 $(@D)/Rack $(TARGET_DIR)/opt/vcvrack/Rack

	# Install resources
	cp -a $(@D)/res $(TARGET_DIR)/opt/vcvrack/
	cp -a $(@D)/template.vcv $(TARGET_DIR)/opt/vcvrack/

	# Create plugin directory
	mkdir -p $(TARGET_DIR)/home/weird/.config/Rack2/plugins

	# Symlink for PATH access
	ln -sf /opt/vcvrack/Rack $(TARGET_DIR)/usr/bin/vcvrack
endef

$(eval $(generic-package))
