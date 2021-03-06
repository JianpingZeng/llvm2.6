//=====-- PIC16TargetAsmInfo.h - PIC16 asm properties ---------*- C++ -*--====//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source 
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the PIC16TargetAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef PIC16TARGETASMINFO_H
#define PIC16TARGETASMINFO_H

#include "llvm/Target/TargetAsmInfo.h"

namespace llvm {
  class Target;
  class StringRef;

  class PIC16TargetAsmInfo : public TargetAsmInfo {
    const char *RomData8bitsDirective;
    const char *RomData16bitsDirective;
    const char *RomData32bitsDirective;
  public:    
    PIC16TargetAsmInfo(const Target &T, const StringRef &TT);
    
    virtual const char *getDataASDirective(unsigned size, unsigned AS) const;
  };

} // namespace llvm

#endif
