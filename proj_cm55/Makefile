################################################################################
# \file Makefile
# \version 1.0
#
# \brief
# Makefile for configuring the Cortex-M55 core project
#
################################################################################
# \copyright
# (c) 2024-2026, Infineon Technologies AG, or an affiliate of Infineon
# Technologies AG.  SPDX-License-Identifier: Apache-2.0
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################

include ../common.mk

################################################################################
# Basic Configuration
################################################################################


# Name of application (used to derive name of final linked file).
#
# If APPNAME is edited, ensure to update or regenerate launch
# configurations for your IDE.
APPNAME=proj_cm55

# If set to "true" or "1", display full command-lines when building.
VERBOSE=


################################################################################
# Advanced Configuration
################################################################################

# Enable optional code that is ordinarily disabled by default.
#
# Available components depend on the specific targeted hardware and firmware
# in use. In general, if you have
#
#    COMPONENTS=foo bar
#
# ... then code in directories named COMPONENT_foo and COMPONENT_bar will be
# added to the build
#
COMPONENTS+=FREERTOS RTOS_AWARE APP_LOGGER

ifeq ($(CONFIG_DISPLAY), DASTEK_ROUND_1_43_INCH)
# Exclude Smartwatch GUI when running the benchmark demo
ifneq ($(DEMO_BENCHMARK), 1)
COMPONENTS += SMARTWATCH_ROUND_GUI
endif
DEFINES += MTB_DISPLAY_CO5300 MTB_CTP_FT6146
# Ignore unused 4.3-inch display and touch driver files from compilation
CY_IGNORE += $(SEARCH_display-dsi-waveshare-4-3-lcd)
CY_IGNORE += $(SEARCH_touch-ctp-ft5406)
# Ignore unused 1.43-inch touch driver file from compilation
CY_IGNORE += $(SEARCH_touch-ctp-ft3268)

else ifeq ($(CONFIG_DISPLAY), MICROTECH_ROUND_1_43_INCH)
ifneq ($(DEMO_BENCHMARK), 1)
COMPONENTS += SMARTWATCH_ROUND_GUI
endif
DEFINES += MTB_DISPLAY_CO5300 MTB_CTP_FT3268
# Ignore unused 4.3-inch display and touch driver files from compilation
CY_IGNORE += $(SEARCH_display-dsi-waveshare-4-3-lcd)
CY_IGNORE += $(SEARCH_touch-ctp-ft5406)
# Ignore unused 1.43-inch touch driver file from compilation
CY_IGNORE += $(SEARCH_touch-ctp-ft6146-m00)

else ifeq ($(CONFIG_DISPLAY), RECTANGLE_4_3_INCH)
ifneq ($(DEMO_BENCHMARK), 1)
COMPONENTS += SMARTWATCH_RECTANGLE_GUI
endif
DEFINES += W4P3INCH_DISP MTB_CTP_FT5406
# Ignore unused 1.43-inch display and touch driver files from compilation
CY_IGNORE += $(SEARCH_display-amoled-co5300)
CY_IGNORE += $(SEARCH_touch-ctp-ft6146-m00)
CY_IGNORE += $(SEARCH_touch-ctp-ft3268)
endif

# Like COMPONENTS, but disable optional code that was enabled by default.
DISABLE_COMPONENTS=

SEARCH+=../components

CORE=CM55
CORE_NAME=CM55_0

# By default the build system automatically looks in the Makefile's directory
# tree for source code and builds it. The SOURCES variable can be used to
# manually add source code to the build process from a location not searched
# by default, or otherwise not found by the build system.
SOURCES+=

# Like SOURCES, but for include directories. Value should be paths to
# directories (without a leading -I).
INCLUDES+=

# Add additional defines to the build process (without a leading -D).
# Enable frames-per-second (fps) measurement by adding "USE_PERFORMANCE_MONITOR"
# in the DEFINES.
DEFINES+=CY_RETARGET_IO_CONVERT_LF_TO_CRLF _BAREMETAL=0 APP_LOG_PRINT_EN
DEFINES+=LV_USE_DEMO_BENCHMARK=$(DEMO_BENCHMARK)
# DEFINES+=USE_PERFORMANCE_MONITOR

