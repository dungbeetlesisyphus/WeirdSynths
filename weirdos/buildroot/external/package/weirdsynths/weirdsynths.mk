################################################################################
#
# weirdsynths — Face tracking VCV Rack plugin
#
################################################################################

WEIRDSYNTHS_VERSION = 2.0.0
WEIRDSYNTHS_SITE = https://github.com/dungbeetlesisyphus/WeirdSynths
WEIRDSYNTHS_SITE_METHOD = git
WEIRDSYNTHS_LICENSE = GPL-3.0-or-later
WEIRDSYNTHS_LICENSE_FILES = LICENSE
WEIRDSYNTHS_DEPENDENCIES = vcvrack

define WEIRDSYNTHS_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		STRIP="$(TARGET_STRIP)" \
		RACK_DIR="/opt/vcvrack" \
		ARCH=lin-aarch64
endef

define WEIRDSYNTHS_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/home/weird/.config/Rack2/plugins/WeirdSynths
	cp $(@D)/plugin.so $(TARGET_DIR)/home/weird/.config/Rack2/plugins/WeirdSynths/
	cp $(@D)/plugin.json $(TARGET_DIR)/home/weird/.config/Rack2/plugins/WeirdSynths/
	cp -a $(@D)/res $(TARGET_DIR)/home/weird/.config/Rack2/plugins/WeirdSynths/
endef

$(eval $(generic-package))
