##===- runtime/Makefile ------------------------------------*- Makefile -*-===##
# 
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
# 
##===----------------------------------------------------------------------===##

LEVEL = ..
include $(LEVEL)/Makefile.config

PARALLEL_DIRS  := libprofile

# Disable libprofile: a faulty libtool is generated by autoconf which breaks the
# build on Sparc
ifeq ($(ARCH), Sparc)
PARALLEL_DIRS := $(filter-out libprofile, $(PARALLEL_DIRS))
endif

include $(LEVEL)/Makefile.common

install::

