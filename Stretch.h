#ifndef PAQ8PX_STRETCH_H
#define PAQ8PX_STRETCH_H

#include <cassert>
#include "Array.h"

// return p = 1/(1 + exp(-d)), d scaled by 8 bits, p scaled by 12 bits
int squash(int d) {
    static const int t[33] = {
            1, 2, 3, 6, 10, 16, 27, 45, 73, 120, 194, 310, 488, 747, 1101,
            1546, 2047, 2549, 2994, 3348, 3607, 3785, 3901, 3975, 4022,
            4050, 4068, 4079, 4085, 4089, 4092, 4093, 4094};
    if(d > 2047) { return 4095; }
    if(d < -2047) { return 0; }
    int w = d & 127;
    d = (d >> 7) + 16;
    return (t[d] * (128 - w) + t[(d + 1)] * w + 64) >> 7;
}

// Inverse of squash. d = ln(p/(1-p)), d scaled by 8 bits, p by 12 bits.
// d has range -2047 to 2047 representing -8 to 8.  p has range 0 to 4095.

class Stretch {
    Array<short> t;
public:
    Stretch();

    int operator()(int p) const {
        assert(p >= 0 && p < 4096);
        DEBUG_MSG("p: " << p << ", t[p]: " << t[p]);
        return t[p];
    }
} stretch;

Stretch::Stretch() : t(4096) {
    int pi = 0;
    for(int x = -2047; x <= 2047; ++x) {  // invert squash()
        int i = squash(x);
        for(int j = pi; j <= i; ++j) {
            t[j] = x;
        }
        pi = i + 1;
    }
    t[4095] = 2047;
}

#endif //PAQ8PX_STRETCH_H
