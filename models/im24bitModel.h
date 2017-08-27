#ifndef PAQ8PX_IM24BITMODEL_H
#define PAQ8PX_IM24BITMODEL_H

#include "../utils.h"
#include "../ContextMap.h"
#include "../Mixer.h"

// Model for 24/32-bit image data
void im24bitModel(Mixer &m, int w, int alpha = 0) {
    const int SC = 0x20000;
    static SmallStationaryContextMap scm1(SC), scm2(SC),
            scm3(SC), scm4(SC), scm5(SC), scm6(SC), scm7(SC), scm8(SC), scm9(SC * 2), scm10(512);
    static ContextMap cm(MEM * 4, 13 + 7);
    static int color = -1;
    static int stride = 3;
    static int ctx, padding, lastPos, x = 0;

    // Select nearby pixels as context
    if(!bpos) {
        assert(w > 3 + alpha);
        if((color < 0) || (pos - lastPos != 1)) {
            stride = 3 + alpha;
            padding = w % stride;
            x = 0;
        }
        lastPos = pos;
        x *= (++x) < w;
        if(x + padding < w) {
            color *= (++color) < stride;
        } else {
            color = (padding) * (stride + 1);
        }

        int mean = buf(stride) + buf(w - stride) + buf(w) + buf(w + stride);
        const int var = (sqrbuf(stride) + sqrbuf(w - stride) + sqrbuf(w) + sqrbuf(w + stride) - mean * mean / 4) >> 2;
        mean >>= 2;
        const int logvar = ilog(var);
        int i = color << 5;

        int WWW = buf(3 * stride), WW = buf(2 * stride), W = buf(stride), NW = buf(w + stride), N = buf(w), NE = buf(
                w - stride), NNW = buf(w * 2 + stride), NN = buf(w * 2), NNE = buf(w * 2 - stride), NNN = buf(w * 3);
        ctx = (min(color, stride) << 9) | ((abs(W - N) > 8) << 8) | ((W > N) << 7) | ((W > NW) << 6) |
              ((abs(N - NW) > 8) << 5) | ((N > NW) << 4) | ((abs(N - NE) > 8) << 3) | ((N > NE) << 2) |
              ((W > WW) << 1) | (N > NN);
        cm.set(hash((N + 1) >> 1, LogMeanDiffQt(N, Clip(NN * 2 - NNN))));
        cm.set(hash((W + 1) >> 1, LogMeanDiffQt(W, Clip(WW * 2 - WWW))));
        cm.set(hash(Clamp4(W + N - NW, W, NW, N, NE), LogMeanDiffQt(Clip(N + NE - NNE), Clip(N + NW - NNW))));
        cm.set(hash((NNN + N + 4) / 8, Clip(N * 3 - NN * 3 + NNN) >> 1));
        cm.set(hash((WWW + W + 4) / 8, Clip(W * 3 - WW * 3 + WWW) >> 1));
        cm.set(hash(++i, (W + Clip(NE * 3 - NNE * 3 + buf(w * 3 - stride))) / 4));
        cm.set(hash(++i, Clip((-buf(4 * stride) + 5 * WWW - 10 * WW + 10 * W +
                               Clamp4(NE * 4 - NNE * 6 + buf(w * 3 - stride) * 4 - buf(w * 4 - stride), N, NE,
                                      buf(w - 2 * stride), buf(w - 3 * stride))) / 5) / 4));

        cm.set(hash(++i, buf(stride)));
        cm.set(hash(++i, buf(stride), buf(1)));
        cm.set(hash(++i, buf(stride), buf(1), buf(2)));
        cm.set(hash(++i, buf(w)));
        cm.set(hash(++i, buf(w), buf(1)));
        cm.set(hash(++i, buf(w), buf(1), buf(2)));
        cm.set(hash(++i, (buf(stride) + buf(w)) >> 3, buf(1) >> 4, buf(2) >> 4));
        cm.set(hash(++i, buf(1), buf(2)));
        cm.set(hash(++i, buf(stride), buf(1) - buf(4)));
        cm.set(hash(++i, buf(stride) + buf(1) - buf(4)));
        cm.set(hash(++i, buf(w), buf(1) - buf(w + 1)));
        cm.set(hash(++i, buf(w) + buf(1) - buf(w + 1)));
        cm.set(hash(++i, mean, logvar >> 4));
        scm1.set(buf(stride) + buf(w) - buf(w + stride));
        scm2.set(buf(stride) + buf(w - stride) - buf(w));
        scm3.set(buf(stride) * 2 - buf(stride * 2));
        scm4.set(buf(w) * 2 - buf(w * 2));
        scm5.set(buf(w + stride) * 2 - buf(w * 2 + stride * 2));
        scm6.set(buf(w - stride) * 2 - buf(w * 2 - stride * 2));
        scm7.set(buf(w - stride) + buf(1) - buf(w - 2));
        scm8.set(buf(w) + buf(w - stride) - buf(w * 2 - stride));
        scm9.set(mean >> 1 | (logvar << 1 & 0x180));
    }

    // Predict next bit
    scm1.mix(m);
    scm2.mix(m);
    scm3.mix(m);
    scm4.mix(m);
    scm5.mix(m);
    scm6.mix(m);
    scm7.mix(m);
    scm8.mix(m);
    scm9.mix(m);
    scm10.mix(m);
    cm.mix(m);
    static int col = 0;
    if(++col >= stride * 8) { col = 0; }
    m.set(ctx, 2048);
    m.set(col, stride * 8);
    m.set((buf(1 + (alpha && !color)) >> 4) * stride + (x % stride), stride * 16);
    m.set(c0, 256);
}

#endif //PAQ8PX_IM24BITMODEL_H
