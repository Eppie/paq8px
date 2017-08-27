//
// Created by Epstein, Andrew (ELS-PHI) on 8/18/17.
//

#ifndef PAQ8PX_BUF_H
#define PAQ8PX_BUF_H


#include <cassert>
#include "Array.h"

// Buf(n) buf; creates an array of n bytes (must be a power of 2).
// buf[i] returns a reference to the i'th byte with wrap (no out of bounds).
// buf(i) returns i'th byte back from pos (i > 0)
// buf.size() returns n.
typedef unsigned char U8;
int pos;  // Number of input bytes in buf (not wrapped)

class Buf {
    Array<U8> b;
public:
    Buf(int i = 0) : b(i) {}

    void setsize(int i) {
        if(!i) { return; }
        assert(i > 0 && (i & (i - 1)) == 0);
        b.resize(i);
    }

    U8 &operator[](int i) {
        return b[i & (b.size() - 1)];
    }

    int operator()(int i) const {
        assert(i > 0);
        return b[(pos - i) & (b.size() - 1)];
    }

    int size() const {
        return b.size();
    }
};

// IntBuf(n) is a buffer of n int (must be a power of 2).
// intBuf[i] returns a reference to i'th element with wrap.

class IntBuf {
    Array<int> b;
public:
    IntBuf(int i = 0) : b(i) {}

    int &operator[](int i) {
        return b[i & (b.size() - 1)];
    }
};

#endif //PAQ8PX_BUF_H
