//
// Created by Epstein, Andrew (ELS-PHI) on 8/18/17.
//

#ifndef PAQ8PX_BH_H
#define PAQ8PX_BH_H

#include <cassert>
#include "Array.h"

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;

// A BH maps a 32 bit hash to an array of B bytes (checksum and B-2 values)
//
// BH bh(N); creates N element table with B bytes each.
//   N must be a power of 2.  The first byte of each element is
//   reserved for a checksum to detect collisions.  The remaining
//   B-1 bytes are values, prioritized by the first value.  This
//   byte is 0 to mark an unused element.
//
// bh[i] returns a pointer to the i'th element, such that
//   bh[i][0] is a checksum of i, bh[i][1] is the priority, and
//   bh[i][2..B-1] are other values (0-255).
//   The low lg(n) bits as an index into the table.
//   If a collision is detected, up to M nearby locations in the same
//   cache line are tested and the first matching checksum or
//   empty element is returned.
//   If no match or empty element is found, then the lowest priority
//   element is replaced.

// 2 byte checksum with LRU replacement (except last 2 by priority)
template<int B>
class BH {
    enum {
        M = 8
    };  // search limit
    Array<U8> t; // elements
    U32 n; // size-1
public:
    BH(int i) : t(i * B), n(i - 1) {
        assert(B >= 2 && i > 0 && (i & (i - 1)) == 0); // size a power of 2?
    }

    U8 *operator[](U32 i);
};

template<int B>
inline U8 *BH<B>::operator[](U32 i) {
    U16 chk = (i >> 16 ^ i) & 0xffff;
    i = i * M & n;
    U8 *p;
    U16 *cp;
    int j;
    for(j = 0; j < M; ++j) {
        p = &t[(i + j) * B];
        cp = (U16 *) p;
        if(p[2] == 0) {
            *cp = chk;
            break;
        }
        if(*cp == chk) { break; }  // found
    }
    if(j == 0) { return p + 1; }  // front
    static U8 tmp[B];  // element to move to front
    if(j == M) {
        --j;
        memset(tmp, 0, B);
        memmove(tmp, &chk, 2);
        if(M > 2 && t[(i + j) * B + 2] > t[(i + j - 1) * B + 2]) --j;
    } else memcpy(tmp, cp, B);
    memmove(&t[(i + 1) * B], &t[i * B], j * B);
    memcpy(&t[i * B], tmp, B);
    return &t[i * B + 1];
}


#endif //PAQ8PX_BH_H
