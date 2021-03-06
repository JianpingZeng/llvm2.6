//===-- MSP430ISelDAGToDAG.cpp - A dag to dag inst selector for MSP430 ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the MSP430 target.
//
//===----------------------------------------------------------------------===//

#include "MSP430.h"
#include "MSP430ISelLowering.h"
#include "MSP430TargetMachine.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/Intrinsics.h"
#include "llvm/CallingConv.h"
#include "llvm/Constants.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

/// MSP430DAGToDAGISel - MSP430 specific code to select MSP430 machine
/// instructions for SelectionDAG operations.
///
namespace {
  class MSP430DAGToDAGISel : public SelectionDAGISel {
    MSP430TargetLowering &Lowering;
    const MSP430Subtarget &Subtarget;

  public:
    MSP430DAGToDAGISel(MSP430TargetMachine &TM, CodeGenOpt::Level OptLevel)
      : SelectionDAGISel(TM, OptLevel),
        Lowering(*TM.getTargetLowering()),
        Subtarget(*TM.getSubtargetImpl()) { }

    virtual void InstructionSelect();

    virtual const char *getPassName() const {
      return "MSP430 DAG->DAG Pattern Instruction Selection";
    }

    // Include the pieces autogenerated from the target description.
  #include "MSP430GenDAGISel.inc"

  private:
    SDNode *Select(SDValue Op);
    bool SelectAddr(SDValue Op, SDValue Addr, SDValue &Base, SDValue &Disp);

  #ifndef NDEBUG
    unsigned Indent;
  #endif
  };
}  // end anonymous namespace

/// createMSP430ISelDag - This pass converts a legalized DAG into a
/// MSP430-specific DAG, ready for instruction scheduling.
///
FunctionPass *llvm::createMSP430ISelDag(MSP430TargetMachine &TM,
                                        CodeGenOpt::Level OptLevel) {
  return new MSP430DAGToDAGISel(TM, OptLevel);
}

// FIXME: This is pretty dummy routine and needs to be rewritten in the future.
bool MSP430DAGToDAGISel::SelectAddr(SDValue Op, SDValue Addr,
                                    SDValue &Base, SDValue &Disp) {
  // Try to match frame address first.
  if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i16);
    Disp = CurDAG->getTargetConstant(0, MVT::i16);
    return true;
  }

  switch (Addr.getOpcode()) {
  case ISD::ADD:
   // Operand is a result from ADD with constant operand which fits into i16.
   if (ConstantSDNode *CN = dyn_cast<ConstantSDNode>(Addr.getOperand(1))) {
      uint64_t CVal = CN->getZExtValue();
      // Offset should fit into 16 bits.
      if (((CVal << 48) >> 48) == CVal) {
        SDValue N0 = Addr.getOperand(0);
        if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(N0))
          Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i16);
        else
          Base = N0;

        Disp = CurDAG->getTargetConstant(CVal, MVT::i16);
        return true;
      }
    }
    break;
  case MSP430ISD::Wrapper:
    SDValue N0 = Addr.getOperand(0);
    if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(N0)) {
      Base = CurDAG->getTargetGlobalAddress(G->getGlobal(),
                                            MVT::i16, G->getOffset());
      Disp = CurDAG->getTargetConstant(0, MVT::i16);
      return true;
    } else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(N0)) {
      Base = CurDAG->getTargetExternalSymbol(E->getSymbol(), MVT::i16);
      Disp = CurDAG->getTargetConstant(0, MVT::i16);
    }
    break;
  };

  Base = Addr;
  Disp = CurDAG->getTargetConstant(0, MVT::i16);

  return true;
}



/// InstructionSelect - This callback is invoked by
/// SelectionDAGISel when it has created a SelectionDAG for us to codegen.
void MSP430DAGToDAGISel::InstructionSelect() {
  DEBUG(BB->dump());

  // Codegen the basic block.
#ifndef NDEBUG
  DOUT(llvm::dbgs() << "===== Instruction selection begins:\n";
  Indent = 0;
#endif
  SelectRoot(*CurDAG);
#ifndef NDEBUG
  DOUT(llvm::dbgs() << "===== Instruction selection ends:\n";
#endif

  CurDAG->RemoveDeadNodes();
}

SDNode *MSP430DAGToDAGISel::Select(SDValue Op) {
  SDNode *Node = Op.getNode();
  DebugLoc dl = Op.getDebugLoc();

  // Dump information about the Node being selected
  #ifndef NDEBUG
  DOUT(llvm::dbgs() << std::string(Indent, ' ') << "Selecting: ";
  DEBUG(Node->dump(CurDAG));
  DOUT(llvm::dbgs() << "\n";
  Indent += 2;
  #endif

  // If we have a custom node, we already have selected!
  if (Node->isMachineOpcode()) {
    #ifndef NDEBUG
    DOUT(llvm::dbgs() << std::string(Indent-2, ' ') << "== ";
    DEBUG(Node->dump(CurDAG));
    DOUT(llvm::dbgs() << "\n";
    Indent -= 2;
    #endif
    return NULL;
  }

  // Few custom selection stuff.
  switch (Node->getOpcode()) {
  default: break;
  case ISD::FrameIndex: {
    assert(Op.getValueType() == MVT::i16);
    int FI = cast<FrameIndexSDNode>(Node)->getIndex();
    SDValue TFI = CurDAG->getTargetFrameIndex(FI, MVT::i16);
    if (Node->hasOneUse())
      return CurDAG->SelectNodeTo(Node, MSP430::ADD16ri, MVT::i16,
                                  TFI, CurDAG->getTargetConstant(0, MVT::i16));
    return CurDAG->getTargetNode(MSP430::ADD16ri, dl, MVT::i16,
                                 TFI, CurDAG->getTargetConstant(0, MVT::i16));
  }
  }

  // Select the default instruction
  SDNode *ResNode = SelectCode(Op);

  #ifndef NDEBUG
  DOUT(llvm::dbgs() << std::string(Indent-2, ' ') << "=> ";
  if (ResNode == NULL || ResNode == Op.getNode())
    DEBUG(Op.getNode()->dump(CurDAG));
  else
    DEBUG(ResNode->dump(CurDAG));
  DOUT(llvm::dbgs() << "\n";
  Indent -= 2;
  #endif

  return ResNode;
}
