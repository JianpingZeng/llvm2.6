//===------- SemaTemplateDeduction.cpp - Template Argument Deduction ------===/
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===/
//
//  This file implements C++ template argument deduction.
//
//===----------------------------------------------------------------------===/

#include "Sema.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/Parse/DeclSpec.h"
#include "llvm/Support/Compiler.h"

namespace clang {
  /// \brief Various flags that control template argument deduction.
  ///
  /// These flags can be bitwise-OR'd together.
  enum TemplateDeductionFlags {
    /// \brief No template argument deduction flags, which indicates the
    /// strictest results for template argument deduction (as used for, e.g.,
    /// matching class template partial specializations).
    TDF_None = 0,
    /// \brief Within template argument deduction from a function call, we are
    /// matching with a parameter type for which the original parameter was
    /// a reference.
    TDF_ParamWithReferenceType = 0x1,
    /// \brief Within template argument deduction from a function call, we
    /// are matching in a case where we ignore cv-qualifiers.
    TDF_IgnoreQualifiers = 0x02,
    /// \brief Within template argument deduction from a function call,
    /// we are matching in a case where we can perform template argument
    /// deduction from a template-id of a derived class of the argument type.
    TDF_DerivedClass = 0x04
  };
}

using namespace clang;

static Sema::TemplateDeductionResult
DeduceTemplateArguments(ASTContext &Context, 
                        TemplateParameterList *TemplateParams,
                        const TemplateArgument &Param,
                        const TemplateArgument &Arg,
                        Sema::TemplateDeductionInfo &Info,
                        llvm::SmallVectorImpl<TemplateArgument> &Deduced);

/// \brief If the given expression is of a form that permits the deduction
/// of a non-type template parameter, return the declaration of that
/// non-type template parameter.
static NonTypeTemplateParmDecl *getDeducedParameterFromExpr(Expr *E) {
  if (ImplicitCastExpr *IC = dyn_cast<ImplicitCastExpr>(E))
    E = IC->getSubExpr();
  
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E))
    return dyn_cast<NonTypeTemplateParmDecl>(DRE->getDecl());
  
  return 0;
}

/// \brief Deduce the value of the given non-type template parameter 
/// from the given constant.
static Sema::TemplateDeductionResult
DeduceNonTypeTemplateArgument(ASTContext &Context, 
                              NonTypeTemplateParmDecl *NTTP, 
                              llvm::APSInt Value,
                              Sema::TemplateDeductionInfo &Info,
                              llvm::SmallVectorImpl<TemplateArgument> &Deduced) {
  assert(NTTP->getDepth() == 0 && 
         "Cannot deduce non-type template argument with depth > 0");
  
  if (Deduced[NTTP->getIndex()].isNull()) {
    QualType T = NTTP->getType();
    
    // FIXME: Make sure we didn't overflow our data type!
    unsigned AllowedBits = Context.getTypeSize(T);
    if (Value.getBitWidth() != AllowedBits)
      Value.extOrTrunc(AllowedBits);
    Value.setIsSigned(T->isSignedIntegerType());

    Deduced[NTTP->getIndex()] = TemplateArgument(SourceLocation(), Value, T);
    return Sema::TDK_Success;
  }
  
  assert(Deduced[NTTP->getIndex()].getKind() == TemplateArgument::Integral);
  
  // If the template argument was previously deduced to a negative value, 
  // then our deduction fails.
  const llvm::APSInt *PrevValuePtr = Deduced[NTTP->getIndex()].getAsIntegral();
  if (PrevValuePtr->isNegative()) {
    Info.Param = NTTP;
    Info.FirstArg = Deduced[NTTP->getIndex()];
    Info.SecondArg = TemplateArgument(SourceLocation(), Value, NTTP->getType());
    return Sema::TDK_Inconsistent;
  }

  llvm::APSInt PrevValue = *PrevValuePtr;
  if (Value.getBitWidth() > PrevValue.getBitWidth())
    PrevValue.zext(Value.getBitWidth());
  else if (Value.getBitWidth() < PrevValue.getBitWidth())
    Value.zext(PrevValue.getBitWidth());

  if (Value != PrevValue) {
    Info.Param = NTTP;
    Info.FirstArg = Deduced[NTTP->getIndex()];
    Info.SecondArg = TemplateArgument(SourceLocation(), Value, NTTP->getType());
    return Sema::TDK_Inconsistent;
  }

  return Sema::TDK_Success;
}

/// \brief Deduce the value of the given non-type template parameter 
/// from the given type- or value-dependent expression.
///
/// \returns true if deduction succeeded, false otherwise.

static Sema::TemplateDeductionResult
DeduceNonTypeTemplateArgument(ASTContext &Context, 
                              NonTypeTemplateParmDecl *NTTP,
                              Expr *Value,
                              Sema::TemplateDeductionInfo &Info,
                           llvm::SmallVectorImpl<TemplateArgument> &Deduced) {
  assert(NTTP->getDepth() == 0 && 
         "Cannot deduce non-type template argument with depth > 0");
  assert((Value->isTypeDependent() || Value->isValueDependent()) &&
         "Expression template argument must be type- or value-dependent.");
  
  if (Deduced[NTTP->getIndex()].isNull()) {
    // FIXME: Clone the Value?
    Deduced[NTTP->getIndex()] = TemplateArgument(Value);
    return Sema::TDK_Success;
  }
  
  if (Deduced[NTTP->getIndex()].getKind() == TemplateArgument::Integral) {
    // Okay, we deduced a constant in one case and a dependent expression 
    // in another case. FIXME: Later, we will check that instantiating the 
    // dependent expression gives us the constant value.
    return Sema::TDK_Success;
  }
  
  // FIXME: Compare the expressions for equality!
  return Sema::TDK_Success;
}

static Sema::TemplateDeductionResult
DeduceTemplateArguments(ASTContext &Context,
                        TemplateName Param,
                        TemplateName Arg,
                        Sema::TemplateDeductionInfo &Info,
                        llvm::SmallVectorImpl<TemplateArgument> &Deduced) {
  // FIXME: Implement template argument deduction for template
  // template parameters.

  // FIXME: this routine does not have enough information to produce
  // good diagnostics.

  TemplateDecl *ParamDecl = Param.getAsTemplateDecl();
  TemplateDecl *ArgDecl = Arg.getAsTemplateDecl();
  
  if (!ParamDecl || !ArgDecl) {
    // FIXME: fill in Info.Param/Info.FirstArg
    return Sema::TDK_Inconsistent;
  }

  ParamDecl = cast<TemplateDecl>(ParamDecl->getCanonicalDecl());
  ArgDecl = cast<TemplateDecl>(ArgDecl->getCanonicalDecl());
  if (ParamDecl != ArgDecl) {
    // FIXME: fill in Info.Param/Info.FirstArg
    return Sema::TDK_Inconsistent;
  }

  return Sema::TDK_Success;
}

/// \brief Deduce the template arguments by comparing the template parameter 
/// type (which is a template-id) with the template argument type.
///
/// \param Context the AST context in which this deduction occurs.
///
/// \param TemplateParams the template parameters that we are deducing
///
/// \param Param the parameter type
///
/// \param Arg the argument type
///
/// \param Info information about the template argument deduction itself
///
/// \param Deduced the deduced template arguments
///
/// \returns the result of template argument deduction so far. Note that a
/// "success" result means that template argument deduction has not yet failed,
/// but it may still fail, later, for other reasons.
static Sema::TemplateDeductionResult
DeduceTemplateArguments(ASTContext &Context,
                        TemplateParameterList *TemplateParams,
                        const TemplateSpecializationType *Param,
                        QualType Arg,
                        Sema::TemplateDeductionInfo &Info,
                        llvm::SmallVectorImpl<TemplateArgument> &Deduced) {
  assert(Arg->isCanonical() && "Argument type must be canonical");
  
  // Check whether the template argument is a dependent template-id.
  // FIXME: This is untested code; it can be tested when we implement
  // partial ordering of class template partial specializations.
  if (const TemplateSpecializationType *SpecArg 
        = dyn_cast<TemplateSpecializationType>(Arg)) {
    // Perform template argument deduction for the template name.
    if (Sema::TemplateDeductionResult Result
          = DeduceTemplateArguments(Context,
                                    Param->getTemplateName(),
                                    SpecArg->getTemplateName(),
                                    Info, Deduced))
      return Result;
    
    unsigned NumArgs = Param->getNumArgs();
    
    // FIXME: When one of the template-names refers to a
    // declaration with default template arguments, do we need to
    // fill in those default template arguments here? Most likely,
    // the answer is "yes", but I don't see any references. This
    // issue may be resolved elsewhere, because we may want to
    // instantiate default template arguments when we actually write
    // the template-id.
    if (SpecArg->getNumArgs() != NumArgs)
      return Sema::TDK_NonDeducedMismatch;
    
    // Perform template argument deduction on each template
    // argument.
    for (unsigned I = 0; I != NumArgs; ++I)
      if (Sema::TemplateDeductionResult Result
            = DeduceTemplateArguments(Context, TemplateParams,
                                      Param->getArg(I),
                                      SpecArg->getArg(I),
                                      Info, Deduced))
        return Result;
    
    return Sema::TDK_Success;
  }
  
  // If the argument type is a class template specialization, we
  // perform template argument deduction using its template
  // arguments.
  const RecordType *RecordArg = dyn_cast<RecordType>(Arg);
  if (!RecordArg)
    return Sema::TDK_NonDeducedMismatch;
  
  ClassTemplateSpecializationDecl *SpecArg 
    = dyn_cast<ClassTemplateSpecializationDecl>(RecordArg->getDecl());
  if (!SpecArg)
    return Sema::TDK_NonDeducedMismatch;
  
  // Perform template argument deduction for the template name.
  if (Sema::TemplateDeductionResult Result
        = DeduceTemplateArguments(Context, 
                                  Param->getTemplateName(),
                               TemplateName(SpecArg->getSpecializedTemplate()),
                                  Info, Deduced))
    return Result;
    
  // FIXME: Can the # of arguments in the parameter and the argument
  // differ due to default arguments?
  unsigned NumArgs = Param->getNumArgs();
  const TemplateArgumentList &ArgArgs = SpecArg->getTemplateArgs();
  if (NumArgs != ArgArgs.size())
    return Sema::TDK_NonDeducedMismatch;
  
  for (unsigned I = 0; I != NumArgs; ++I)
    if (Sema::TemplateDeductionResult Result 
          = DeduceTemplateArguments(Context, TemplateParams,
                                    Param->getArg(I),
                                    ArgArgs.get(I),
                                    Info, Deduced))
      return Result;
    
  return Sema::TDK_Success;
}

