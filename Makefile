RACK_DIR ?= ../..

FLAGS += -Idep/include -std=c++17
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += $(wildcard LICENSE*) res

include $(RACK_DIR)/plugin.mk

CXXFLAGS := $(filter-out -std=c++11,$(CXXFLAGS))