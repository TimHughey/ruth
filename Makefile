#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := mcr_esp

include $(IDF_PATH)/make/project.mk

MCP_PRIV := ../mcp/priv
MCR_BIN_FILE := build/mcr_esp.bin

#.PHONY: deploy
deploy: all
	OS := $(shell uname)
	ifeq ($(OS), "Linux")
		$(INSTALL) --suffix=.prev $(MCR_BIN_FILE) $(MCP_PRIV)
	else ($(OS), "Darwin")
		$(INSTALL) -B .prev -b install $(MCR_BIN_FILE) $(MCP_PRIV)
	else
		$(INSTALL) $(MCR_BIN_FILE) $(MCP_PRIV)
	endif

	$(info Deployed $(MCR_BIN_FILE) to $(MCP_PRIV))
