#ifndef PAQ8PX_IM8BITMODEL_H
#define PAQ8PX_IM8BITMODEL_H

#include "../utils.h"
#include "../Mixer.h"
#include "../ContextMap.h"

// Model for 8-bit image data
void im8bitModel(Mixer &m, int w, int gray = 0) {
    const int SC = 0x20000;
    static SmallStationaryContextMap scm1(SC), scm2(SC),
            scm3(SC), scm4(SC), scm5(SC), scm6(SC * 2), scm7(SC);
    static ContextMap cm(MEM * 4, 45 + 12 + 5);
    static int itype = 0, id8 = 1, id9 = 1;
    static int ctx, col = 0;
    // Select nearby pixels as context
    if(!bpos) {
        assert(w > 3);
        int mean = buf(1) + buf(w - 1) + buf(w) + buf(w + 1);
        const int var = (sqrbuf(1) + sqrbuf(w - 1) + sqrbuf(w) + sqrbuf(w + 1) - mean * mean / 4) >> 2;
        mean >>= 2;
        const int logvar = ilog(var);
        int i = 0;

        const int errr = (buf(2) + buf(w + 1) - buf(w));
        if(abs(errr - buf(w - 1) + buf(1) - buf(w)) > 255) { id8++; } else { id9++; }
        if(blpos == 0) { id8 = id9 = 1, itype = 0; }    // reset on new block
        if(blpos % w == 0 && blpos > w) { itype = (id9 / id8) < 4; } // select model

        if(itype == 0) { //faster, for smooth images
            cm.set(hash(++i, buf(1), 0));
            cm.set(hash(++i, buf(w - 1), 0));
            cm.set(hash(++i, buf(w - 2), 0));
            cm.set(hash(++i, buf(2), 0));
            cm.set(hash(++i, buf(w * 2 - 1), 0));
            cm.set(hash(++i, buf(w - 1) + buf(1) - buf(w), buf(1)));
            cm.set(hash(++i, buf(w + 1), 0));
            cm.set(hash(++i, buf(w * 2 - 2), 0));
            cm.set(hash(++i, 2 * buf(w - 1) - buf(w * 2 - 1), buf(1)));
            cm.set(hash(++i, 2 * buf(1) - buf(2), buf(1)));
            cm.set(hash(++i, (abs(buf(1) - buf(2)) + abs(buf(w - 1) - buf(w)) + abs(buf(w - 1) - buf(w - 2))), buf(1)));
            cm.set(hash(++i,
                        (abs(buf(1) - buf(w)) + abs(buf(w - 1) - buf(w * 2 - 1)) + abs(buf(w - 2) - buf(w * 2 - 2))),
                        buf(1)));
            cm.set(hash(++i, abs(errr - buf(w - 1) + buf(1) - buf(w)), buf(1)));
            cm.set(hash(++i, mean, logvar));
            cm.set(hash(++i, 2 * buf(1) - buf(2), 2 * buf(w - 1) - buf(w * 2 - 1)));
            cm.set(hash(++i, (abs(buf(1) - buf(2)) + abs(buf(w - 1) - buf(w)) + abs(buf(w - 1) - buf(w - 2))),
                        (abs(buf(1) - buf(w)) + abs(buf(w - 1) - buf(w * 2 - 1)) + abs(buf(w - 2) - buf(w * 2 - 2)))));
            cm.set(hash(++i, buf(1) >> 2, buf(w) >> 2));
            cm.set(hash(++i, buf(1) >> 2, buf(2) >> 2));
            cm.set(hash(++i, buf(w) >> 2, buf(w * 2) >> 2));
            cm.set(hash(++i, buf(1) >> 2, buf(w - 1) >> 2));
            cm.set(hash(++i, buf(w) >> 2, buf(w + 1) >> 2));
            cm.set(hash(++i, buf(w + 1) >> 2, buf(w + 2) >> 2));
            cm.set(hash(++i, (buf(w + 1) + buf(w * 2 + 2)) >> 1));
            cm.set(hash(++i, (buf(w - 1) + buf(w * 2 - 2)) >> 1));
            cm.set(hash(++i, 2 * buf(w - 1) - buf(w * 2 - 1), buf(w - 1)));
            cm.set(hash(++i, 2 * buf(1) - buf(2), buf(w - 1)));
            cm.set(hash(++i, buf(w * 2 - 1), buf(w - 2), buf(1)));
            cm.set(hash(++i, (buf(1) + buf(w)) >> 1));
            cm.set(hash(++i, (buf(1) + buf(2)) >> 1));
            cm.set(hash(++i, (buf(w) + buf(w * 2)) >> 1));
            cm.set(hash(++i, (buf(1) + buf(w - 1)) >> 1));
            cm.set(hash(++i, (buf(w) + buf(w + 1)) >> 1));
            cm.set(hash(++i, (buf(w + 1) + buf(w + 2)) >> 1));
            cm.set(hash(++i, (buf(w + 1) + buf(w * 2 + 2)) >> 1));
            cm.set(hash(++i, (buf(w - 1) + buf(w * 2 - 2)) >> 1));
            cm.set(hash(++i, buf(w * 2 - 2), buf(w - 1), buf(1)));
            cm.set(hash(++i, buf(w + 1), buf(w - 1), buf(w - 2), buf(1)));
            cm.set(hash(++i, 2 * buf(1) - buf(2), buf(w - 2), buf(w * 2 - 2)));
            cm.set(hash(++i, buf(2), buf(w + 1), buf(w), buf(w - 1)));
            cm.set(hash(++i, buf(w * 3), buf(w), buf(1)));
            cm.set(hash(++i, buf(w) >> 2, buf(3) >> 2, buf(w - 1) >> 2));
            cm.set(hash(++i, buf(3) >> 2, buf(w - 2) >> 2, buf(w * 2 - 2) >> 2));
            cm.set(hash(++i, buf(w) >> 2, buf(1) >> 2, buf(w - 1) >> 2));
            cm.set(hash(++i, buf(w - 1) >> 2, buf(w) >> 2, buf(w + 1) >> 2));
            cm.set(hash(++i, buf(1) >> 2, buf(w - 1) >> 2, buf(w * 2 - 1) >> 2));
        } else {
            i = 512;
            cm.set(hash(++i, buf(1), 0));
            cm.set(hash(++i, buf(2), 0));
            cm.set(hash(++i, buf(w), 0));
            cm.set(hash(++i, buf(w + 1), 0));
            cm.set(hash(++i, buf(w - 1), 0));
            cm.set(hash(++i, (buf(2) + buf(w) - buf(w + 1)), 0));
            cm.set(hash(++i, (buf(w) + buf(2) - buf(w + 1)) >> 1, 0));
            cm.set(hash(++i, (buf(2) + buf(w + 1)) >> 1, 0));
            cm.set(hash(++i, (buf(w - 1) - buf(w)), buf(1) >> 1));
            cm.set(hash(++i, (buf(w) - buf(w + 1)), buf(1) >> 1));
            cm.set(hash(++i, (buf(w + 1) + buf(2)), buf(1) >> 1));
            cm.set(hash(++i, buf(1) >> 2, buf(w) >> 2));
            cm.set(hash(++i, buf(1) >> 2, buf(2) >> 2));
            cm.set(hash(++i, buf(w) >> 2, buf(w * 2) >> 2));
            cm.set(hash(++i, buf(1) >> 2, buf(w - 1) >> 2));
            cm.set(hash(++i, buf(w) >> 2, buf(w + 1) >> 2));
            cm.set(hash(++i, buf(w + 1) >> 2, buf(w + 2) >> 2));
            cm.set(hash(++i, buf(w + 1) >> 2, buf(w * 2 + 2) >> 2));
            cm.set(hash(++i, buf(w - 1) >> 2, buf(w * 2 - 2) >> 2));
            cm.set(hash(++i, (buf(1) + buf(w)) >> 1));
            cm.set(hash(++i, (buf(1) + buf(2)) >> 1));
            cm.set(hash(++i, (buf(w) + buf(w * 2)) >> 1));
            cm.set(hash(++i, (buf(1) + buf(w - 1)) >> 1));
            cm.set(hash(++i, (buf(w) + buf(w + 1)) >> 1));
            cm.set(hash(++i, (buf(w + 1) + buf(w + 2)) >> 1));
            cm.set(hash(++i, (buf(w + 1) + buf(w * 2 + 2)) >> 1));
            cm.set(hash(++i, (buf(w - 1) + buf(w * 2 - 2)) >> 1));
            cm.set(hash(++i, buf(w) >> 2, buf(1) >> 2, buf(w - 1) >> 2));
            cm.set(hash(++i, buf(w - 1) >> 2, buf(w) >> 2, buf(w + 1) >> 2));
            cm.set(hash(++i, buf(1) >> 2, buf(w - 1) >> 2, buf(w * 2 - 1) >> 2));
            cm.set(hash(++i, (buf(3) + buf(w)) >> 1, buf(1) >> 2, buf(2) >> 2));
            cm.set(hash(++i, (buf(2) + buf(1)) >> 1, (buf(w) + buf(w * 2)) >> 1, buf(w - 1) >> 2));
            cm.set(hash(++i, (buf(2) + buf(1)) >> 2, (buf(w - 1) + buf(w)) >> 2));
            cm.set(hash(++i, (buf(2) + buf(1)) >> 1, (buf(w) + buf(w * 2)) >> 1));
            cm.set(hash(++i, (buf(2) + buf(1)) >> 1, (buf(w - 1) + buf(w * 2 - 2)) >> 1));
            cm.set(hash(++i, (buf(2) + buf(1)) >> 1, (buf(w + 1) + buf(w * 2 + 2)) >> 1));
            cm.set(hash(++i, (buf(w) + buf(w * 2)) >> 1, (buf(w - 1) + buf(w * 2 + 2)) >> 1));
            cm.set(hash(++i, (buf(w - 1) + buf(w)) >> 1, (buf(w) + buf(w + 1)) >> 1));
            cm.set(hash(++i, (buf(1) + buf(w - 1)) >> 1, (buf(w) + buf(w * 2)) >> 1));
            cm.set(hash(++i, (buf(1) + buf(w - 1)) >> 2, (buf(w) + buf(w + 1)) >> 2));
            cm.set(hash(++i, (((buf(1) - buf(w - 1)) >> 1) + buf(w)) >> 2));
            cm.set(hash(++i, (((buf(w - 1) - buf(w)) >> 1) + buf(1)) >> 2));
            cm.set(hash(++i, (-buf(1) + buf(w - 1) + buf(w)) >> 2));
            cm.set(hash(++i, (buf(1) * 2 - buf(2)) >> 1));
            cm.set(hash(++i, mean, logvar));
            cm.set(hash(++i, (buf(w) * 2 - buf(w * 2)) >> 1));
            cm.set(hash(++i, (buf(1) + buf(w) - buf(w + 1)) >> 1));
            cm.set(hash(++i, (buf(4) + buf(3)) >> 2, (buf(w - 1) + buf(w)) >> 2));
            cm.set(hash(++i, (buf(4) + buf(3)) >> 1, (buf(w) + buf(w * 2)) >> 1));
            cm.set(hash(++i, (buf(4) + buf(3)) >> 1, (buf(w - 1) + buf(w * 2 - 2)) >> 1));
            cm.set(hash(++i, (buf(4) + buf(3)) >> 1, (buf(w + 1) + buf(w * 2 + 2)) >> 1));
            cm.set(hash(++i, (buf(4) + buf(1)) >> 2, (buf(w - 3) + buf(w)) >> 2));
            cm.set(hash(++i, (buf(4) + buf(1)) >> 1, (buf(w) + buf(w * 2)) >> 1));
            cm.set(hash(++i, (buf(4) + buf(1)) >> 1, (buf(w - 3) + buf(w * 2 - 3)) >> 1));
            cm.set(hash(++i, (buf(4) + buf(1)) >> 1, (buf(w + 3) + buf(w * 2 + 3)) >> 1));
            cm.set(hash(++i, buf(w) >> 2, buf(3) >> 2, buf(w - 1) >> 2));
            cm.set(hash(++i, buf(3) >> 2, buf(w - 2) >> 2, buf(w * 2 - 2) >> 2));
        }

        int WWW = buf(3), WW = buf(2), W = buf(1), NW = buf(w + 1), N = buf(w), NE = buf(w - 1), NEE = buf(
                w - 2), NNW = buf(w * 2 + 1), NN = buf(w * 2), NNE = buf(w * 2 - 1), NNN = buf(w * 3);

        if(gray) {
            ctx = min(0x1F, (blpos % w) / max(1, w / 32)) |
                  ((((abs(W - N) * 16 > W + N) << 1) | (abs(N - NW) > 8)) << 5) | ((W + N) & 0x180);

            cm.set(hash(++i, (N + 1) >> 1, LogMeanDiffQt(N, Clip(NN * 2 - NNN))));
            cm.set(hash(++i, (W + 1) >> 1, LogMeanDiffQt(W, Clip(WW * 2 - WWW))));
            cm.set(hash(++i, Clamp4(W + N - NW, W, NW, N, NE), LogMeanDiffQt(Clip(N + NE - NNE), Clip(N + NW - NNW))));
            cm.set(hash(++i, (NNN + N + 4) / 8, Clip(N * 3 - NN * 3 + NNN) >> 1));
            cm.set(hash(++i, (WWW + W + 4) / 8, Clip(W * 3 - WW * 3 + WWW) >> 1));
        } else {
            ctx = min(0x1F, (blpos % w) / max(1, w / 32));

            cm.set(hash(++i, W, NEE));
            cm.set(hash(++i, WW, NN));
            cm.set(hash(++i, W, WWW));
            cm.set(hash(++i, N, NNN));
            cm.set(hash(++i, NNW, NN));
        }

        scm1.set((buf(1) + buf(w)) >> 1);
        scm2.set((buf(1) + buf(w) - buf(w + 1)) >> 1);
        scm3.set((buf(1) * 2 - buf(2)) >> 1);
        scm4.set((buf(w) * 2 - buf(w * 2)) >> 1);
        scm5.set((buf(1) + buf(w) - buf(w - 1)) >> 1);
        scm6.set(mean >> 1 | (logvar << 1 & 0x180));
    }

    // Predict next bit
    scm1.mix(m);
    scm2.mix(m);
    scm3.mix(m);
    scm4.mix(m);
    scm5.mix(m);
    scm6.mix(m);
    scm7.mix(m); // Amazingly but improves compression!
    cm.mix(m);
    if(++col >= 8) { col = 0; } // reset after every 24 columns?
    m.set((gray) ? ctx : ctx | ((bpos > 4) << 8), 512);
    m.set(col, 8);
    m.set((buf(w) + buf(1)) >> 4, 32);
    m.set(c0, 256);
}



#endif //PAQ8PX_IM8BITMODEL_H
