// RUN: clang-cc -analyze -checker-cfref -analyzer-store=basic -analyzer-constraints=basic -verify %s &&
// RUN: clang-cc -analyze -checker-cfref -analyzer-store=basic -analyzer-constraints=range -verify %s &&
// RUN: clang-cc -analyze -checker-cfref -analyzer-store=region -analyzer-constraints=basic -verify %s &&
// RUN: clang-cc -analyze -checker-cfref -analyzer-store=region -analyzer-constraints=range -verify %s

struct s {
  int data;
  int data_array[10];
};

typedef struct {
  int data;
} STYPE;

void g(char *p);
void g1(struct s* p);

// Array to pointer conversion. Array in the struct field.
void f(void) {
  int a[10];
  int (*p)[10];
  p = &a;
  (*p)[3] = 1;
  
  struct s d;
  struct s *q;
  q = &d;
  q->data = 3;
  d.data_array[9] = 17;
}

// StringLiteral in lvalue context and pointer to array type.
// p: ElementRegion, q: StringRegion
void f2() {
  char *p = "/usr/local";
  char (*q)[4];
  q = &"abc";
}

// Typedef'ed struct definition.
void f3() {
  STYPE s;
}

// Initialize array with InitExprList.
void f4() {
  int a[] = { 1, 2, 3};
  int b[3] = { 1, 2 };
  struct s c[] = {{1,{1}}};
}

// Struct variable in lvalue context.
// Assign UnknownVal to the whole struct.
void f5() {
  struct s data;
  g1(&data);
}

// AllocaRegion test.
void f6() {
  char *p;
  p = __builtin_alloca(10); 
  g(p);
  char c = *p;
  p[1] = 'a';
  // Test if RegionStore::EvalBinOp converts the alloca region to element
  // region.
  p += 2;
}

struct s2;

void g2(struct s2 *p);

// Incomplete struct pointer used as function argument.
void f7() {
  struct s2 *p = __builtin_alloca(10);
  g2(p);
}

// sizeof() is unsigned while -1 is signed in array index.
void f8() {
  int a[10];
  a[sizeof(a)/sizeof(int) - 1] = 1; // no-warning
}

// Initialization of struct array elements.
void f9() {
  struct s a[10];
}

// Initializing array with string literal.
void f10() {
  char a1[4] = "abc";
  char a3[6] = "abc";
}

// Retrieve the default value of element/field region.
void f11() {
  struct s a;
  g1(&a);
  if (a.data == 0) // no-warning
    a.data = 1;
}

// Convert unsigned offset to signed when creating ElementRegion from 
// SymbolicRegion.
void f12(int *list) {
  unsigned i = 0;
  list[i] = 1;
}

struct s1 {
  struct s2 {
    int d;
  } e;
};

// The binding of a.e.d should not be removed. Test recursive subregion map
// building: a->e, e->d. Only then 'a' could be added to live region roots.
void f13(double timeout) {
  struct s1 a;
  a.e.d = (long) timeout;
  if (a.e.d == 10)
    a.e.d = 4;
}

struct s3 {
  int a[2];
};

static struct s3 opt;

// Test if the embedded array is retrieved correctly.
void f14() {
  struct s3 my_opt = opt;
}

void bar(int*);

// Test if the array is correctly invalidated.
void f15() {
  int a[10];
  bar(a);
  if (a[1]) // no-warning
    (void)1;
}

struct s3 p[1];

// Code from postgresql.
// Current cast logic of region store mistakenly leaves the final result region
// an ElementRegion of type 'char'. Then load a nonloc::SymbolVal from it and
// assigns to 'a'. 
void f16(struct s3 *p) {
  struct s3 a = *((struct s3*) ((char*) &p[0]));
}

void inv(struct s1 *);

// Invalidate the struct field.
void f17() {
  struct s1 t;
  int x;
  inv(&t);
  if (t.e.d)
    x = 1;
}

void read(char*);

void f18() {
  char *q;
  char *p = (char *) __builtin_alloca(10);
  read(p);
  q = p;
  q++;
  if (*q) { // no-warning
  }
}
