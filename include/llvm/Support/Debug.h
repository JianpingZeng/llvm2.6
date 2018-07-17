//===- llvm/Support/Debug.h - Easy way to add debug output ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements a handy way of adding debugging information to your
// code, without it being enabled all of the time, and without having to add
// command line options to enable it.
//
// In particular, just wrap your code with the DEBUG() macro, and it will be
// enabled automatically if you specify '-debug' on the command-line.
// Alternatively, you can also use the SET_DEBUG_TYPE("foo") macro to specify
// that your debug code belongs to class "foo".  Then, on the command line, you
// can specify '-debug-only=foo' to enable JUST the debug information for the
// foo class.
//
// When compiling without assertions, the -debug-* options and all code in
// DEBUG() statements disappears, so it does not effect the runtime of the code.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_DEBUG_H
#define LLVM_SUPPORT_DEBUG_H

#include "llvm/Support/raw_ostream.h"
#include <iostream>
namespace llvm {

class raw_ostream;

// DebugFlag - This boolean is set to true if the '-debug' command line option
// is specified.  This should probably not be referenced directly, instead, use
// the DEBUG macro below.
//
extern bool DebugFlag;

// isCurrentDebugType - Return true if the specified string is the debug type
// specified on the command line, or if none was specified on the command line
// with the -debug-only=X option.
//
bool isCurrentDebugType(const char *Type);

// DEBUG_WITH_TYPE macro - This macro should be used by passes to emit debug
// information.  In the '-debug' option is specified on the commandline, and if
// this is a debug build, then the code specified as the option to the macro
// will be executed.  Otherwise it will not be.  Example:
//
// DEBUG_WITH_TYPE("bitset", errs() << "Bitset contains: " << Bitset << "\n");
//
// This will emit the debug information if -debug is present, and -debug-only is
// not specified, or is specified as "bitset".

#ifdef NDEBUG
#define DEBUG_WITH_TYPE(TYPE, X) do { } while (0)
#else
#define DEBUG_WITH_TYPE(X)                                        \
  do { if (DebugFlag) { X; } } while (0)
#endif

// DEBUG macro - This macro should be used by passes to emit debug information.
// In the '-debug' option is specified on the commandline, and if this is a
// debug build, then the code specified as the option to the macro will be
// executed.  Otherwise it will not be.  Example:
//
// DEBUG(errs() << "Bitset contains: " << Bitset << "\n");
//

#ifndef DEBUG_TYPE
#define DEBUG_TYPE ""
#endif

#define DEBUG(X) DEBUG_WITH_TYPE(X)

/// getNullOutputStream - Return a null string that does not output
/// anything.  This hides the static variable from other modules.
///
//OStream &getNullOutputStream();

/// getErrorOutputStream - Returns the error output stream (std::cerr). This
/// places the std::c* I/O streams into one .cpp file and relieves the whole
/// program from having to have hundreds of static c'tor/d'tors for them.
///
//OStream &getErrorOutputStream(const char *DebugType);

/// dbgs() - This returns a reference to a raw_ostream for debugging
/// messages.  If debugging is disabled it returns errs().  Use it
/// like: dbgs() << "foo" << "bar";
raw_ostream &dbgs();

#define DOUT(X)    DEBUG(X)

} // End llvm namespace

#endif
