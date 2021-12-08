BIN?=bin
SIGNALS_PATH?=signals
SRC?=$(SIGNALS_PATH)/src
EXTRA?=$(SIGNALS_PATH)/extra

SIGNALS_HAS_X11?=0
SIGNALS_HAS_APPS?=0

#------------------------------------------------------------------------------

EXE_OTHER_SRCS+=$(shell find src/ -path "*/unittests/*.cpp" | sort)

include $(SIGNALS_PATH)/Makefile

#------------------------------------------------------------------------------

ProjectName:=src/
include $(ProjectName)/project.mk

#------------------------------------------------------------------------------

targets: $(TARGETS)
