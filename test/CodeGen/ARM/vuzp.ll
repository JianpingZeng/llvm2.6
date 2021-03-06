; RUN: llvm-as < %s | llc -march=arm -mattr=+neon | FileCheck %s

%struct.__builtin_neon_v8qi2 = type { <8 x i8>,  <8 x i8> }
%struct.__builtin_neon_v4hi2 = type { <4 x i16>, <4 x i16> }
%struct.__builtin_neon_v2si2 = type { <2 x i32>, <2 x i32> }
%struct.__builtin_neon_v2sf2 = type { <2 x float>, <2 x float> }

%struct.__builtin_neon_v16qi2 = type { <16 x i8>, <16 x i8> }
%struct.__builtin_neon_v8hi2  = type { <8 x i16>, <8 x i16> }
%struct.__builtin_neon_v4si2  = type { <4 x i32>, <4 x i32> }
%struct.__builtin_neon_v4sf2  = type { <4 x float>, <4 x float> }

define <8 x i8> @vuzpi8(<8 x i8>* %A, <8 x i8>* %B) nounwind {
;CHECK: vuzpi8:
;CHECK: vuzp.8
;CHECK-NEXT: vadd.i8
	%tmp1 = load <8 x i8>* %A
	%tmp2 = load <8 x i8>* %B
	%tmp3 = shufflevector <8 x i8> %tmp1, <8 x i8> %tmp2, <8 x i32> <i32 0, i32 2, i32 4, i32 6, i32 8, i32 10, i32 12, i32 14>
	%tmp4 = shufflevector <8 x i8> %tmp1, <8 x i8> %tmp2, <8 x i32> <i32 1, i32 3, i32 5, i32 7, i32 9, i32 11, i32 13, i32 15>
        %tmp5 = add <8 x i8> %tmp3, %tmp4
	ret <8 x i8> %tmp5
}

define <4 x i16> @vuzpi16(<4 x i16>* %A, <4 x i16>* %B) nounwind {
;CHECK: vuzpi16:
;CHECK: vuzp.16
;CHECK-NEXT: vadd.i16
	%tmp1 = load <4 x i16>* %A
	%tmp2 = load <4 x i16>* %B
	%tmp3 = shufflevector <4 x i16> %tmp1, <4 x i16> %tmp2, <4 x i32> <i32 0, i32 2, i32 4, i32 6>
	%tmp4 = shufflevector <4 x i16> %tmp1, <4 x i16> %tmp2, <4 x i32> <i32 1, i32 3, i32 5, i32 7>
        %tmp5 = add <4 x i16> %tmp3, %tmp4
	ret <4 x i16> %tmp5
}

; VUZP.32 is equivalent to VTRN.32 for 64-bit vectors.

define <16 x i8> @vuzpQi8(<16 x i8>* %A, <16 x i8>* %B) nounwind {
;CHECK: vuzpQi8:
;CHECK: vuzp.8
;CHECK-NEXT: vadd.i8
	%tmp1 = load <16 x i8>* %A
	%tmp2 = load <16 x i8>* %B
	%tmp3 = shufflevector <16 x i8> %tmp1, <16 x i8> %tmp2, <16 x i32> <i32 0, i32 2, i32 4, i32 6, i32 8, i32 10, i32 12, i32 14, i32 16, i32 18, i32 20, i32 22, i32 24, i32 26, i32 28, i32 30>
	%tmp4 = shufflevector <16 x i8> %tmp1, <16 x i8> %tmp2, <16 x i32> <i32 1, i32 3, i32 5, i32 7, i32 9, i32 11, i32 13, i32 15, i32 17, i32 19, i32 21, i32 23, i32 25, i32 27, i32 29, i32 31>
        %tmp5 = add <16 x i8> %tmp3, %tmp4
	ret <16 x i8> %tmp5
}

define <8 x i16> @vuzpQi16(<8 x i16>* %A, <8 x i16>* %B) nounwind {
;CHECK: vuzpQi16:
;CHECK: vuzp.16
;CHECK-NEXT: vadd.i16
	%tmp1 = load <8 x i16>* %A
	%tmp2 = load <8 x i16>* %B
	%tmp3 = shufflevector <8 x i16> %tmp1, <8 x i16> %tmp2, <8 x i32> <i32 0, i32 2, i32 4, i32 6, i32 8, i32 10, i32 12, i32 14>
	%tmp4 = shufflevector <8 x i16> %tmp1, <8 x i16> %tmp2, <8 x i32> <i32 1, i32 3, i32 5, i32 7, i32 9, i32 11, i32 13, i32 15>
        %tmp5 = add <8 x i16> %tmp3, %tmp4
	ret <8 x i16> %tmp5
}

define <4 x i32> @vuzpQi32(<4 x i32>* %A, <4 x i32>* %B) nounwind {
;CHECK: vuzpQi32:
;CHECK: vuzp.32
;CHECK-NEXT: vadd.i32
	%tmp1 = load <4 x i32>* %A
	%tmp2 = load <4 x i32>* %B
	%tmp3 = shufflevector <4 x i32> %tmp1, <4 x i32> %tmp2, <4 x i32> <i32 0, i32 2, i32 4, i32 6>
	%tmp4 = shufflevector <4 x i32> %tmp1, <4 x i32> %tmp2, <4 x i32> <i32 1, i32 3, i32 5, i32 7>
        %tmp5 = add <4 x i32> %tmp3, %tmp4
	ret <4 x i32> %tmp5
}

define <4 x float> @vuzpQf(<4 x float>* %A, <4 x float>* %B) nounwind {
;CHECK: vuzpQf:
;CHECK: vuzp.32
;CHECK-NEXT: vadd.f32
	%tmp1 = load <4 x float>* %A
	%tmp2 = load <4 x float>* %B
	%tmp3 = shufflevector <4 x float> %tmp1, <4 x float> %tmp2, <4 x i32> <i32 0, i32 2, i32 4, i32 6>
	%tmp4 = shufflevector <4 x float> %tmp1, <4 x float> %tmp2, <4 x i32> <i32 1, i32 3, i32 5, i32 7>
        %tmp5 = add <4 x float> %tmp3, %tmp4
	ret <4 x float> %tmp5
}