# Ignore unused LVGL middleware files from compilation
# --- Platform-specific draw backends (not needed for PSoC Edge) ---
CY_IGNORE += $(SEARCH_lvgl)/src/draw/nxp
CY_IGNORE += $(SEARCH_lvgl)/src/draw/renesas
CY_IGNORE += $(SEARCH_lvgl)/src/draw/dma2d
CY_IGNORE += $(SEARCH_lvgl)/src/draw/espressif
CY_IGNORE += $(SEARCH_lvgl)/src/draw/eve
CY_IGNORE += $(SEARCH_lvgl)/src/draw/nanovg
CY_IGNORE += $(SEARCH_lvgl)/src/draw/nema_gfx
CY_IGNORE += $(SEARCH_lvgl)/src/draw/opengles
CY_IGNORE += $(SEARCH_lvgl)/src/draw/sdl
# Unused converts (premultiply uses C fallback; blend uses helium)
CY_IGNORE += $(SEARCH_lvgl)/src/draw/convert/helium
CY_IGNORE += $(SEARCH_lvgl)/src/draw/convert/neon
# --- Architecture-specific SW blend (not for CM55) ---
# Helium is ENABLED: ignore other MVE acceleration
CY_IGNORE += $(SEARCH_lvgl)/src/draw/sw/blend/neon
CY_IGNORE += $(SEARCH_lvgl)/src/draw/sw/blend/arm2d
CY_IGNORE += $(SEARCH_lvgl)/src/draw/sw/blend/riscv_v
# --- Unused libraries ---
CY_IGNORE += $(SEARCH_lvgl)/src/libs/thorvg
CY_IGNORE += $(SEARCH_lvgl)/src/libs/FT800-FT813
CY_IGNORE += $(SEARCH_lvgl)/src/libs/frogfs
CY_IGNORE += $(SEARCH_lvgl)/src/libs/gltf
CY_IGNORE += $(SEARCH_lvgl)/src/libs/gstreamer
CY_IGNORE += $(SEARCH_lvgl)/src/libs/libwebp
CY_IGNORE += $(SEARCH_lvgl)/src/libs/nanovg
CY_IGNORE += $(SEARCH_lvgl)/src/libs/svg
CY_IGNORE += $(SEARCH_lvgl)/src/libs/vg_lite_driver
# --- Unused debugging/test/simulator ---
CY_IGNORE += $(SEARCH_lvgl)/src/debugging/test
CY_IGNORE += $(SEARCH_lvgl)/src/debugging/vg_lite_tvg
CY_IGNORE += $(SEARCH_lvgl)/src/debugging/monkey
# --- Unused test suites, examples, env support ---
CY_IGNORE += $(SEARCH_lvgl)/tests
CY_IGNORE += $(SEARCH_lvgl)/examples
CY_IGNORE += $(SEARCH_lvgl)/env_support
# --- Unused display/input/draw drivers (we use custom lv_port_disp/indev) ---
CY_IGNORE += $(SEARCH_lvgl)/src/drivers
# --- Unused libraries (ignoring saves significant ITCM) ---
CY_IGNORE += $(SEARCH_lvgl)/src/libs/barcode
CY_IGNORE += $(SEARCH_lvgl)/src/libs/ffmpeg
CY_IGNORE += $(SEARCH_lvgl)/src/libs/freetype
CY_IGNORE += $(SEARCH_lvgl)/src/libs/fsdrv
CY_IGNORE += $(SEARCH_lvgl)/src/libs/gif
CY_IGNORE += $(SEARCH_lvgl)/src/libs/libjpeg_turbo
CY_IGNORE += $(SEARCH_lvgl)/src/libs/libpng
CY_IGNORE += $(SEARCH_lvgl)/src/libs/lz4
CY_IGNORE += $(SEARCH_lvgl)/src/libs/rlottie
CY_IGNORE += $(SEARCH_lvgl)/src/libs/tiny_ttf
# --- Unused widgets ---
CY_IGNORE += $(SEARCH_lvgl)/src/widgets/property
CY_IGNORE += $(SEARCH_lvgl)/src/widgets/3dtexture
CY_IGNORE += $(SEARCH_lvgl)/src/widgets/gif
CY_IGNORE += $(SEARCH_lvgl)/src/widgets/lottie
CY_IGNORE += $(SEARCH_lvgl)/src/widgets/arclabel
# --- Unused stdlib variants ---
CY_IGNORE += $(SEARCH_lvgl)/src/stdlib/micropython
CY_IGNORE += $(SEARCH_lvgl)/src/stdlib/rtthread
CY_IGNORE += $(SEARCH_lvgl)/src/stdlib/uefi
# --- Unused font manager ---
CY_IGNORE += $(SEARCH_lvgl)/src/font/font_manager
# --- Unused demos ---
CY_IGNORE += $(SEARCH_lvgl)/demos/gltf
CY_IGNORE += $(SEARCH_lvgl)/demos/render
CY_IGNORE += $(SEARCH_lvgl)/demos/vector_graphic
CY_IGNORE += $(SEARCH_lvgl)/demos/keypad_encoder
CY_IGNORE += $(SEARCH_lvgl)/demos/stress
# --- Ignore mtb_shared originals; patched versions are in proj_cm55 ---
CY_IGNORE += $(SEARCH_lvgl)/src/draw/vg_lite/lv_draw_vg_lite.c
CY_IGNORE += $(SEARCH_lvgl)/src/draw/vg_lite/lv_draw_vg_lite_img.c
CY_IGNORE += $(SEARCH_lvgl)/src/draw/vg_lite/lv_vg_lite_utils.c
# --- Helium convert: local override skips helium premultiply (uses C fallback) ---
CY_IGNORE += $(SEARCH_lvgl)/src/draw/convert/lv_draw_buf_convert.c

