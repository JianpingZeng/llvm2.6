// RUN: clang-cc -fsyntax-only -verify %s
template<int X[10]> struct A;
template<int *X> struct A;
template<int f(float, double)> struct B;
typedef float FLOAT;
template<int (*f)(FLOAT, double)> struct B;