/// \brief Returns a completely-unqualified array type, capturing the 
/// qualifiers in CVRQuals.
///
/// \param Context the AST context in which the array type was built.
///
/// \param T a canonical type that may be an array type.
///
/// \param CVRQuals will receive the set of const/volatile/restrict qualifiers
/// that were applied to the element type of the array.
///
/// \returns if \p T is an array type, the completely unqualified array type
/// that corresponds to T. Otherwise, returns T.
static QualType getUnqualifiedArrayType(ASTContext &Context, QualType T,
                                        unsigned &CVRQuals) {
  assert(T->isCanonical() && "Only operates on canonical types");
  if (!isa<ArrayType>(T)) {
    CVRQuals = T.getCVRQualifiers();
    return T.getUnqualifiedType();
  }
  
  if (const ConstantArrayType *CAT = dyn_cast<ConstantArrayType>(T)) {
    QualType Elt = getUnqualifiedArrayType(Context, CAT->getElementType(),
                                           CVRQuals);
    if (Elt == CAT->getElementType())
      return T;

    return Context.getConstantArrayType(Elt, CAT->getSize(), 
                                        CAT->getSizeModifier(), 0);
  }
  
  if (const IncompleteArrayType *IAT = dyn_cast<IncompleteArrayType>(T)) {
    QualType Elt = getUnqualifiedArrayType(Context, IAT->getElementType(),
                                           CVRQuals);
    if (Elt == IAT->getElementType())
      return T;
    
    return Context.getIncompleteArrayType(Elt, IAT->getSizeModifier(), 0);
  }
  
  const DependentSizedArrayType *DSAT = cast<DependentSizedArrayType>(T);
  QualType Elt = getUnqualifiedArrayType(Context, DSAT->getElementType(),
                                         CVRQuals);
  if (Elt == DSAT->getElementType())
    return T;
  
  return Context.getDependentSizedArrayType(Elt, DSAT->getSizeExpr()->Retain(),
                                            DSAT->getSizeModifier(), 0,
                                            SourceRange());
}

