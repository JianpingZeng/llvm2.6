add_executable(tblgen
  AsmMatcherEmitter.cpp
  AsmWriterEmitter.cpp
  CallingConvEmitter.cpp
  ClangDiagnosticsEmitter.cpp
  CodeEmitterGen.cpp
  CodeGenDAGPatterns.cpp
  CodeGenInstruction.cpp
  CodeGenTarget.cpp
  DAGISelEmitter.cpp
  FastISelEmitter.cpp
  InstrEnumEmitter.cpp
  InstrInfoEmitter.cpp
  IntrinsicEmitter.cpp
  LLVMCConfigurationEmitter.cpp
  Record.cpp
  RegisterInfoEmitter.cpp
  SubtargetEmitter.cpp
  TGLexer.cpp
  TGParser.cpp
  TGValueTypes.cpp
  TableGen.cpp
  TableGenBackend.cpp
  )

target_link_libraries(tblgen LLVMSupport LLVMSystem)
if( MINGW )
  target_link_libraries(tblgen imagehlp psapi)
endif( MINGW )
if( LLVM_ENABLE_THREADS AND HAVE_LIBPTHREAD )
  target_link_libraries(tblgen pthread)
endif()
