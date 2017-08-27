#ifndef PAQ8PX_PREDICTOR_H
#define PAQ8PX_PREDICTOR_H

// A Predictor estimates the probability that the next bit of
// uncompressed data is 1.  Methods:
// p() returns P(1) as a 12 bit number (0-4095).
// update(y) trains the predictor with the actual bit (0 or 1).

#include <cassert>
#include "utils.h"
#include "APM1.h"
#include "models/models.h"

class Predictor {
    int pr;  // next prediction
public:
    Predictor();

    int p() const {
        assert(pr >= 0 && pr < 4096);
        return pr;
    }

    void update();
};

Predictor::Predictor() : pr(2048) {}

void Predictor::update() {
    static APM1 a(256), a1(0x10000), a2(0x10000), a3(0x10000),
            a4(0x10000), a5(0x10000), a6(0x10000);

    // Update global context: pos, bpos, c0, c4, buf, f4
    c0 += c0 + y;
    if(c0 >= 256) {
        buf[pos++] = c0;
        c4 = (c4 << 8) + c0 - 256;
        c0 = 1;

        int b1 = buf(1);
        b2 = b1;
        if(b1 == '.' || b1 == '!' || b1 == '?' || b1 == '/' || b1 == ')' || b1 == '}') {
            f4 = (f4 & 0xfffffff0) + 2;
        }
        if(b1 == 32) {
            --b1;
        }
        f4 = f4 * 16 + (b1 >> 4);

    }
    bpos = (bpos + 1) & 7;

    // Filter the context model with APMs
    int pr0 = contextModel2();

    pr = a.p(pr0, c0);

    int pr1 = a1.p(pr0, c0 + 256 * buf(1));
    int pr2 = a2.p(pr0, (c0 ^ hash(buf(1), buf(2))) & 0xffff);
    int pr3 = a3.p(pr0, (c0 ^ hash(buf(1), buf(2), buf(3))) & 0xffff);
    pr0 = (pr0 + pr1 + pr2 + pr3 + 2) >> 2;

    pr1 = a4.p(pr, c0 + 256 * buf(1));
    pr2 = a5.p(pr, (c0 ^ hash(buf(1), buf(2))) & 0xffff);
    pr3 = a6.p(pr, (c0 ^ hash(buf(1), buf(2), buf(3))) & 0xffff);
    pr = (pr + pr1 + pr2 + pr3 + 2) >> 2;

    pr = (pr + pr0 + 1) >> 1;
}

#endif //PAQ8PX_PREDICTOR_H
