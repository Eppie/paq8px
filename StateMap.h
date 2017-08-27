//
// Created by Epstein, Andrew (ELS-PHI) on 8/18/17.
//

#ifndef PAQ8PX_STATEMAP_H
#define PAQ8PX_STATEMAP_H

#include <cassert>
#include "Array.h"
// A StateMap maps a context to a probability.  Methods:
//
// StateMap sm(n) creates a StateMap with n contexts using 4*n bytes memory.
// sm.p(y, cx, limit) converts state cx (0..n-1) to a probability (0..4095).
//     that the next y=1, updating the previous prediction with y (0..1).
//     limit (1..1023, default 1023) is the maximum count for computing a
//     prediction.  Larger values are better for stationary sources.

class StateMap {
protected:
    const int N;  // Number of contexts
    int cxt;      // Context of last prediction
    Array<U32> t;       // cxt -> prediction in high 22 bits, count in low 10 bits
    inline void update(int limit) {
        assert(cxt >= 0 && cxt < N);
        U32 *p = &t[cxt], p0 = p[0];
        int n = p0 & 1023, pr = p0 >> 10;  // count, prediction
        if(n < limit) { ++p0; }
        else { p0 = (p0 & 0xfffffc00) | limit; }
        p0 += (((y << 22) - pr) >> 3) * dt[n] & 0xfffffc00;
        p[0] = p0;
    }

public:
    StateMap(int n = 256);

    // update bit y (0..1), predict next bit in context cx
    int p(int cx, int limit = 1023) {
        assert(cx >= 0 && cx < N);
        assert(limit > 0 && limit < 1024);
        update(limit);
        return t[cxt = cx] >> 20;
    }
};

StateMap::StateMap(int n) : N(n), cxt(0), t(n) {
    for(int i = 0; i < N; ++i) {
        t[i] = 1 << 31;
    }
}

// An APM maps a probability and a context to a new probability.  Methods:
//
// APM a(n) creates with n contexts using 96*n bytes memory.
// a.pp(y, pr, cx, limit) updates and returns a new probability (0..4095)
//     like with StateMap.  pr (0..4095) is considered part of the context.
//     The output is computed by interpolating pr into 24 ranges nonlinearly
//     with smaller ranges near the ends.  The initial output is pr.
//     y=(0..1) is the last bit.  cx=(0..n-1) is the other context.
//     limit=(0..1023) defaults to 255.

class APM : public StateMap {
public:
    APM(int n);

    int p(int pr, int cx, int limit = 255) {
        // assert(y>>1==0);
        assert(pr >= 0 && pr < 4096);
        assert(cx >= 0 && cx < N / 24);
        assert(limit > 0 && limit < 1024);
        update(limit);
        pr = (stretch(pr) + 2048) * 23;
        int wt = pr & 0xfff;  // interpolation weight of next element
        cx = cx * 24 + (pr >> 12);
        assert(cx >= 0 && cx < N - 1);
        cxt = cx + (wt >> 11);
        pr = ((t[cx] >> 13) * (0x1000 - wt) + (t[cx + 1] >> 13) * wt) >> 19;
        return pr;
    }
};

APM::APM(int n) : StateMap(n * 24) {
    for(int i = 0; i < N; ++i) {
        int p = ((i % 24 * 2 + 1) * 4096) / 48 - 2048;
        t[i] = (U32(squash(p)) << 20) + 6;
    }
}


#endif //PAQ8PX_STATEMAP_H
