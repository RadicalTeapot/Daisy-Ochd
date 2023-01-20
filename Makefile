# Project Name
TARGET = daisy-ochd

# Sources
CPP_SOURCES = main.cpp

CPP_STANDARD = -include displayDriver.h

# Library Locations
LIBDAISY_DIR ?= ../DaisyExamples/libDaisy
DAISYSP_DIR ?= ../DaisyExamples/DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