/// \brief Deduce the template arguments by comparing the parameter type and
/// the argument type (C++ [temp.deduct.type]).
///
/// \param Context the AST context in which this deduction occurs.
///
/// \param TemplateParams the template parameters that we are deducing
///
/// \param ParamIn the parameter type
///
/// \param ArgIn the argument type
///
/// \param Info information about the template argument deduction itself
///
/// \param Deduced the deduced template arguments
///
/// \param TDF bitwise OR of the TemplateDeductionFlags bits that describe
/// how template argument deduction is performed. 
///
/// \returns the result of template argument deduction so far. Note that a
/// "success" result means that template argument deduction has not yet failed,
/// but it may still fail, later, for other reasons.
static Sema::TemplateDeductionResult
DeduceTemplateArguments(ASTContext &Context, 
                        TemplateParameterList *TemplateParams,
                        QualType ParamIn, QualType ArgIn,
                        Sema::TemplateDeductionInfo &Info,
                        llvm::SmallVectorImpl<TemplateArgument> &Deduced,
                        unsigned TDF) {
  // We only want to look at the canonical types, since typedefs and
  // sugar are not part of template argument deduction.
  QualType Param = Context.getCanonicalType(ParamIn);
  QualType Arg = Context.getCanonicalType(ArgIn);

  // C++0x [temp.deduct.call]p4 bullet 1:
  //   - If the original P is a reference type, the deduced A (i.e., the type
  //     referred to by the reference) can be more cv-qualified than the 
  //     transformed A.
  if (TDF & TDF_ParamWithReferenceType) {
    unsigned ExtraQualsOnParam 
      = Param.getCVRQualifiers() & ~Arg.getCVRQualifiers();
    Param.setCVRQualifiers(Param.getCVRQualifiers() & ~ExtraQualsOnParam);
  }
  
  // If the parameter type is not dependent, there is nothing to deduce.
  if (!Param->isDependentType())
    return Sema::TDK_Success;

  // C++ [temp.deduct.type]p9:
  //   A template type argument T, a template template argument TT or a 
  //   template non-type argument i can be deduced if P and A have one of 
  //   the following forms:
  //
  //     T
  //     cv-list T
  if (const TemplateTypeParmType *TemplateTypeParm 
        = Param->getAsTemplateTypeParmType()) {
    unsigned Index = TemplateTypeParm->getIndex();
    bool RecanonicalizeArg = false;
    
    // If the argument type is an array type, move the qualifiers up to the
    // top level, so they can be matched with the qualifiers on the parameter.
    // FIXME: address spaces, ObjC GC qualifiers
    if (isa<ArrayType>(Arg)) {
      unsigned CVRQuals = 0;
      Arg = getUnqualifiedArrayType(Context, Arg, CVRQuals);
      if (CVRQuals) {
        Arg = Arg.getWithAdditionalQualifiers(CVRQuals);
        RecanonicalizeArg = true;
      }
    }
                                          
    // The argument type can not be less qualified than the parameter
    // type.
    if (Param.isMoreQualifiedThan(Arg) && !(TDF & TDF_IgnoreQualifiers)) {
      Info.Param = cast<TemplateTypeParmDecl>(TemplateParams->getParam(Index));
      Info.FirstArg = Deduced[Index];
      Info.SecondArg = TemplateArgument(SourceLocation(), Arg);
      return Sema::TDK_InconsistentQuals;
    }

    assert(TemplateTypeParm->getDepth() == 0 && "Can't deduce with depth > 0");
	  
    unsigned Quals = Arg.getCVRQualifiers() & ~Param.getCVRQualifiers();
    QualType DeducedType = Arg.getQualifiedType(Quals);
    if (RecanonicalizeArg)
      DeducedType = Context.getCanonicalType(DeducedType);
    
    if (Deduced[Index].isNull())
      Deduced[Index] = TemplateArgument(SourceLocation(), DeducedType);
    else {
      // C++ [temp.deduct.type]p2: 
      //   [...] If type deduction cannot be done for any P/A pair, or if for
      //   any pair the deduction leads to more than one possible set of 
      //   deduced values, or if different pairs yield different deduced 
      //   values, or if any template argument remains neither deduced nor 
      //   explicitly specified, template argument deduction fails.
      if (Deduced[Index].getAsType() != DeducedType) {
        Info.Param 
          = cast<TemplateTypeParmDecl>(TemplateParams->getParam(Index));
        Info.FirstArg = Deduced[Index];
        Info.SecondArg = TemplateArgument(SourceLocation(), Arg);
        return Sema::TDK_Inconsistent;
      }
    }
    return Sema::TDK_Success;
  }

  // Set up the template argument deduction information for a failure.
  Info.FirstArg = TemplateArgument(SourceLocation(), ParamIn);
  Info.SecondArg = TemplateArgument(SourceLocation(), ArgIn);

  // Check the cv-qualifiers on the parameter and argument types.
  if (!(TDF & TDF_IgnoreQualifiers)) {
    if (TDF & TDF_ParamWithReferenceType) {
      if (Param.isMoreQualifiedThan(Arg))
        return Sema::TDK_NonDeducedMismatch;
    } else {
      if (Param.getCVRQualifiers() != Arg.getCVRQualifiers())
        return Sema::TDK_NonDeducedMismatch;  
    }
  }

  switch (Param->getTypeClass()) {
    // No deduction possible for these types
    case Type::Builtin:
      return Sema::TDK_NonDeducedMismatch;
      
    //     T *
    case Type::Pointer: {
      const PointerType *PointerArg = Arg->getAs<PointerType>();
      if (!PointerArg)
        return Sema::TDK_NonDeducedMismatch;
      
      unsigned SubTDF = TDF & (TDF_IgnoreQualifiers | TDF_DerivedClass);
      return DeduceTemplateArguments(Context, TemplateParams,
                                   cast<PointerType>(Param)->getPointeeType(),
                                     PointerArg->getPointeeType(),
                                     Info, Deduced, SubTDF);
    }
      
    //     T &
    case Type::LValueReference: {
      const LValueReferenceType *ReferenceArg = Arg->getAs<LValueReferenceType>();
      if (!ReferenceArg)
        return Sema::TDK_NonDeducedMismatch;
      
      return DeduceTemplateArguments(Context, TemplateParams,
                           cast<LValueReferenceType>(Param)->getPointeeType(),
                                     ReferenceArg->getPointeeType(),
                                     Info, Deduced, 0);
    }

    //     T && [C++0x]
    case Type::RValueReference: {
      const RValueReferenceType *ReferenceArg = Arg->getAs<RValueReferenceType>();
      if (!ReferenceArg)
        return Sema::TDK_NonDeducedMismatch;
      
      return DeduceTemplateArguments(Context, TemplateParams,
                           cast<RValueReferenceType>(Param)->getPointeeType(),
                                     ReferenceArg->getPointeeType(),
                                     Info, Deduced, 0);
    }
      
    //     T [] (implied, but not stated explicitly)
    case Type::IncompleteArray: {
      const IncompleteArrayType *IncompleteArrayArg = 
        Context.getAsIncompleteArrayType(Arg);
      if (!IncompleteArrayArg)
        return Sema::TDK_NonDeducedMismatch;
      
      return DeduceTemplateArguments(Context, TemplateParams,
                     Context.getAsIncompleteArrayType(Param)->getElementType(),
                                     IncompleteArrayArg->getElementType(),
                                     Info, Deduced, 0);
    }

    //     T [integer-constant]
    case Type::ConstantArray: {
      const ConstantArrayType *ConstantArrayArg = 
        Context.getAsConstantArrayType(Arg);
      if (!ConstantArrayArg)
        return Sema::TDK_NonDeducedMismatch;
      
      const ConstantArrayType *ConstantArrayParm = 
        Context.getAsConstantArrayType(Param);
      if (ConstantArrayArg->getSize() != ConstantArrayParm->getSize())
        return Sema::TDK_NonDeducedMismatch;
      
      return DeduceTemplateArguments(Context, TemplateParams,
                                     ConstantArrayParm->getElementType(),
                                     ConstantArrayArg->getElementType(),
                                     Info, Deduced, 0);
    }

    //     type [i]
    case Type::DependentSizedArray: {
      const ArrayType *ArrayArg = dyn_cast<ArrayType>(Arg);
      if (!ArrayArg)
        return Sema::TDK_NonDeducedMismatch;
      
      // Check the element type of the arrays
      const DependentSizedArrayType *DependentArrayParm
        = cast<DependentSizedArrayType>(Param);
      if (Sema::TemplateDeductionResult Result
            = DeduceTemplateArguments(Context, TemplateParams,
                                      DependentArrayParm->getElementType(),
                                      ArrayArg->getElementType(),
                                      Info, Deduced, 0))
        return Result;
          
      // Determine the array bound is something we can deduce.
      NonTypeTemplateParmDecl *NTTP 
        = getDeducedParameterFromExpr(DependentArrayParm->getSizeExpr());
      if (!NTTP)
        return Sema::TDK_Success;
      
      // We can perform template argument deduction for the given non-type 
      // template parameter.
      assert(NTTP->getDepth() == 0 && 
             "Cannot deduce non-type template argument at depth > 0");
      if (const ConstantArrayType *ConstantArrayArg 
            = dyn_cast<ConstantArrayType>(ArrayArg)) {
        llvm::APSInt Size(ConstantArrayArg->getSize());
        return DeduceNonTypeTemplateArgument(Context, NTTP, Size,
                                             Info, Deduced);
      }
      if (const DependentSizedArrayType *DependentArrayArg
            = dyn_cast<DependentSizedArrayType>(ArrayArg))
        return DeduceNonTypeTemplateArgument(Context, NTTP,
                                             DependentArrayArg->getSizeExpr(),
                                             Info, Deduced);
      
      // Incomplete type does not match a dependently-sized array type
      return Sema::TDK_NonDeducedMismatch;
    }
      
    //     type(*)(T) 
    //     T(*)() 
    //     T(*)(T) 
    case Type::FunctionProto: {
      const FunctionProtoType *FunctionProtoArg = 
        dyn_cast<FunctionProtoType>(Arg);
      if (!FunctionProtoArg)
        return Sema::TDK_NonDeducedMismatch;
      
      const FunctionProtoType *FunctionProtoParam = 
        cast<FunctionProtoType>(Param);

      if (FunctionProtoParam->getTypeQuals() != 
          FunctionProtoArg->getTypeQuals())
        return Sema::TDK_NonDeducedMismatch;
      
      if (FunctionProtoParam->getNumArgs() != FunctionProtoArg->getNumArgs())
        return Sema::TDK_NonDeducedMismatch;
      
      if (FunctionProtoParam->isVariadic() != FunctionProtoArg->isVariadic())
        return Sema::TDK_NonDeducedMismatch;

      // Check return types.
      if (Sema::TemplateDeductionResult Result
            = DeduceTemplateArguments(Context, TemplateParams,
                                      FunctionProtoParam->getResultType(),
                                      FunctionProtoArg->getResultType(),
                                      Info, Deduced, 0))
        return Result;
      
      for (unsigned I = 0, N = FunctionProtoParam->getNumArgs(); I != N; ++I) {
        // Check argument types.
        if (Sema::TemplateDeductionResult Result
              = DeduceTemplateArguments(Context, TemplateParams,
                                        FunctionProtoParam->getArgType(I),
                                        FunctionProtoArg->getArgType(I),
                                        Info, Deduced, 0))
          return Result;
      }
      
      return Sema::TDK_Success;
    }
     
    //     template-name<T> (where template-name refers to a class template)
    //     template-name<i>
    //     TT<T> (TODO)
    //     TT<i> (TODO)
    //     TT<> (TODO)
    case Type::TemplateSpecialization: {
      const TemplateSpecializationType *SpecParam
        = cast<TemplateSpecializationType>(Param);
      
      // Try to deduce template arguments from the template-id.
      Sema::TemplateDeductionResult Result
        = DeduceTemplateArguments(Context, TemplateParams, SpecParam, Arg,  
                                  Info, Deduced);
      
      if (Result && (TDF & TDF_DerivedClass) && 
          Result != Sema::TDK_Inconsistent) {
        // C++ [temp.deduct.call]p3b3:
        //   If P is a class, and P has the form template-id, then A can be a
        //   derived class of the deduced A. Likewise, if P is a pointer to a
        //   class of the form template-id, A can be a pointer to a derived 
        //   class pointed to by the deduced A.
        //
        // More importantly:
        //   These alternatives are considered only if type deduction would 
        //   otherwise fail.
        if (const RecordType *RecordT = dyn_cast<RecordType>(Arg)) {
          // Use data recursion to crawl through the list of base classes.
          // Visited contains the set of nodes we have already visited, while 
          // ToVisit is our stack of records that we still need to visit.
          llvm::SmallPtrSet<const RecordType *, 8> Visited;
          llvm::SmallVector<const RecordType *, 8> ToVisit;
          ToVisit.push_back(RecordT);
          bool Successful = false;
          while (!ToVisit.empty()) {
            // Retrieve the next class in the inheritance hierarchy.
            const RecordType *NextT = ToVisit.back();
            ToVisit.pop_back();
            
            // If we have already seen this type, skip it.
            if (!Visited.insert(NextT))
              continue;
           
            // If this is a base class, try to perform template argument
            // deduction from it.
            if (NextT != RecordT) {
              Sema::TemplateDeductionResult BaseResult
                = DeduceTemplateArguments(Context, TemplateParams, SpecParam,
                                          QualType(NextT, 0), Info, Deduced);
              
              // If template argument deduction for this base was successful,
              // note that we had some success.
              if (BaseResult == Sema::TDK_Success)
                Successful = true;
              // If deduction against this base resulted in an inconsistent
              // set of deduced template arguments, template argument
              // deduction fails.
              else if (BaseResult == Sema::TDK_Inconsistent)
                return BaseResult;
            }
            
            // Visit base classes
            CXXRecordDecl *Next = cast<CXXRecordDecl>(NextT->getDecl());
            for (CXXRecordDecl::base_class_iterator Base = Next->bases_begin(),
                                                 BaseEnd = Next->bases_end();
               Base != BaseEnd; ++Base) {
              assert(Base->getType()->isRecordType() && 
                     "Base class that isn't a record?");
              ToVisit.push_back(Base->getType()->getAs<RecordType>());
            }
          }
          
          if (Successful)
            return Sema::TDK_Success;
        }
        
      }
      
      return Result;
    }

    //     T type::*
    //     T T::*
    //     T (type::*)()
    //     type (T::*)()
    //     type (type::*)(T)
    //     type (T::*)(T)
    //     T (type::*)(T)
    //     T (T::*)()
    //     T (T::*)(T)
    case Type::MemberPointer: {
      const MemberPointerType *MemPtrParam = cast<MemberPointerType>(Param);
      const MemberPointerType *MemPtrArg = dyn_cast<MemberPointerType>(Arg);
      if (!MemPtrArg)
        return Sema::TDK_NonDeducedMismatch;

      if (Sema::TemplateDeductionResult Result
            = DeduceTemplateArguments(Context, TemplateParams,
                                      MemPtrParam->getPointeeType(),
                                      MemPtrArg->getPointeeType(),
                                      Info, Deduced,
                                      TDF & TDF_IgnoreQualifiers))
        return Result;

      return DeduceTemplateArguments(Context, TemplateParams,
                                     QualType(MemPtrParam->getClass(), 0),
                                     QualType(MemPtrArg->getClass(), 0),
                                     Info, Deduced, 0);
    }

    //     (clang extension)
    //
    //     type(^)(T) 
    //     T(^)() 
    //     T(^)(T) 
    case Type::BlockPointer: {
      const BlockPointerType *BlockPtrParam = cast<BlockPointerType>(Param);
      const BlockPointerType *BlockPtrArg = dyn_cast<BlockPointerType>(Arg);
      
      if (!BlockPtrArg)
        return Sema::TDK_NonDeducedMismatch;
      
      return DeduceTemplateArguments(Context, TemplateParams,
                                     BlockPtrParam->getPointeeType(),
                                     BlockPtrArg->getPointeeType(), Info,
                                     Deduced, 0);
    }

    case Type::TypeOfExpr:
    case Type::TypeOf:
    case Type::Typename:
      // No template argument deduction for these types
      return Sema::TDK_Success;

    default:
      break;
  }

  // FIXME: Many more cases to go (to go).
  return Sema::TDK_Success;
}

