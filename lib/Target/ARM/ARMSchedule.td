//===- ARMSchedule.td - ARM Scheduling Definitions ---------*- tablegen -*-===//
// 
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Functional units across ARM processors
//
def FU_Issue   : FuncUnit; // issue
def FU_Pipe0   : FuncUnit; // pipeline 0
def FU_Pipe1   : FuncUnit; // pipeline 1
def FU_LdSt0   : FuncUnit; // pipeline 0 load/store
def FU_LdSt1   : FuncUnit; // pipeline 1 load/store

//===----------------------------------------------------------------------===//
// Instruction Itinerary classes used for ARM
//
def IIC_iALUx      : InstrItinClass;
def IIC_iALUi      : InstrItinClass;
def IIC_iALUr      : InstrItinClass;
def IIC_iALUsi     : InstrItinClass;
def IIC_iALUsr     : InstrItinClass;
def IIC_iUNAr      : InstrItinClass;
def IIC_iUNAsi     : InstrItinClass;
def IIC_iUNAsr     : InstrItinClass;
def IIC_iCMPi      : InstrItinClass;
def IIC_iCMPr      : InstrItinClass;
def IIC_iCMPsi     : InstrItinClass;
def IIC_iCMPsr     : InstrItinClass;
def IIC_iMOVi      : InstrItinClass;
def IIC_iMOVr      : InstrItinClass;
def IIC_iMOVsi     : InstrItinClass;
def IIC_iMOVsr     : InstrItinClass;
def IIC_iCMOVi     : InstrItinClass;
def IIC_iCMOVr     : InstrItinClass;
def IIC_iCMOVsi    : InstrItinClass;
def IIC_iCMOVsr    : InstrItinClass;
def IIC_iMUL16     : InstrItinClass;
def IIC_iMAC16     : InstrItinClass;
def IIC_iMUL32     : InstrItinClass;
def IIC_iMAC32     : InstrItinClass;
def IIC_iMUL64     : InstrItinClass;
def IIC_iMAC64     : InstrItinClass;
def IIC_iLoadi     : InstrItinClass;
def IIC_iLoadr     : InstrItinClass;
def IIC_iLoadsi    : InstrItinClass;
def IIC_iLoadiu    : InstrItinClass;
def IIC_iLoadru    : InstrItinClass;
def IIC_iLoadsiu   : InstrItinClass;
def IIC_iLoadm     : InstrItinClass;
def IIC_iStorei    : InstrItinClass;
def IIC_iStorer    : InstrItinClass;
def IIC_iStoresi   : InstrItinClass;
def IIC_iStoreiu   : InstrItinClass;
def IIC_iStoreru   : InstrItinClass;
def IIC_iStoresiu  : InstrItinClass;
def IIC_iStorem    : InstrItinClass;
def IIC_fpALU      : InstrItinClass;
def IIC_fpMPY      : InstrItinClass;
def IIC_fpLoad     : InstrItinClass;
def IIC_fpStore    : InstrItinClass;
def IIC_Br         : InstrItinClass;

//===----------------------------------------------------------------------===//
// Processor instruction itineraries.

def GenericItineraries : ProcessorItineraries<[
  InstrItinData<IIC_iALUx   , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iALUi   , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iALUr   , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iALUsi  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iALUsr  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iUNAr   , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iUNAsi  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iUNAsr  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iCMPi   , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iCMPr   , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iCMPsi  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iCMPsr  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iMOVi   , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iMOVr   , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iMOVsi  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iMOVsr  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iCMOVi  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iCMOVr  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iCMOVsi , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iCMOVsr , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iMUL16  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iMAC16  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iMUL32  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iMAC32  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iMUL64  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iMAC64  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iLoadi  , [InstrStage<1, [FU_Pipe0]>,
                               InstrStage<1, [FU_LdSt0]>]>,
  InstrItinData<IIC_iLoadr  , [InstrStage<1, [FU_Pipe0]>,
                               InstrStage<1, [FU_LdSt0]>]>,
  InstrItinData<IIC_iLoadsi , [InstrStage<1, [FU_Pipe0]>,
                               InstrStage<1, [FU_LdSt0]>]>,
  InstrItinData<IIC_iLoadiu , [InstrStage<1, [FU_Pipe0]>,
                               InstrStage<1, [FU_LdSt0]>]>,
  InstrItinData<IIC_iLoadru , [InstrStage<1, [FU_Pipe0]>,
                               InstrStage<1, [FU_LdSt0]>]>,
  InstrItinData<IIC_iLoadsiu, [InstrStage<1, [FU_Pipe0]>,
                               InstrStage<1, [FU_LdSt0]>]>,
  InstrItinData<IIC_iLoadm  , [InstrStage<2, [FU_Pipe0]>,
                               InstrStage<2, [FU_LdSt0]>]>,
  InstrItinData<IIC_iStorei  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iStorer  , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iStoresi , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iStoreiu , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iStoreru , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iStoresiu, [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_iStorem  , [InstrStage<2, [FU_Pipe0]>]>,
  InstrItinData<IIC_Br      , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_fpALU   , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_fpMPY   , [InstrStage<1, [FU_Pipe0]>]>,
  InstrItinData<IIC_fpLoad  , [InstrStage<1, [FU_Pipe0]>,
                               InstrStage<1, [FU_LdSt0]>]>,
  InstrItinData<IIC_fpStore , [InstrStage<1, [FU_Pipe0]>]>
]>;


include "ARMScheduleV6.td"
include "ARMScheduleV7.td"
