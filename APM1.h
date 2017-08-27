#ifndef PAQ8PX_APM1_H
#define PAQ8PX_APM1_H

// APM1 maps a probability and a context into a new probability
// that bit y will next be 1.  After each guess it updates
// its state to improve future guesses.  Methods:
//
// APM1 a(N) creates with N contexts, uses 66*N bytes memory.
// a.p(pr, cx, rate=7) returned adjusted probability in context cx (0 to
//   N-1).  rate determines the learning rate (smaller = faster, default 7).
//   Probabilities are scaled 12 bits (0-4095).

#include <cassert>
#include "Array.h"
#include "Stretch.h"

typedef unsigned short U16;

class APM1 {
    int index;     // last p, context
    const int N;   // number of contexts
    Array<U16> t;  // [N][33]:  p, context -> p
public:
    APM1(int n);

    int p(int pr = 2048, int cxt = 0, int rate = 7) {
        assert(pr >= 0 && pr < 4096 && cxt >= 0 && cxt < N && rate > 0 && rate < 32);
        pr = stretch(pr);
        int g = (y << 16) + (y << rate) - y - y;
        t[index] += (g - t[index]) >> rate;
        t[index + 1] += (g - t[index + 1]) >> rate;
        const int w = pr & 127;  // interpolation weight (33 points)
        index = ((pr + 2048) >> 7) + cxt * 33;
        return (t[index] * (128 - w) + t[index + 1] * w) >> 11;
    }
};

// maps p, cxt -> p initially
APM1::APM1(int n) : index(0), N(n), t(n * 33) {
    for(int i = 0; i < N; ++i) {
        for(int j = 0; j < 33; ++j) {
            t[i * 33 + j] = i == 0 ? squash((j - 16) * 128) * 16 : t[j];
        }
    }
}


#endif //PAQ8PX_APM1_H