static Sema::TemplateDeductionResult
DeduceTemplateArguments(ASTContext &Context, 
                        TemplateParameterList *TemplateParams,
                        const TemplateArgument &Param,
                        const TemplateArgument &Arg,
                        Sema::TemplateDeductionInfo &Info,
                        llvm::SmallVectorImpl<TemplateArgument> &Deduced) {
  switch (Param.getKind()) {
  case TemplateArgument::Null:
    assert(false && "Null template argument in parameter list");
    break;
      
  case TemplateArgument::Type: 
    assert(Arg.getKind() == TemplateArgument::Type && "Type/value mismatch");
    return DeduceTemplateArguments(Context, TemplateParams, Param.getAsType(),
                                   Arg.getAsType(), Info, Deduced, 0);

  case TemplateArgument::Declaration:
    // FIXME: Implement this check
    assert(false && "Unimplemented template argument deduction case");
    Info.FirstArg = Param;
    Info.SecondArg = Arg;
    return Sema::TDK_NonDeducedMismatch;
      
  case TemplateArgument::Integral:
    if (Arg.getKind() == TemplateArgument::Integral) {
      // FIXME: Zero extension + sign checking here?
      if (*Param.getAsIntegral() == *Arg.getAsIntegral())
        return Sema::TDK_Success;

      Info.FirstArg = Param;
      Info.SecondArg = Arg;
      return Sema::TDK_NonDeducedMismatch;
    }

    if (Arg.getKind() == TemplateArgument::Expression) {
      Info.FirstArg = Param;
      Info.SecondArg = Arg;
      return Sema::TDK_NonDeducedMismatch;
    }

    assert(false && "Type/value mismatch");
    Info.FirstArg = Param;
    Info.SecondArg = Arg;
    return Sema::TDK_NonDeducedMismatch;
      
  case TemplateArgument::Expression: {
    if (NonTypeTemplateParmDecl *NTTP 
          = getDeducedParameterFromExpr(Param.getAsExpr())) {
      if (Arg.getKind() == TemplateArgument::Integral)
        // FIXME: Sign problems here
        return DeduceNonTypeTemplateArgument(Context, NTTP, 
                                             *Arg.getAsIntegral(), 
                                             Info, Deduced);
      if (Arg.getKind() == TemplateArgument::Expression)
        return DeduceNonTypeTemplateArgument(Context, NTTP, Arg.getAsExpr(),
                                             Info, Deduced);
      
      assert(false && "Type/value mismatch");
      Info.FirstArg = Param;
      Info.SecondArg = Arg;
      return Sema::TDK_NonDeducedMismatch;
    }
    
    // Can't deduce anything, but that's okay.
    return Sema::TDK_Success;
  }
  case TemplateArgument::Pack:
    assert(0 && "FIXME: Implement!");
    break;
  }
      
  return Sema::TDK_Success;
}

static Sema::TemplateDeductionResult 
DeduceTemplateArguments(ASTContext &Context,
                        TemplateParameterList *TemplateParams,
                        const TemplateArgumentList &ParamList,
                        const TemplateArgumentList &ArgList,
                        Sema::TemplateDeductionInfo &Info,
                        llvm::SmallVectorImpl<TemplateArgument> &Deduced) {
  assert(ParamList.size() == ArgList.size());
  for (unsigned I = 0, N = ParamList.size(); I != N; ++I) {
    if (Sema::TemplateDeductionResult Result
          = DeduceTemplateArguments(Context, TemplateParams,
                                    ParamList[I], ArgList[I], 
                                    Info, Deduced))
      return Result;
  }
  return Sema::TDK_Success;
}

/// \brief Determine whether two template arguments are the same.
static bool isSameTemplateArg(ASTContext &Context, 
                              const TemplateArgument &X,
                              const TemplateArgument &Y) {
  if (X.getKind() != Y.getKind())
    return false;
  
  switch (X.getKind()) {
    case TemplateArgument::Null:
      assert(false && "Comparing NULL template argument");
      break;
      
    case TemplateArgument::Type:
      return Context.getCanonicalType(X.getAsType()) ==
             Context.getCanonicalType(Y.getAsType());
      
    case TemplateArgument::Declaration:
      return X.getAsDecl()->getCanonicalDecl() ==
             Y.getAsDecl()->getCanonicalDecl();
      
    case TemplateArgument::Integral:
      return *X.getAsIntegral() == *Y.getAsIntegral();
      
    case TemplateArgument::Expression:
      // FIXME: We assume that all expressions are distinct, but we should
      // really check their canonical forms.
      return false;
      
    case TemplateArgument::Pack:
      if (X.pack_size() != Y.pack_size())
        return false;
      
      for (TemplateArgument::pack_iterator XP = X.pack_begin(), 
                                        XPEnd = X.pack_end(), 
                                           YP = Y.pack_begin();
           XP != XPEnd; ++XP, ++YP) 
        if (!isSameTemplateArg(Context, *XP, *YP))
          return false;

      return true;
  }

  return false;
}

/// \brief Helper function to build a TemplateParameter when we don't
/// know its type statically.
static TemplateParameter makeTemplateParameter(Decl *D) {
  if (TemplateTypeParmDecl *TTP = dyn_cast<TemplateTypeParmDecl>(D))
    return TemplateParameter(TTP);
  else if (NonTypeTemplateParmDecl *NTTP = dyn_cast<NonTypeTemplateParmDecl>(D))
    return TemplateParameter(NTTP);
  
  return TemplateParameter(cast<TemplateTemplateParmDecl>(D));
}

