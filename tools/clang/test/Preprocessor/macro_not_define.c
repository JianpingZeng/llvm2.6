// RUN: clang-cc -E %s | grep '^ # define X 3$'

#define H # 
 #define D define 
 
 #define DEFINE(a, b) H D a b 
 
 DEFINE(X, 3) 