# IAR assembler cannot process GCC-style C preprocessing in .S files
# The LVGL helium blend assembly is not compatible with IAR
ifeq ($(TOOLCHAIN), IAR)
CY_IGNORE += $(SEARCH_lvgl)/src/draw/sw/blend/helium
endif

# Select softfp or hardfp floating point. Default is softfp.
VFP_SELECT+=

# Additional / custom C compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CFLAGS+=

# Additional / custom C++ compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CXXFLAGS+=

# Additional / custom assembler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
ASFLAGS+=

# Additional / custom linker flags.
# Suppress GNU-stack warning emitted by Helium blend assembly
ifeq ($(TOOLCHAIN), GCC_ARM)
LDFLAGS+=-Wl,--no-warn-execstack
endif

# Additional / custom libraries to link in to the application.
LDLIBS+=

# Path to the linker script to use (if empty, use the default linker script).
LINKER_SCRIPT=

# Custom pre-build commands to run.
PREBUILD=

# Custom post-build commands to run.
POSTBUILD=


################################################################################
# Paths
################################################################################

# Relative path to the project directory (default is the Makefile's directory).
#
# This controls where automatic source code discovery looks for code.
CY_APP_PATH=

# Relative path to the shared repo location.
#
# All .mtb files have the format, <URI>#<COMMIT>#<LOCATION>. If the <LOCATION>
# field begins with $$ASSET_REPO$$, then the repo is deposited in the path
# specified by the CY_GETLIBS_SHARED_PATH variable. The default location is one
# directory level above the current app directory. This is used with
# CY_GETLIBS_SHARED_NAME variable, which specifies the directory name.
CY_GETLIBS_SHARED_PATH=../..

# Directory name of the shared repo location.
#
CY_GETLIBS_SHARED_NAME=mtb_shared

include $(CY_TOOLS_DIR)/make/start.mk