/// \brief Perform template argument deduction to determine whether
/// the given template arguments match the given class template
/// partial specialization per C++ [temp.class.spec.match].
Sema::TemplateDeductionResult
Sema::DeduceTemplateArguments(ClassTemplatePartialSpecializationDecl *Partial,
                              const TemplateArgumentList &TemplateArgs,
                              TemplateDeductionInfo &Info) {
  // C++ [temp.class.spec.match]p2:
  //   A partial specialization matches a given actual template
  //   argument list if the template arguments of the partial
  //   specialization can be deduced from the actual template argument
  //   list (14.8.2).
  SFINAETrap Trap(*this);
  llvm::SmallVector<TemplateArgument, 4> Deduced;
  Deduced.resize(Partial->getTemplateParameters()->size());
  if (TemplateDeductionResult Result
        = ::DeduceTemplateArguments(Context, 
                                    Partial->getTemplateParameters(),
                                    Partial->getTemplateArgs(), 
                                    TemplateArgs, Info, Deduced))
    return Result;

  InstantiatingTemplate Inst(*this, Partial->getLocation(), Partial,
                             Deduced.data(), Deduced.size());
  if (Inst)
    return TDK_InstantiationDepth;

  // C++ [temp.deduct.type]p2:
  //   [...] or if any template argument remains neither deduced nor
  //   explicitly specified, template argument deduction fails.
  TemplateArgumentListBuilder Builder(Partial->getTemplateParameters(),
                                      Deduced.size());
  for (unsigned I = 0, N = Deduced.size(); I != N; ++I) {
    if (Deduced[I].isNull()) {
      Decl *Param 
        = const_cast<Decl *>(Partial->getTemplateParameters()->getParam(I));
      if (TemplateTypeParmDecl *TTP = dyn_cast<TemplateTypeParmDecl>(Param))
        Info.Param = TTP;
      else if (NonTypeTemplateParmDecl *NTTP 
                 = dyn_cast<NonTypeTemplateParmDecl>(Param))
        Info.Param = NTTP;
      else
        Info.Param = cast<TemplateTemplateParmDecl>(Param);
      return TDK_Incomplete;
    }

    Builder.Append(Deduced[I]);
  }

  // Form the template argument list from the deduced template arguments.
  TemplateArgumentList *DeducedArgumentList 
    = new (Context) TemplateArgumentList(Context, Builder, /*TakeArgs=*/true);
  Info.reset(DeducedArgumentList);

  // Substitute the deduced template arguments into the template
  // arguments of the class template partial specialization, and
  // verify that the instantiated template arguments are both valid
  // and are equivalent to the template arguments originally provided
  // to the class template. 
  ClassTemplateDecl *ClassTemplate = Partial->getSpecializedTemplate();
  const TemplateArgumentList &PartialTemplateArgs = Partial->getTemplateArgs();
  for (unsigned I = 0, N = PartialTemplateArgs.flat_size(); I != N; ++I) {
    Decl *Param = const_cast<Decl *>(
                    ClassTemplate->getTemplateParameters()->getParam(I));
    TemplateArgument InstArg = Instantiate(PartialTemplateArgs[I],
                                           *DeducedArgumentList);
    if (InstArg.isNull()) {
      Info.Param = makeTemplateParameter(Param);
      Info.FirstArg = PartialTemplateArgs[I];
      return TDK_SubstitutionFailure;      
    }
    
    if (InstArg.getKind() == TemplateArgument::Expression) {
      // When the argument is an expression, check the expression result 
      // against the actual template parameter to get down to the canonical
      // template argument.
      Expr *InstExpr = InstArg.getAsExpr();
      if (NonTypeTemplateParmDecl *NTTP 
            = dyn_cast<NonTypeTemplateParmDecl>(Param)) {
        if (CheckTemplateArgument(NTTP, NTTP->getType(), InstExpr, InstArg)) {
          Info.Param = makeTemplateParameter(Param);
          Info.FirstArg = PartialTemplateArgs[I];
          return TDK_SubstitutionFailure;      
        }
      } else if (TemplateTemplateParmDecl *TTP 
                   = dyn_cast<TemplateTemplateParmDecl>(Param)) {
        // FIXME: template template arguments should really resolve to decls
        DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(InstExpr);
        if (!DRE || CheckTemplateArgument(TTP, DRE)) {
          Info.Param = makeTemplateParameter(Param);
          Info.FirstArg = PartialTemplateArgs[I];
          return TDK_SubstitutionFailure;      
        }
      }
    }
    
    if (!isSameTemplateArg(Context, TemplateArgs[I], InstArg)) {
      Info.Param = makeTemplateParameter(Param);
      Info.FirstArg = TemplateArgs[I];
      Info.SecondArg = InstArg;
      return TDK_NonDeducedMismatch;
    }
  }

  if (Trap.hasErrorOccurred())
    return TDK_SubstitutionFailure;

  return TDK_Success;
}

/// \brief Determine whether the given type T is a simple-template-id type.
static bool isSimpleTemplateIdType(QualType T) {
  if (const TemplateSpecializationType *Spec 
        = T->getAsTemplateSpecializationType())
    return Spec->getTemplateName().getAsTemplateDecl() != 0;
  
  return false;
}

/// \brief Substitute the explicitly-provided template arguments into the
/// given function template according to C++ [temp.arg.explicit].
///
/// \param FunctionTemplate the function template into which the explicit
/// template arguments will be substituted.
///
/// \param ExplicitTemplateArguments the explicitly-specified template 
/// arguments.
///
/// \param NumExplicitTemplateArguments the number of explicitly-specified 
/// template arguments in @p ExplicitTemplateArguments. This value may be zero.
///
/// \param Deduced the deduced template arguments, which will be populated 
/// with the converted and checked explicit template arguments.
///
/// \param ParamTypes will be populated with the instantiated function 
/// parameters.
///
/// \param FunctionType if non-NULL, the result type of the function template
/// will also be instantiated and the pointed-to value will be updated with
/// the instantiated function type.
///
/// \param Info if substitution fails for any reason, this object will be
/// populated with more information about the failure.
///
/// \returns TDK_Success if substitution was successful, or some failure
/// condition.
Sema::TemplateDeductionResult
Sema::SubstituteExplicitTemplateArguments(
                                      FunctionTemplateDecl *FunctionTemplate,
                                const TemplateArgument *ExplicitTemplateArgs,
                                          unsigned NumExplicitTemplateArgs,
                            llvm::SmallVectorImpl<TemplateArgument> &Deduced,
                                 llvm::SmallVectorImpl<QualType> &ParamTypes,
                                          QualType *FunctionType,
                                          TemplateDeductionInfo &Info) {
  FunctionDecl *Function = FunctionTemplate->getTemplatedDecl();
  TemplateParameterList *TemplateParams
    = FunctionTemplate->getTemplateParameters();

  if (NumExplicitTemplateArgs == 0) {
    // No arguments to substitute; just copy over the parameter types and
    // fill in the function type.
    for (FunctionDecl::param_iterator P = Function->param_begin(),
                                   PEnd = Function->param_end();
         P != PEnd;
         ++P)
      ParamTypes.push_back((*P)->getType());
    
    if (FunctionType)
      *FunctionType = Function->getType();
    return TDK_Success;
  }
  
  // Substitution of the explicit template arguments into a function template
  /// is a SFINAE context. Trap any errors that might occur.
  SFINAETrap Trap(*this);  
  
  // C++ [temp.arg.explicit]p3:
  //   Template arguments that are present shall be specified in the 
  //   declaration order of their corresponding template-parameters. The 
  //   template argument list shall not specify more template-arguments than
  //   there are corresponding template-parameters. 
  TemplateArgumentListBuilder Builder(TemplateParams, 
                                      NumExplicitTemplateArgs);
  
  // Enter a new template instantiation context where we check the 
  // explicitly-specified template arguments against this function template,
  // and then substitute them into the function parameter types.
  InstantiatingTemplate Inst(*this, FunctionTemplate->getLocation(), 
                             FunctionTemplate, Deduced.data(), Deduced.size(),
           ActiveTemplateInstantiation::ExplicitTemplateArgumentSubstitution);
  if (Inst)
    return TDK_InstantiationDepth;
  
  if (CheckTemplateArgumentList(FunctionTemplate,
                                SourceLocation(), SourceLocation(),
                                ExplicitTemplateArgs,
                                NumExplicitTemplateArgs,
                                SourceLocation(),
                                true,
                                Builder) || Trap.hasErrorOccurred())
    return TDK_InvalidExplicitArguments;
  
  // Form the template argument list from the explicitly-specified
  // template arguments.
  TemplateArgumentList *ExplicitArgumentList 
    = new (Context) TemplateArgumentList(Context, Builder, /*TakeArgs=*/true);
  Info.reset(ExplicitArgumentList);
  
  // Instantiate the types of each of the function parameters given the
  // explicitly-specified template arguments.
  for (FunctionDecl::param_iterator P = Function->param_begin(),
                                PEnd = Function->param_end();
       P != PEnd;
       ++P) {
    QualType ParamType = InstantiateType((*P)->getType(), 
                                         *ExplicitArgumentList, 
                                         (*P)->getLocation(), 
                                         (*P)->getDeclName());
    if (ParamType.isNull() || Trap.hasErrorOccurred())
      return TDK_SubstitutionFailure;
    
    ParamTypes.push_back(ParamType);
  }

  // If the caller wants a full function type back, instantiate the return
  // type and form that function type.
  if (FunctionType) {
    // FIXME: exception-specifications?
    const FunctionProtoType *Proto 
      = Function->getType()->getAsFunctionProtoType();
    assert(Proto && "Function template does not have a prototype?");
    
    QualType ResultType = InstantiateType(Proto->getResultType(),
                                          *ExplicitArgumentList,
                                          Function->getTypeSpecStartLoc(),
                                          Function->getDeclName());
    if (ResultType.isNull() || Trap.hasErrorOccurred())
      return TDK_SubstitutionFailure;
    
    *FunctionType = BuildFunctionType(ResultType, 
                                      ParamTypes.data(), ParamTypes.size(),
                                      Proto->isVariadic(),
                                      Proto->getTypeQuals(),
                                      Function->getLocation(),
                                      Function->getDeclName());
    if (FunctionType->isNull() || Trap.hasErrorOccurred())
      return TDK_SubstitutionFailure;
  }
  
  // C++ [temp.arg.explicit]p2:
  //   Trailing template arguments that can be deduced (14.8.2) may be 
  //   omitted from the list of explicit template-arguments. If all of the 
  //   template arguments can be deduced, they may all be omitted; in this
  //   case, the empty template argument list <> itself may also be omitted.
  //
  // Take all of the explicitly-specified arguments and put them into the
  // set of deduced template arguments. 
  Deduced.reserve(TemplateParams->size());
  for (unsigned I = 0, N = ExplicitArgumentList->size(); I != N; ++I)
    Deduced.push_back(ExplicitArgumentList->get(I));  
  
  return TDK_Success;
}

