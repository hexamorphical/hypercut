#!/usr/bin/make -f
# Makefile for DISTRHO Plugins #
# ---------------------------- #
# Created by falkTX
#

# --------------------------------------------------------------
# Project name, used for binaries

NAME = HyperCut

# --------------------------------------------------------------
# Files to build

FILES_DSP = \
	LiveCut.cpp \
	lib/BBCutter.cpp \
	lib/BitCrusher.cpp \
	lib/Comb.cpp \
	lib/DelayLine.cpp \
	lib/FirstOrderLowpass.cpp
	

FILES_UI  = \
	LiveCutUI.cpp \
	../../dpf-widgets/opengl/DearImGui.cpp

# --------------------------------------------------------------
# Do some magic

include ../../dpf/Makefile.plugins.mk
BUILD_CXX_FLAGS += -I./lib
BUILD_CXX_FLAGS += -I../../dpf-widgets/generic
BUILD_CXX_FLAGS += -I../../dpf-widgets/opengl
# 32-bit draw indices: the UI draws more than 65k vertices per frame when everything is animated
BUILD_CXX_FLAGS += -D'ImDrawIdx=unsigned int'

# --------------------------------------------------------------
# Enable all possible plugin types

TARGETS += jack
TARGETS += clap
TARGETS += lv2   # monolithic: required by DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
TARGETS += vst2
TARGETS += vst3

all: $(TARGETS)

# --------------------------------------------------------------
