set(files
  emmintrin.h	
  float.h		
  iso646.h	
  limits.h	
  mm_malloc.h	
  mmintrin.h	
  pmmintrin.h	
  stdarg.h	
  stdbool.h	
  stddef.h	
  stdint.h	
  tgmath.h
  tmmintrin.h
  xmmintrin.h)

#FIXME: Centralize Clang version info
if (MSVC_IDE OR XCODE)
  set(output_dir ${LLVM_BINARY_DIR}/bin/lib/clang/1.0/include)
else ()
  set(output_dir ${LLVM_BINARY_DIR}/lib/clang/1.0/include)
endif ()


foreach( f ${files} )
  set( src ${CMAKE_CURRENT_SOURCE_DIR}/${f} )
  set( dst ${output_dir}/${f} )
  add_custom_command(OUTPUT ${dst}
    DEPENDS ${src}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${src} ${dst}
    COMMENT "Copying clang's ${f}...")
endforeach( f )

add_custom_target(clang-headers ALL
  DEPENDS ${files})

install(FILES ${files}
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  DESTINATION Headers)