/// \brief Finish template argument deduction for a function template, 
/// checking the deduced template arguments for completeness and forming
/// the function template specialization.
Sema::TemplateDeductionResult 
Sema::FinishTemplateArgumentDeduction(FunctionTemplateDecl *FunctionTemplate,
                            llvm::SmallVectorImpl<TemplateArgument> &Deduced,
                                      FunctionDecl *&Specialization,
                                      TemplateDeductionInfo &Info) {
  TemplateParameterList *TemplateParams
    = FunctionTemplate->getTemplateParameters();
  
  // C++ [temp.deduct.type]p2:
  //   [...] or if any template argument remains neither deduced nor
  //   explicitly specified, template argument deduction fails.
  TemplateArgumentListBuilder Builder(TemplateParams, Deduced.size());
  for (unsigned I = 0, N = Deduced.size(); I != N; ++I) {
    if (Deduced[I].isNull()) {
      Info.Param = makeTemplateParameter(
                            const_cast<Decl *>(TemplateParams->getParam(I)));
      return TDK_Incomplete;
    }
    
    Builder.Append(Deduced[I]);
  }
  
  // Form the template argument list from the deduced template arguments.
  TemplateArgumentList *DeducedArgumentList 
    = new (Context) TemplateArgumentList(Context, Builder, /*TakeArgs=*/true);
  Info.reset(DeducedArgumentList);
  
  // Template argument deduction for function templates in a SFINAE context.
  // Trap any errors that might occur.
  SFINAETrap Trap(*this);  
  
  // Enter a new template instantiation context while we instantiate the
  // actual function declaration.
  InstantiatingTemplate Inst(*this, FunctionTemplate->getLocation(), 
                             FunctionTemplate, Deduced.data(), Deduced.size(),
              ActiveTemplateInstantiation::DeducedTemplateArgumentSubstitution);
  if (Inst)
    return TDK_InstantiationDepth; 
  
  // Substitute the deduced template arguments into the function template 
  // declaration to produce the function template specialization.
  Specialization = cast_or_null<FunctionDecl>(
                      InstantiateDecl(FunctionTemplate->getTemplatedDecl(),
                                      FunctionTemplate->getDeclContext(),
                                      *DeducedArgumentList));
  if (!Specialization)
    return TDK_SubstitutionFailure;
  
  // If the template argument list is owned by the function template 
  // specialization, release it.
  if (Specialization->getTemplateSpecializationArgs() == DeducedArgumentList)
    Info.take();
  
  // There may have been an error that did not prevent us from constructing a
  // declaration. Mark the declaration invalid and return with a substitution
  // failure.
  if (Trap.hasErrorOccurred()) {
    Specialization->setInvalidDecl(true);
    return TDK_SubstitutionFailure;
  }
  
  return TDK_Success;  
}

/// \brief Perform template argument deduction from a function call
/// (C++ [temp.deduct.call]).
///
/// \param FunctionTemplate the function template for which we are performing
/// template argument deduction.
///
/// \param HasExplicitTemplateArgs whether any template arguments were 
/// explicitly specified.
///
/// \param ExplicitTemplateArguments when @p HasExplicitTemplateArgs is true,
/// the explicitly-specified template arguments.
///
/// \param NumExplicitTemplateArguments when @p HasExplicitTemplateArgs is true,
/// the number of explicitly-specified template arguments in 
/// @p ExplicitTemplateArguments. This value may be zero.
///
/// \param Args the function call arguments
///
/// \param NumArgs the number of arguments in Args
///
/// \param Specialization if template argument deduction was successful,
/// this will be set to the function template specialization produced by 
/// template argument deduction.
///
/// \param Info the argument will be updated to provide additional information
/// about template argument deduction.
///
/// \returns the result of template argument deduction.
Sema::TemplateDeductionResult
Sema::DeduceTemplateArguments(FunctionTemplateDecl *FunctionTemplate,
                              bool HasExplicitTemplateArgs,
                              const TemplateArgument *ExplicitTemplateArgs,
                              unsigned NumExplicitTemplateArgs,
                              Expr **Args, unsigned NumArgs,
                              FunctionDecl *&Specialization,
                              TemplateDeductionInfo &Info) {
  FunctionDecl *Function = FunctionTemplate->getTemplatedDecl();

  // C++ [temp.deduct.call]p1:
  //   Template argument deduction is done by comparing each function template
  //   parameter type (call it P) with the type of the corresponding argument
  //   of the call (call it A) as described below.
  unsigned CheckArgs = NumArgs;
  if (NumArgs < Function->getMinRequiredArguments())
    return TDK_TooFewArguments;
  else if (NumArgs > Function->getNumParams()) {
    const FunctionProtoType *Proto 
      = Function->getType()->getAsFunctionProtoType();
    if (!Proto->isVariadic())
      return TDK_TooManyArguments;
    
    CheckArgs = Function->getNumParams();
  }
    
  // The types of the parameters from which we will perform template argument
  // deduction.
  TemplateParameterList *TemplateParams
    = FunctionTemplate->getTemplateParameters();
  llvm::SmallVector<TemplateArgument, 4> Deduced;
  llvm::SmallVector<QualType, 4> ParamTypes;
  if (NumExplicitTemplateArgs) {
    TemplateDeductionResult Result =
      SubstituteExplicitTemplateArguments(FunctionTemplate,
                                          ExplicitTemplateArgs,
                                          NumExplicitTemplateArgs,
                                          Deduced,
                                          ParamTypes,
                                          0,
                                          Info);
    if (Result)
      return Result;
  } else {
    // Just fill in the parameter types from the function declaration.
    for (unsigned I = 0; I != CheckArgs; ++I)
      ParamTypes.push_back(Function->getParamDecl(I)->getType());
  }
                                        
  // Deduce template arguments from the function parameters.
  Deduced.resize(TemplateParams->size());  
  for (unsigned I = 0; I != CheckArgs; ++I) {
    QualType ParamType = ParamTypes[I];
    QualType ArgType = Args[I]->getType();
    
    // C++ [temp.deduct.call]p2:
    //   If P is not a reference type:
    QualType CanonParamType = Context.getCanonicalType(ParamType);
    bool ParamWasReference = isa<ReferenceType>(CanonParamType);
    if (!ParamWasReference) {
      //   - If A is an array type, the pointer type produced by the 
      //     array-to-pointer standard conversion (4.2) is used in place of 
      //     A for type deduction; otherwise,
      if (ArgType->isArrayType())
        ArgType = Context.getArrayDecayedType(ArgType);
      //   - If A is a function type, the pointer type produced by the 
      //     function-to-pointer standard conversion (4.3) is used in place 
      //     of A for type deduction; otherwise,
      else if (ArgType->isFunctionType())
        ArgType = Context.getPointerType(ArgType);
      else {
        // - If A is a cv-qualified type, the top level cv-qualifiers of A’s
        //   type are ignored for type deduction.
        QualType CanonArgType = Context.getCanonicalType(ArgType);
        if (CanonArgType.getCVRQualifiers())
          ArgType = CanonArgType.getUnqualifiedType();
      }
    }
    
    // C++0x [temp.deduct.call]p3:
    //   If P is a cv-qualified type, the top level cv-qualifiers of P’s type
    //   are ignored for type deduction. 
    if (CanonParamType.getCVRQualifiers())
      ParamType = CanonParamType.getUnqualifiedType();
    if (const ReferenceType *ParamRefType = ParamType->getAs<ReferenceType>()) {
      //   [...] If P is a reference type, the type referred to by P is used 
      //   for type deduction. 
      ParamType = ParamRefType->getPointeeType();
      
      //   [...] If P is of the form T&&, where T is a template parameter, and 
      //   the argument is an lvalue, the type A& is used in place of A for 
      //   type deduction.
      if (isa<RValueReferenceType>(ParamRefType) &&
          ParamRefType->getAsTemplateTypeParmType() &&
          Args[I]->isLvalue(Context) == Expr::LV_Valid)
        ArgType = Context.getLValueReferenceType(ArgType);
    }
    
    // C++0x [temp.deduct.call]p4:
    //   In general, the deduction process attempts to find template argument
    //   values that will make the deduced A identical to A (after the type A
    //   is transformed as described above). [...]
    unsigned TDF = 0;
    
    //     - If the original P is a reference type, the deduced A (i.e., the
    //       type referred to by the reference) can be more cv-qualified than
    //       the transformed A.
    if (ParamWasReference)
      TDF |= TDF_ParamWithReferenceType;
    //     - The transformed A can be another pointer or pointer to member 
    //       type that can be converted to the deduced A via a qualification 
    //       conversion (4.4).
    if (ArgType->isPointerType() || ArgType->isMemberPointerType())
      TDF |= TDF_IgnoreQualifiers;
    //     - If P is a class and P has the form simple-template-id, then the 
    //       transformed A can be a derived class of the deduced A. Likewise,
    //       if P is a pointer to a class of the form simple-template-id, the
    //       transformed A can be a pointer to a derived class pointed to by
    //       the deduced A.
    if (isSimpleTemplateIdType(ParamType) ||
        (isa<PointerType>(ParamType) && 
         isSimpleTemplateIdType(
                              ParamType->getAs<PointerType>()->getPointeeType())))
      TDF |= TDF_DerivedClass;
    
    if (TemplateDeductionResult Result
        = ::DeduceTemplateArguments(Context, TemplateParams,
                                    ParamType, ArgType, Info, Deduced,
                                    TDF))
      return Result;
    
    // FIXME: C++0x [temp.deduct.call] paragraphs 6-9 deal with function
    // pointer parameters. 

    // FIXME: we need to check that the deduced A is the same as A,
    // modulo the various allowed differences.
  }

  return FinishTemplateArgumentDeduction(FunctionTemplate, Deduced, 
                                         Specialization, Info);
}

