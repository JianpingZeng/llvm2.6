; RUN: llvm-as < %s | llc -march=systemz
define i64 @foo(i64 %a, i64 %b) {
entry:
    %c = add i64 %a, 1
    ret i64 %c
}