// RUN: clang-cc -fsyntax-only -verify %s
template<typename T, typename U = int> struct A; // expected-note 2{{template is declared here}}

template<> struct A<double, double>; // expected-note{{forward declaration}}

template<> struct A<float, float> {  // expected-note{{previous definition}}
  int x;
};

template<> struct A<float> { // expected-note{{previous definition}}
  int y;
};

int test_specs(A<float, float> *a1, A<float, int> *a2) {
  return a1->x + a2->y;
}

int test_incomplete_specs(A<double, double> *a1, 
                          A<double> *a2)
{
  (void)a1->x; // expected-error{{incomplete definition of type 'A<double, double>'}}
  (void)a2->x; // expected-error{{implicit instantiation of undefined template 'struct A<double, int>'}}
}

typedef float FLOAT;

template<> struct A<float, FLOAT>;

template<> struct A<FLOAT, float> { }; // expected-error{{redefinition}}

template<> struct A<float, int> { }; // expected-error{{redefinition}}

template<typename T, typename U = int> struct X;

template <> struct X<int, int> { int foo(); }; // #1
template <> struct X<float> { int bar(); };  // #2

typedef int int_type;
void testme(X<int_type> *x1, X<float, int> *x2) { 
  (void)x1->foo(); // okay: refers to #1
  (void)x2->bar(); // okay: refers to #2
}

// Make sure specializations are proper classes.
template<>
struct A<char> {
  A();
};

A<char>::A() { }

// Diagnose specialization errors
struct A<double> { }; // expected-error{{template specialization requires 'template<>'}}

template<> struct ::A<double>;

namespace N {
  template<typename T> struct B; // expected-note 2{{template is declared here}}

  template<> struct ::N::B<char>; // okay
  template<> struct ::N::B<short>; // okay
  template<> struct ::N::B<int>; // okay

  int f(int);
}

template<> struct N::B<int> { }; // okay

template<> struct N::B<float> { }; // expected-error{{class template specialization of 'B' not in namespace 'N'}}

namespace M {
  template<> struct ::N::B<short> { }; // expected-error{{class template specialization of 'B' not in a namespace enclosing 'N'}}

  template<> struct ::A<long double>; // expected-error{{class template specialization of 'A' must occur in the global scope}}
}

template<> struct N::B<char> { 
  int testf(int x) { return f(x); }
};