/// \brief Deduce template arguments when taking the address of a function
/// template (C++ [temp.deduct.funcaddr]).
///
/// \param FunctionTemplate the function template for which we are performing
/// template argument deduction.
///
/// \param HasExplicitTemplateArgs whether any template arguments were 
/// explicitly specified.
///
/// \param ExplicitTemplateArguments when @p HasExplicitTemplateArgs is true,
/// the explicitly-specified template arguments.
///
/// \param NumExplicitTemplateArguments when @p HasExplicitTemplateArgs is true,
/// the number of explicitly-specified template arguments in 
/// @p ExplicitTemplateArguments. This value may be zero.
///
/// \param ArgFunctionType the function type that will be used as the
/// "argument" type (A) when performing template argument deduction from the
/// function template's function type.
///
/// \param Specialization if template argument deduction was successful,
/// this will be set to the function template specialization produced by 
/// template argument deduction.
///
/// \param Info the argument will be updated to provide additional information
/// about template argument deduction.
///
/// \returns the result of template argument deduction.
Sema::TemplateDeductionResult
Sema::DeduceTemplateArguments(FunctionTemplateDecl *FunctionTemplate,
                              bool HasExplicitTemplateArgs,
                              const TemplateArgument *ExplicitTemplateArgs,
                              unsigned NumExplicitTemplateArgs,
                              QualType ArgFunctionType,
                              FunctionDecl *&Specialization,
                              TemplateDeductionInfo &Info) {
  FunctionDecl *Function = FunctionTemplate->getTemplatedDecl();
  TemplateParameterList *TemplateParams
    = FunctionTemplate->getTemplateParameters();
  QualType FunctionType = Function->getType();
  
  // Substitute any explicit template arguments.
  llvm::SmallVector<TemplateArgument, 4> Deduced;
  llvm::SmallVector<QualType, 4> ParamTypes;
  if (HasExplicitTemplateArgs) {
    if (TemplateDeductionResult Result 
          = SubstituteExplicitTemplateArguments(FunctionTemplate, 
                                                ExplicitTemplateArgs, 
                                                NumExplicitTemplateArgs,
                                                Deduced, ParamTypes, 
                                                &FunctionType, Info))
      return Result;
  }

  // Template argument deduction for function templates in a SFINAE context.
  // Trap any errors that might occur.
  SFINAETrap Trap(*this);  
  
  // Deduce template arguments from the function type.
  Deduced.resize(TemplateParams->size());  
  if (TemplateDeductionResult Result
        = ::DeduceTemplateArguments(Context, TemplateParams,
                                    FunctionType, ArgFunctionType, Info, 
                                    Deduced, 0))
    return Result;
  
  return FinishTemplateArgumentDeduction(FunctionTemplate, Deduced, 
                                         Specialization, Info);
}

/// \brief Deduce template arguments for a templated conversion
/// function (C++ [temp.deduct.conv]) and, if successful, produce a
/// conversion function template specialization.
Sema::TemplateDeductionResult
Sema::DeduceTemplateArguments(FunctionTemplateDecl *FunctionTemplate,
                              QualType ToType,
                              CXXConversionDecl *&Specialization,
                              TemplateDeductionInfo &Info) {
  CXXConversionDecl *Conv 
    = cast<CXXConversionDecl>(FunctionTemplate->getTemplatedDecl());
  QualType FromType = Conv->getConversionType();

  // Canonicalize the types for deduction.
  QualType P = Context.getCanonicalType(FromType);
  QualType A = Context.getCanonicalType(ToType);

  // C++0x [temp.deduct.conv]p3:
  //   If P is a reference type, the type referred to by P is used for
  //   type deduction.
  if (const ReferenceType *PRef = P->getAs<ReferenceType>())
    P = PRef->getPointeeType();

  // C++0x [temp.deduct.conv]p3:
  //   If A is a reference type, the type referred to by A is used
  //   for type deduction.
  if (const ReferenceType *ARef = A->getAs<ReferenceType>())
    A = ARef->getPointeeType();
  // C++ [temp.deduct.conv]p2:
  //
  //   If A is not a reference type: 
  else {
    assert(!A->isReferenceType() && "Reference types were handled above");

    //   - If P is an array type, the pointer type produced by the
    //     array-to-pointer standard conversion (4.2) is used in place 
    //     of P for type deduction; otherwise,
    if (P->isArrayType())
      P = Context.getArrayDecayedType(P);
    //   - If P is a function type, the pointer type produced by the
    //     function-to-pointer standard conversion (4.3) is used in
    //     place of P for type deduction; otherwise,
    else if (P->isFunctionType())
      P = Context.getPointerType(P);
    //   - If P is a cv-qualified type, the top level cv-qualifiers of
    //     P’s type are ignored for type deduction.
    else
      P = P.getUnqualifiedType();

    // C++0x [temp.deduct.conv]p3:
    //   If A is a cv-qualified type, the top level cv-qualifiers of A’s
    //   type are ignored for type deduction.
    A = A.getUnqualifiedType();
  }

  // Template argument deduction for function templates in a SFINAE context.
  // Trap any errors that might occur.
  SFINAETrap Trap(*this);  

  // C++ [temp.deduct.conv]p1:
  //   Template argument deduction is done by comparing the return
  //   type of the template conversion function (call it P) with the
  //   type that is required as the result of the conversion (call it
  //   A) as described in 14.8.2.4.
  TemplateParameterList *TemplateParams
    = FunctionTemplate->getTemplateParameters();
  llvm::SmallVector<TemplateArgument, 4> Deduced;
  Deduced.resize(TemplateParams->size());  

  // C++0x [temp.deduct.conv]p4:
  //   In general, the deduction process attempts to find template
  //   argument values that will make the deduced A identical to
  //   A. However, there are two cases that allow a difference:
  unsigned TDF = 0;
  //     - If the original A is a reference type, A can be more
  //       cv-qualified than the deduced A (i.e., the type referred to
  //       by the reference)
  if (ToType->isReferenceType())
    TDF |= TDF_ParamWithReferenceType;
  //     - The deduced A can be another pointer or pointer to member
  //       type that can be converted to A via a qualiﬁcation
  //       conversion.
  //
  // (C++0x [temp.deduct.conv]p6 clarifies that this only happens when
  // both P and A are pointers or member pointers. In this case, we
  // just ignore cv-qualifiers completely).
  if ((P->isPointerType() && A->isPointerType()) ||
      (P->isMemberPointerType() && P->isMemberPointerType()))
    TDF |= TDF_IgnoreQualifiers;
  if (TemplateDeductionResult Result
        = ::DeduceTemplateArguments(Context, TemplateParams,
                                    P, A, Info, Deduced, TDF))
    return Result;

  // FIXME: we need to check that the deduced A is the same as A,
  // modulo the various allowed differences.
  
  // Finish template argument deduction.
  FunctionDecl *Spec = 0;
  TemplateDeductionResult Result
    = FinishTemplateArgumentDeduction(FunctionTemplate, Deduced, Spec, Info);
  Specialization = cast_or_null<CXXConversionDecl>(Spec);
  return Result;
}

