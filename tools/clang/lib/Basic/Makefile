##===- clang/lib/Basic/Makefile ----------------------------*- Makefile -*-===##
# 
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
# 
##===----------------------------------------------------------------------===##
#
#  This implements the Basic library for the C-Language front-end.
#
##===----------------------------------------------------------------------===##

LEVEL = ../../../..
LIBRARYNAME := clangBasic
BUILD_ARCHIVE = 1
CXXFLAGS = -fno-rtti

# Don't install Clang libraries
NO_INSTALL = 1

CPPFLAGS += -I$(PROJ_SRC_DIR)/../../include -I$(PROJ_OBJ_DIR)/../../include

include $(LEVEL)/Makefile.common

