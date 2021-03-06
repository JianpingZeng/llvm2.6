; RUN: llvm-as < %s | llc -march=cellspu

declare {i16, i1} @llvm.smul.with.overflow.i16(i16 %a, i16 %b)
define i1 @a(i16 %x) zeroext nounwind {
  %res = call {i16, i1} @llvm.smul.with.overflow.i16(i16 %x, i16 3)
  %obil = extractvalue {i16, i1} %res, 1
  ret i1 %obil
}

declare {i16, i1} @llvm.umul.with.overflow.i16(i16 %a, i16 %b)
define i1 @b(i16 %x) zeroext nounwind {
  %res = call {i16, i1} @llvm.umul.with.overflow.i16(i16 %x, i16 3)
  %obil = extractvalue {i16, i1} %res, 1
  ret i1 %obil
}