/// \brief Returns the more specialization function template according
/// to the rules of function template partial ordering (C++ [temp.func.order]).
///
/// \param FT1 the first function template
///
/// \param FT2 the second function template
///
/// \param isCallContext whether partial ordering is being performed
/// for a function call (which ignores the return types of the
/// functions).
/// 
/// \returns the more specialization function template. If neither
/// template is more specialized, returns NULL.
FunctionTemplateDecl *
Sema::getMoreSpecializedTemplate(FunctionTemplateDecl *FT1,
                                 FunctionTemplateDecl *FT2,
                                 bool isCallContext) {
#if 0
  // FIXME: Implement this
  bool Better1 = isAtLeastAsSpecializedAs(*this, FT1, FT2, isCallContext);
  bool Better2 = isAtLeastAsSpecializedAs(*this, FT2, FT1, isCallContext);
  if (Better1 == Better2)
    return 0;
  if (Better1)
    return FT1;
  return FT2;
#else
  Diag(SourceLocation(), diag::unsup_function_template_partial_ordering);
  return 0;
#endif
}

static void 
MarkDeducedTemplateParameters(Sema &SemaRef,
                              const TemplateArgument &TemplateArg,
                              llvm::SmallVectorImpl<bool> &Deduced);

/// \brief Mark the template arguments that are deduced by the given
/// expression.
static void 
MarkDeducedTemplateParameters(const Expr *E, 
                              llvm::SmallVectorImpl<bool> &Deduced) {
  const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(E);
  if (!E)
    return;

  const NonTypeTemplateParmDecl *NTTP 
    = dyn_cast<NonTypeTemplateParmDecl>(DRE->getDecl());
  if (!NTTP)
    return;

  Deduced[NTTP->getIndex()] = true;
}

/// \brief Mark the template parameters that are deduced by the given
/// type.
static void 
MarkDeducedTemplateParameters(Sema &SemaRef, QualType T,
                              llvm::SmallVectorImpl<bool> &Deduced) {
  // Non-dependent types have nothing deducible
  if (!T->isDependentType())
    return;

  T = SemaRef.Context.getCanonicalType(T);
  switch (T->getTypeClass()) {
  case Type::ExtQual:
    MarkDeducedTemplateParameters(SemaRef, 
                              QualType(cast<ExtQualType>(T)->getBaseType(), 0),
                                  Deduced);
    break;

  case Type::Pointer:
    MarkDeducedTemplateParameters(SemaRef,
                                  cast<PointerType>(T)->getPointeeType(),
                                  Deduced);
    break;

  case Type::BlockPointer:
    MarkDeducedTemplateParameters(SemaRef,
                                  cast<BlockPointerType>(T)->getPointeeType(),
                                  Deduced);
    break;

  case Type::LValueReference:
  case Type::RValueReference:
    MarkDeducedTemplateParameters(SemaRef,
                                  cast<ReferenceType>(T)->getPointeeType(),
                                  Deduced);
    break;

  case Type::MemberPointer: {
    const MemberPointerType *MemPtr = cast<MemberPointerType>(T.getTypePtr());
    MarkDeducedTemplateParameters(SemaRef, MemPtr->getPointeeType(), Deduced);
    MarkDeducedTemplateParameters(SemaRef, QualType(MemPtr->getClass(), 0),
                                  Deduced);
    break;
  }

  case Type::DependentSizedArray:
    MarkDeducedTemplateParameters(cast<DependentSizedArrayType>(T)->getSizeExpr(),
                                  Deduced);
    // Fall through to check the element type

  case Type::ConstantArray:
  case Type::IncompleteArray:
    MarkDeducedTemplateParameters(SemaRef,
                                  cast<ArrayType>(T)->getElementType(),
                                  Deduced);
    break;

  case Type::Vector:
  case Type::ExtVector:
    MarkDeducedTemplateParameters(SemaRef,
                                  cast<VectorType>(T)->getElementType(),
                                  Deduced);
    break;

  case Type::DependentSizedExtVector: {
    const DependentSizedExtVectorType *VecType
      = cast<DependentSizedExtVectorType>(T);
    MarkDeducedTemplateParameters(SemaRef, VecType->getElementType(), Deduced);
    MarkDeducedTemplateParameters(VecType->getSizeExpr(), Deduced);
    break;
  }

  case Type::FunctionProto: {
    const FunctionProtoType *Proto = cast<FunctionProtoType>(T);
    MarkDeducedTemplateParameters(SemaRef, Proto->getResultType(), Deduced);
    for (unsigned I = 0, N = Proto->getNumArgs(); I != N; ++I)
      MarkDeducedTemplateParameters(SemaRef, Proto->getArgType(I), Deduced);
    break;
  }

  case Type::TemplateTypeParm:
    Deduced[cast<TemplateTypeParmType>(T)->getIndex()] = true;
    break;

  case Type::TemplateSpecialization: {
    const TemplateSpecializationType *Spec 
      = cast<TemplateSpecializationType>(T);
    if (TemplateDecl *Template = Spec->getTemplateName().getAsTemplateDecl())
      if (TemplateTemplateParmDecl *TTP 
            = dyn_cast<TemplateTemplateParmDecl>(Template))
        Deduced[TTP->getIndex()] = true;
      
      for (unsigned I = 0, N = Spec->getNumArgs(); I != N; ++I)
        MarkDeducedTemplateParameters(SemaRef, Spec->getArg(I), Deduced);

    break;
  }

  // None of these types have any deducible parts.
  case Type::Builtin:
  case Type::FixedWidthInt:
  case Type::Complex:
  case Type::VariableArray:
  case Type::FunctionNoProto:
  case Type::Record:
  case Type::Enum:
  case Type::Typename:
  case Type::ObjCInterface:
  case Type::ObjCObjectPointer:
#define TYPE(Class, Base)
#define ABSTRACT_TYPE(Class, Base)
#define DEPENDENT_TYPE(Class, Base)
#define NON_CANONICAL_TYPE(Class, Base) case Type::Class:
#include "clang/AST/TypeNodes.def"
    break;
  }
}

/// \brief Mark the template parameters that are deduced by this
/// template argument.
static void 
MarkDeducedTemplateParameters(Sema &SemaRef,
                              const TemplateArgument &TemplateArg,
                              llvm::SmallVectorImpl<bool> &Deduced) {
  switch (TemplateArg.getKind()) {
  case TemplateArgument::Null:
  case TemplateArgument::Integral:
    break;
    
  case TemplateArgument::Type:
    MarkDeducedTemplateParameters(SemaRef, TemplateArg.getAsType(), Deduced);
    break;

  case TemplateArgument::Declaration:
    if (TemplateTemplateParmDecl *TTP 
        = dyn_cast<TemplateTemplateParmDecl>(TemplateArg.getAsDecl()))
      Deduced[TTP->getIndex()] = true;
    break;

  case TemplateArgument::Expression:
    MarkDeducedTemplateParameters(TemplateArg.getAsExpr(), Deduced);
    break;
  case TemplateArgument::Pack:
    assert(0 && "FIXME: Implement!");
    break;
  }
}

/// \brief Mark the template parameters can be deduced by the given
/// template argument list.
///
/// \param TemplateArgs the template argument list from which template
/// parameters will be deduced.
///
/// \param Deduced a bit vector whose elements will be set to \c true
/// to indicate when the corresponding template parameter will be
/// deduced.
void 
Sema::MarkDeducedTemplateParameters(const TemplateArgumentList &TemplateArgs,
                                    llvm::SmallVectorImpl<bool> &Deduced) {
  for (unsigned I = 0, N = TemplateArgs.size(); I != N; ++I)
    ::MarkDeducedTemplateParameters(*this, TemplateArgs[I], Deduced);
}
