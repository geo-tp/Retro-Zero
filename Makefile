ifeq ($(origin CXX),default)
CXX := aarch64-linux-gnu-g++
endif
ifeq ($(origin CC),default)
CC := aarch64-linux-gnu-gcc
endif

BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/cp0-libretro-fb0

LVGL_DIR := vendor/lvgl

SRCS := $(shell find src -name '*.cpp' | sort)
LVGL_SRCS := $(shell find $(LVGL_DIR)/src -name '*.c' 2>/dev/null | sort)

OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)
OBJS += $(LVGL_SRCS:%.c=$(BUILD_DIR)/%.o)

DEPS := $(OBJS:.o=.d)

CPPFLAGS := \
	-Iinclude \
	-I. \
	-Isrc \
	-Ivendor \
	-I$(LVGL_DIR) \
	-DLV_CONF_INCLUDE_SIMPLE \
	-DCP0_HAVE_ALSA=1 \
	-DCP0_WITH_LVGL=1

CP0_WITH_EGL_FBDEV ?= 1
CPPFLAGS += -DCP0_WITH_EGL_FBDEV=$(CP0_WITH_EGL_FBDEV)

CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
CFLAGS ?= -O2 -Wall -Wextra
LDFLAGS ?=

LDLIBS := -ldl -lasound

ifeq ($(CP0_WITH_EGL_FBDEV),1)
LDLIBS += -lEGL -lGLESv2
endif

LDLIBS += -lpthread -lm

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)