// RUN: clang-cc -triple x86_64-apple-darwin -std=c++0x -S %s -o %t-64.s &&
// RUN: FileCheck -check-prefix LP64 --input-file=%t-64.s %s &&
// RUN: clang-cc -triple i386-apple-darwin -std=c++0x -S %s -o %t-32.s &&
// RUN: FileCheck -check-prefix LP32 --input-file=%t-32.s %s &&
// RUN: true

extern "C" int printf(...);


struct C {
	C() : iC(6) {}
	int iC;
};

int foo() {
  return 6;
};

class X { // ...
public: 
	X(int) {}
	X(const X&, int i = 1, int j = 2, int k = foo()) {
		printf("X(const X&, %d, %d, %d)\n", i, j, k);
	}
};

int main()
{
	X a(1);
	X b(a, 2);
	X c = b;
	X d(a, 5, 6);
}
// CHECK-LP64: call	__ZN1XC1ERK1Xiii
// CHECK-LP64: call	__ZN1XC1ERK1Xiii
// CHECK-LP64: call	__ZN1XC1ERK1Xiii

// CHECK-LP32: call	L__ZN1XC1ERK1Xiii
// CHECK-LP32: call	L__ZN1XC1ERK1Xiii
// CHECK-LP32: call	L__ZN1XC1ERK1Xiii
