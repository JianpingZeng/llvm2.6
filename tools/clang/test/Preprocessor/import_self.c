// RUN: clang-cc -E -I. %s | grep BODY_OF_FILE | wc -l | grep 1

// This #import should have no effect, as we're importing the current file.
#import <import_self.c>

BODY_OF_FILE

