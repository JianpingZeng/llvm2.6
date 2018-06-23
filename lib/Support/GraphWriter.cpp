//===-- GraphWriter.cpp - Implements GraphWriter support routines ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements misc. GraphWriter support routines.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/Streams.h"
#include "llvm/System/Path.h"
#include "llvm/System/Program.h"
#include "llvm/Config/config.h"
using namespace llvm;

void llvm::DisplayGraph(const sys::Path &Filename, bool wait,
                        GraphProgram::Name program) {
  std::string ErrMsg;

#define XDOT_PATH   "/usr/bin/xdot"
#ifdef __APPLE__
    // it's not perfect to display dot graph by xdot on MacOSX platform.
    // So we firstly use dot tool to generate pdf from original dot file, then
    // show it by 'open' program.
#undef XDOT_PATH

  sys::Path prog("/usr/bin/dot");
  std::vector<const char*> args;
  args.push_back(prog.c_str());
  args.push_back("-Tps");
  args.push_back("-Nfontname=Courier");
  args.push_back("-Gsize=7.5,10");
  args.push_back(Filename.c_str());
  args.push_back("-o");
  args.push_back(PSFilename.c_str());
  args.push_back(0);

  cerr << "Running '" << prog << "' program... \n";

  if (sys::Program::ExecuteAndWait(prog, &args[0],0,0,0,0,&ErrMsg)) {
     cerr << "Error viewing graph " << Filename << ": '" << ErrMsg << "\n";
  } else {
    cerr << " done. \n";

    sys::Path gv("/usr/bin/open");
    args.clear();
    args.push_back(gv.c_str());
    args.push_back(PSFilename.c_str());
    args.push_back(0);

    ErrMsg.clear();
    if (wait) {
       if (sys::Program::ExecuteAndWait(gv, &args[0],0,0,0,0,&ErrMsg)) {
          cerr << "Error viewing graph: " << ErrMsg << "\n";
       }
       Filename.eraseFromDisk();
       PSFilename.eraseFromDisk();
    }
    else {
       sys::Program::ExecuteNoWait(gv, &args[0],0,0,0,&ErrMsg);
       cerr << "Remember to erase graph files: " << Filename << " " << PSFilename << "\n";
    }
  }

#endif

#ifdef XDOT_PATH
    sys::Path xdot(XDOT_PATH);
    std::vector<const char*> args;
    args.push_back(XDOT_PATH);
    args.push_back(Filename.c_str());
    args.push_back(0);

    cerr << "Running 'XDOT' program... \n";
    if (sys::Program::ExecuteAndWait(xdot, &args[0],0,0,0,0,&ErrMsg)) {
        cerr << "Error viewing graph " << Filename << ": " << ErrMsg << "\n";
    }
    else {
        Filename.eraseFromDisk();
    }
#endif

#if HAVE_GRAPHVIZ
  sys::Path Graphviz(LLVM_PATH_GRAPHVIZ);

  std::vector<const char*> args;
  args.push_back(Graphviz.c_str());
  args.push_back(Filename.c_str());
  args.push_back(0);
  
  cerr << "Running 'Graphviz' program... ";
  if (sys::Program::ExecuteAndWait(Graphviz, &args[0],0,0,0,0,&ErrMsg)) {
     cerr << "Error viewing graph " << Filename << ": " << ErrMsg << "\n";
  }
  else {
     Filename.eraseFromDisk();
  }

#elif (HAVE_GV && (HAVE_DOT || HAVE_FDP || HAVE_NEATO || \
                   HAVE_TWOPI || HAVE_CIRCO))
  sys::Path PSFilename = Filename;
  PSFilename.appendSuffix("ps");

  sys::Path prog;

  // Set default grapher
#if HAVE_CIRCO
  prog = sys::Path(LLVM_PATH_CIRCO);
#endif
#if HAVE_TWOPI
  prog = sys::Path(LLVM_PATH_TWOPI);
#endif
#if HAVE_NEATO
  prog = sys::Path(LLVM_PATH_NEATO);
#endif
#if HAVE_FDP
  prog = sys::Path(LLVM_PATH_FDP);
#endif
#if HAVE_DOT
  prog = sys::Path(LLVM_PATH_DOT);
#endif

  // Find which program the user wants
#if HAVE_DOT
  if (program == GraphProgram::DOT) {
    prog = sys::Path(LLVM_PATH_DOT);
  }
#endif
#if (HAVE_FDP)
  if (program == GraphProgram::FDP) {
    prog = sys::Path(LLVM_PATH_FDP);
  }
#endif
#if (HAVE_NEATO)
  if (program == GraphProgram::NEATO) {
    prog = sys::Path(LLVM_PATH_NEATO);
    }
#endif
#if (HAVE_TWOPI)
  if (program == GraphProgram::TWOPI) {
    prog = sys::Path(LLVM_PATH_TWOPI);
  }
#endif
#if (HAVE_CIRCO)
  if (program == GraphProgram::CIRCO) {
    prog = sys::Path(LLVM_PATH_CIRCO);
  }
#endif

  std::vector<const char*> args;
  args.push_back(prog.c_str());
  args.push_back("-Tps");
  args.push_back("-Nfontname=Courier");
  args.push_back("-Gsize=7.5,10");
  args.push_back(Filename.c_str());
  args.push_back("-o");
  args.push_back(PSFilename.c_str());
  args.push_back(0);
  
  cerr << "Running '" << prog << "' program... ";

  if (sys::Program::ExecuteAndWait(prog, &args[0],0,0,0,0,&ErrMsg)) {
     cerr << "Error viewing graph " << Filename << ": '" << ErrMsg << "\n";
  } else {
    cerr << " done. \n";

    sys::Path gv(LLVM_PATH_GV);
    args.clear();
    args.push_back(gv.c_str());
    args.push_back(PSFilename.c_str());
    args.push_back("-spartan");
    args.push_back(0);
    
    ErrMsg.clear();
    if (wait) {
       if (sys::Program::ExecuteAndWait(gv, &args[0],0,0,0,0,&ErrMsg)) {
          cerr << "Error viewing graph: " << ErrMsg << "\n";
       }
       Filename.eraseFromDisk();
       PSFilename.eraseFromDisk();
    }
    else {
       sys::Program::ExecuteNoWait(gv, &args[0],0,0,0,&ErrMsg);
       cerr << "Remember to erase graph files: " << Filename << " " << PSFilename << "\n";
    }
  }
#elif HAVE_DOTTY
  sys::Path dotty(LLVM_PATH_DOTTY);

  std::vector<const char*> args;
  args.push_back(dotty.c_str());
  args.push_back(Filename.c_str());
  args.push_back(0);
  
  cerr << "Running 'dotty' program... ";
  if (sys::Program::ExecuteAndWait(dotty, &args[0],0,0,0,0,&ErrMsg)) {
     cerr << "Error viewing graph " << Filename << ": " << ErrMsg << "\n";
  } else {
#ifdef __MINGW32__ // Dotty spawns another app and doesn't wait until it returns
    return;
#endif
    Filename.eraseFromDisk();
  }
#endif
}
