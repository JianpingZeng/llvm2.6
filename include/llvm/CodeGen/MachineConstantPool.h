//===-- CodeGen/MachineConstantPool.h - Abstract Constant Pool --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
/// @file
/// This file declares the MachineConstantPool class which is an abstract
/// constant pool to keep track of constants referenced by a function.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_MACHINECONSTANTPOOL_H
#define LLVM_CODEGEN_MACHINECONSTANTPOOL_H

#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <climits>
#include <vector>

namespace llvm {

class Constant;
class FoldingSetNodeID;
class TargetData;
class TargetMachine;
class Type;
class MachineConstantPool;
class raw_ostream;

/// Abstract base class for all machine specific constantpool value subclasses.
///
class MachineConstantPoolValue {
  const Type *Ty;

public:
  explicit MachineConstantPoolValue(const Type *ty) : Ty(ty) {}
  virtual ~MachineConstantPoolValue() {}

  /// getType - get type of this MachineConstantPoolValue.
  ///
  const Type *getType() const { return Ty; }

  
  /// getRelocationInfo - This method classifies the entry according to
  /// whether or not it may generate a relocation entry.  This must be
  /// conservative, so if it might codegen to a relocatable entry, it should say
  /// so.  The return values are the same as Constant::getRelocationInfo().
  virtual unsigned getRelocationInfo() const = 0;
  
  virtual int getExistingMachineCPValue(MachineConstantPool *CP,
                                        unsigned Alignment) = 0;

  virtual void AddSelectionDAGCSEId(FoldingSetNodeID &ID) = 0;

  /// print - Implement operator<<
  virtual void print(raw_ostream &O) const = 0;
};

inline raw_ostream &operator<<(raw_ostream &OS,
                               const MachineConstantPoolValue &V) {
  V.print(OS);
  return OS;
}
  

/// This class is a data container for one entry in a MachineConstantPool.
/// It contains a pointer to the value and an offset from the start of
/// the constant pool.
/// @brief An entry in a MachineConstantPool
class MachineConstantPoolEntry {
public:
  /// The constant itself.
  union {
    Constant *ConstVal;
    MachineConstantPoolValue *MachineCPVal;
  } Val;

  /// The required alignment for this entry. The top bit is set when Val is
  /// a MachineConstantPoolValue.
  unsigned Alignment;

  MachineConstantPoolEntry(Constant *V, unsigned A)
    : Alignment(A) {
    Val.ConstVal = V;
  }
  MachineConstantPoolEntry(MachineConstantPoolValue *V, unsigned A)
    : Alignment(A) {
    Val.MachineCPVal = V; 
    Alignment |= 1 << (sizeof(unsigned)*CHAR_BIT-1);
  }

  bool isMachineConstantPoolEntry() const {
    return (int)Alignment < 0;
  }

  int getAlignment() const { 
    return Alignment & ~(1 << (sizeof(unsigned)*CHAR_BIT-1));
  }

  const Type *getType() const;
  
  /// getRelocationInfo - This method classifies the entry according to
  /// whether or not it may generate a relocation entry.  This must be
  /// conservative, so if it might codegen to a relocatable entry, it should say
  /// so.  The return values are:
  /// 
  ///  0: This constant pool entry is guaranteed to never have a relocation
  ///     applied to it (because it holds a simple constant like '4').
  ///  1: This entry has relocations, but the entries are guaranteed to be
  ///     resolvable by the static linker, so the dynamic linker will never see
  ///     them.
  ///  2: This entry may have arbitrary relocations. 
  unsigned getRelocationInfo() const;
};
  
/// The MachineConstantPool class keeps track of constants referenced by a
/// function which must be spilled to memory.  This is used for constants which
/// are unable to be used directly as operands to instructions, which typically
/// include floating point and large integer constants.
///
/// Instructions reference the address of these constant pool constants through
/// the use of MO_ConstantPoolIndex values.  When emitting assembly or machine
/// code, these virtual address references are converted to refer to the
/// address of the function constant pool values.
/// @brief The machine constant pool.
class MachineConstantPool {
  const TargetData *TD;   ///< The machine's TargetData.
  unsigned PoolAlignment; ///< The alignment for the pool.
  std::vector<MachineConstantPoolEntry> Constants; ///< The pool of constants.
public:
  /// @brief The only constructor.
  explicit MachineConstantPool(const TargetData *td)
    : TD(td), PoolAlignment(1) {}
  ~MachineConstantPool();
    
  /// getConstantPoolAlignment - Return the the alignment required by
  /// the whole constant pool, of which the first element must be aligned.
  unsigned getConstantPoolAlignment() const { return PoolAlignment; }
  
  /// getConstantPoolIndex - Create a new entry in the constant pool or return
  /// an existing one.  User must specify the minimum required alignment for
  /// the object.
  unsigned getConstantPoolIndex(Constant *C, unsigned Alignment);
  unsigned getConstantPoolIndex(MachineConstantPoolValue *V,unsigned Alignment);
  
  /// isEmpty - Return true if this constant pool contains no constants.
  bool isEmpty() const { return Constants.empty(); }

  const std::vector<MachineConstantPoolEntry> &getConstants() const {
    return Constants;
  }

  /// print - Used by the MachineFunction printer to print information about
  /// constant pool objects.  Implemented in MachineFunction.cpp
  ///
  void print(raw_ostream &OS) const;
  void print(std::ostream &O) const {
    raw_os_ostream OS(O);
    print(OS);
  }
  /// dump - Call print(cerr) to be called from the debugger.
  void dump() const;
};

} // End llvm namespace

#endif
