//===-- TypeSymbolTable.cpp - Implement the TypeSymbolTable class ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the TypeSymbolTable class for the VMCore library.
//
//===----------------------------------------------------------------------===//

#include "llvm/TypeSymbolTable.h"
#include "llvm/DerivedTypes.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Streams.h"
#include "llvm/System/RWMutex.h"
#include "llvm/System/Threading.h"
#include <algorithm>
using namespace llvm;

#define DEBUG_SYMBOL_TABLE 0
#define DEBUG_ABSTYPE 0

static ManagedStatic<sys::SmartRWMutex<true> > TypeSymbolTableLock;

TypeSymbolTable::~TypeSymbolTable() {
  // Drop all abstract type references in the type plane...
  for (iterator TI = tmap.begin(), TE = tmap.end(); TI != TE; ++TI) {
    if (TI->second->isAbstract())   // If abstract, drop the reference...
      cast<DerivedType>(TI->second)->removeAbstractTypeUser(this);
  }
}

std::string TypeSymbolTable::getUniqueName(const StringRef &BaseName) const {
  std::string TryName = BaseName;
  
  sys::SmartScopedReader<true> Reader(*TypeSymbolTableLock);
  
  const_iterator End = tmap.end();

  // See if the name exists
  while (tmap.find(TryName) != End)            // Loop until we find a free
    TryName = BaseName.str() + utostr(++LastUnique); // name in the symbol table
  return TryName;
}

// lookup a type by name - returns null on failure
Type* TypeSymbolTable::lookup(const StringRef &Name) const {
  sys::SmartScopedReader<true> Reader(*TypeSymbolTableLock);
  
  const_iterator TI = tmap.find(Name);
  Type* result = 0;
  if (TI != tmap.end())
    result = const_cast<Type*>(TI->second);
  return result;
}

TypeSymbolTable::iterator TypeSymbolTable::find(const StringRef &Name) {
  sys::SmartScopedReader<true> Reader(*TypeSymbolTableLock);  
  return tmap.find(Name);
}

TypeSymbolTable::const_iterator
TypeSymbolTable::find(const StringRef &Name) const {
  sys::SmartScopedReader<true> Reader(*TypeSymbolTableLock);  
  return tmap.find(Name);
}

// remove - Remove a type from the symbol table...
Type* TypeSymbolTable::remove(iterator Entry) {
  TypeSymbolTableLock->writer_acquire();
  
  assert(Entry != tmap.end() && "Invalid entry to remove!");
  const Type* Result = Entry->second;

#if DEBUG_SYMBOL_TABLE
  dump();
  cerr << " Removing Value: " << Result->getName() << "\n";
#endif

  tmap.erase(Entry);
  
  TypeSymbolTableLock->writer_release();

  // If we are removing an abstract type, remove the symbol table from it's use
  // list...
  if (Result->isAbstract()) {
#if DEBUG_ABSTYPE
    cerr << "Removing abstract type from symtab"
         << Result->getDescription()
         << "\n";
#endif
    cast<DerivedType>(Result)->removeAbstractTypeUser(this);
  }

  return const_cast<Type*>(Result);
}


// insert - Insert a type into the symbol table with the specified name...
void TypeSymbolTable::insert(const StringRef &Name, const Type* T) {
  assert(T && "Can't insert null type into symbol table!");

  TypeSymbolTableLock->writer_acquire();

  if (tmap.insert(std::make_pair(Name, T)).second) {
    // Type inserted fine with no conflict.
    
#if DEBUG_SYMBOL_TABLE
    dump();
    cerr << " Inserted type: " << Name << ": " << T->getDescription() << "\n";
#endif
  } else {
    // If there is a name conflict...
    
    // Check to see if there is a naming conflict.  If so, rename this type!
    std::string UniqueName = Name;
    if (lookup(Name))
      UniqueName = getUniqueName(Name);
    
#if DEBUG_SYMBOL_TABLE
    dump();
    cerr << " Inserting type: " << UniqueName << ": "
        << T->getDescription() << "\n";
#endif

    // Insert the tmap entry
    tmap.insert(make_pair(UniqueName, T));
  }
  
  TypeSymbolTableLock->writer_release();

  // If we are adding an abstract type, add the symbol table to it's use list.
  if (T->isAbstract()) {
    cast<DerivedType>(T)->addAbstractTypeUser(this);
#if DEBUG_ABSTYPE
    cerr << "Added abstract type to ST: " << T->getDescription() << "\n";
#endif
  }
}

// This function is called when one of the types in the type plane are refined
void TypeSymbolTable::refineAbstractType(const DerivedType *OldType,
                                         const Type *NewType) {
  sys::SmartScopedReader<true> Reader(*TypeSymbolTableLock);
  
  // Loop over all of the types in the symbol table, replacing any references
  // to OldType with references to NewType.  Note that there may be multiple
  // occurrences, and although we only need to remove one at a time, it's
  // faster to remove them all in one pass.
  //
  for (iterator I = begin(), E = end(); I != E; ++I) {
    if (I->second == (Type*)OldType) {  // FIXME when Types aren't const.
#if DEBUG_ABSTYPE
      cerr << "Removing type " << OldType->getDescription() << "\n";
#endif
      OldType->removeAbstractTypeUser(this);

      I->second = (Type*)NewType;  // TODO FIXME when types aren't const
      if (NewType->isAbstract()) {
#if DEBUG_ABSTYPE
        cerr << "Added type " << NewType->getDescription() << "\n";
#endif
        cast<DerivedType>(NewType)->addAbstractTypeUser(this);
      }
    }
  }
}


// Handle situation where type becomes Concreate from Abstract
void TypeSymbolTable::typeBecameConcrete(const DerivedType *AbsTy) {
  // Loop over all of the types in the symbol table, dropping any abstract
  // type user entries for AbsTy which occur because there are names for the
  // type.
  sys::SmartScopedReader<true> Reader(*TypeSymbolTableLock);
  for (iterator TI = begin(), TE = end(); TI != TE; ++TI)
    if (TI->second == const_cast<Type*>(static_cast<const Type*>(AbsTy)))
      AbsTy->removeAbstractTypeUser(this);
}

static void DumpTypes(const std::pair<const std::string, const Type*>& T ) {
  cerr << "  '" << T.first << "' = ";
  T.second->dump();
  cerr << "\n";
}

void TypeSymbolTable::dump() const {
  cerr << "TypeSymbolPlane: ";
  sys::SmartScopedReader<true> Reader(*TypeSymbolTableLock);
  for_each(tmap.begin(), tmap.end(), DumpTypes);
}

// vim: sw=2 ai
