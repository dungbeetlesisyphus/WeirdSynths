RACK_DIR ?= ../Rack-SDK
SLUG = WeirdSynths
VERSION = 2.0.0

FLAGS += -pthread
CXXFLAGS +=
LDFLAGS +=

ifdef ARCH_WIN
	LDFLAGS += -lws2_32
endif

SOURCES += src/plugin.cpp
SOURCES += src/Nerve.cpp
SOURCES += src/Skull.cpp
SOURCES += src/Mirror.cpp
SOURCES += src/Voice.cpp

DISTRIBUTABLES += res
DISTRIBUTABLES += plugin.json

include $(RACK_DIR)/plugin.mk
