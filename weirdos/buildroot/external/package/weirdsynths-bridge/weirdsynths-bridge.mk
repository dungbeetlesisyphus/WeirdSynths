################################################################################
#
# weirdsynths-bridge — Face tracking bridge (Python + MediaPipe)
#
################################################################################

WEIRDSYNTHS_BRIDGE_VERSION = 2.0.0
WEIRDSYNTHS_BRIDGE_SITE = https://github.com/dungbeetlesisyphus/WeirdSynths
WEIRDSYNTHS_BRIDGE_SITE_METHOD = git
WEIRDSYNTHS_BRIDGE_LICENSE = GPL-3.0-or-later
WEIRDSYNTHS_BRIDGE_DEPENDENCIES = python3 python-numpy python-opencv4

# MediaPipe must be installed via pip at post-build time
# since there's no Buildroot package for it yet

define WEIRDSYNTHS_BRIDGE_INSTALL_TARGET_CMDS
	# Install bridge scripts
	$(INSTALL) -d $(TARGET_DIR)/opt/weirdsynths-bridge
	cp -a $(@D)/bridge/* $(TARGET_DIR)/opt/weirdsynths-bridge/

	# Install pip requirements for MediaPipe
	$(INSTALL) -D -m 0644 $(@D)/bridge/requirements.txt \
		$(TARGET_DIR)/opt/weirdsynths-bridge/requirements.txt

	# Create launcher script
	$(INSTALL) -D -m 0755 /dev/stdin $(TARGET_DIR)/usr/bin/weirdsynths-bridge <<'LAUNCHER'
#!/bin/bash
# WeirdSynths Face Tracking Bridge launcher
cd /opt/weirdsynths-bridge
exec python3 bridge.py "$$@"
LAUNCHER
endef

$(eval $(generic-package))
