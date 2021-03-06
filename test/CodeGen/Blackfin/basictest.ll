; RUN: llvm-as < %s | llc -march=bfin -verify-machineinstrs

define void @void(i32, i32) {
        add i32 0, 0            ; <i32>:3 [#uses=2]
        sub i32 0, 4            ; <i32>:4 [#uses=2]
        br label %5

; <label>:5             ; preds = %5, %2
        add i32 %0, %1          ; <i32>:6 [#uses=2]
        sub i32 %6, %4          ; <i32>:7 [#uses=1]
        icmp sle i32 %7, %3             ; <i1>:8 [#uses=1]
        br i1 %8, label %9, label %5

; <label>:9             ; preds = %5
        add i32 %0, %1          ; <i32>:10 [#uses=0]
        sub i32 %6, %4          ; <i32>:11 [#uses=1]
        icmp sle i32 %11, %3            ; <i1>:12 [#uses=0]
        ret void
}
