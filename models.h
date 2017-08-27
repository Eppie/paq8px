#ifndef PAQ8PX_MODELS_H
#define PAQ8PX_MODELS_H

#include <cmath>
#include "StateTable.h"
#include "utils.h"
#include "Ilog.h"
#include "Stretch.h"
#include "StateMap.h"
#include "Mixer.h"
#include "ContextMap.h"

inline U8 Clip(int Px) {
    return min(0xFF, max(0, Px));
}

inline U8 Clamp4(int Px, U8 n1, U8 n2, U8 n3, U8 n4) {
    return min(max(n1, max(n2, max(n3, n4))), max(min(n1, min(n2, min(n3, n4))), Px));
}

inline U8 LogMeanDiffQt(U8 a, U8 b) {
    return (a != b) ? ((a > b) << 3) | ilog2((a + b) / max(2, abs(a - b) * 2) + 1) : 0;
}

// Square buf(i)
inline int sqrbuf(int i) {
    assert(i > 0);
    return buf(i) * buf(i);
}


// matchModel() finds the longest matching context and returns its length
int matchModel(Mixer &m) {
    const int MAXLEN = 65534;  // longest allowed match + 1
    static Array<int> t(MEM);  // hash table of pointers to contexts
    static int h = 0;  // hash of last 7 bytes
    static int ptr = 0;  // points to next byte of match if any
    static int len = 0;  // length of match, or 0 if no match
    static int result = 0;

    static SmallStationaryContextMap scm1(0x20000);

    if(!bpos) {
        h = (h * 997 * 8 + buf(1) + 1) & (t.size() - 1);  // update context hash
        if(len) { ++len, ++ptr; }
        else {  // find match
            ptr = t[h];
            if(ptr && pos - ptr < buf.size()) {
                while(buf(len + 1) == buf[ptr - len - 1] && len < MAXLEN) { ++len; }
            }
        }
        t[h] = pos;  // update hash table
        result = len;
//    if (result>0 && !(result&0xfff)) printf("pos=%d len=%d ptr=%d\n", pos, len, ptr);
        scm1.set(pos);
    }

    // predict
    if(len) {
        if(buf(1) == buf[ptr - 1] && c0 == (buf[ptr] + 256) >> (8 - bpos)) {
            if(len > MAXLEN) { len = MAXLEN; }
            if(buf[ptr] >> (7 - bpos) & 1) {
                m.add(ilog(len) << 2);
                m.add(min(len, 32) << 6);
            } else {
                m.add(-(ilog(len) << 2));
                m.add(-(min(len, 32) << 6));
            }
        } else {
            len = 0;
            m.add(0);
            m.add(0);
        }
    } else {
        m.add(0);
        m.add(0);
    }

    scm1.mix(m);
    return result;
}

void im1bitModel(Mixer &m, int w) {
    static U32 r0, r1, r2, r3;  // last 4 rows, bit 8 is over current pixel
    static Array<U8> t(0x10200);  // model: cxt -> state
    const int N = 4 + 1 + 1 + 1 + 1 + 1;  // number of contexts
    static int cxt[N];  // contexts
    static StateMap sm[N];

    // update the model
    int i;
    for(i = 0; i < N; ++i) {
        t[cxt[i]] = nex(t[cxt[i]], y);
    }

    // update the contexts (pixels surrounding the predicted one)
    r0 += r0 + y;
    r1 += r1 + ((buf(w - 1) >> (7 - bpos)) & 1);
    r2 += r2 + ((buf(w + w - 1) >> (7 - bpos)) & 1);
    r3 += r3 + ((buf(w + w + w - 1) >> (7 - bpos)) & 1);
    cxt[0] = (r0 & 0x7) | (r1 >> 4 & 0x38) | (r2 >> 3 & 0xc0);
    cxt[1] = 0x100 + ((r0 & 1) | (r1 >> 4 & 0x3e) | (r2 >> 2 & 0x40) | (r3 >> 1 & 0x80));
    cxt[2] = 0x200 + ((r0 & 0x3f) ^ (r1 & 0x3ffe) ^ (r2 << 2 & 0x7f00) ^ (r3 << 5 & 0xf800));
    cxt[3] = 0x400 + ((r0 & 0x3e) ^ (r1 & 0x0c0c) ^ (r2 & 0xc800));
    cxt[4] = 0x800 + (((r1 & 0x30) ^ (r3 & 0x0c0c)) | (r0 & 3));
    cxt[5] = 0x1000 + ((!r0 & 0x444) | (r1 & 0xC0C) | (r2 & 0xAE3) | (r3 & 0x51C));
    cxt[6] = 0x2000 + ((r0 & 1) | (r1 >> 4 & 0x1d) | (r2 >> 1 & 0x60) | (r3 & 0xC0));
    cxt[7] = 0x4000 + ((r0 >> 4 & 0x2AC) | (r1 & 0xA4) | (r2 & 0x349) | (!r3 & 0x14D));

    // predict
    for(i = 0; i < N; ++i) {
        m.add(stretch(sm[i].p(t[cxt[i]])));
    }
}

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

// Model 2-D data with fixed record length.  Also order 1-2 models
// that include the distance to the last match.

#define SPACE 0x20

void recordModel(Mixer &m) {
    static int cpos1[256], cpos2[256], cpos3[256], cpos4[256];
    static int wpos1[0x10000]; // buf(1..2) -> last position
    static int rlen[3] = {2, 3, 4}; // run length and 2 candidates
    static int rcount[2] = {0, 0}; // candidate counts
    static U8 padding = 0; // detected padding byte
    static int prevTransition = 0, col = 0, mxCtx = 0; // position of the last padding transition
    static ContextMap cm(32768, 3), cn(32768 / 2, 3), co(32768 * 2, 3), cp(MEM, 6);

    // Find record length
    if(!bpos) {
        int w = c4 & 0xffff, c = w & 255, d = w >> 8;
        int r = pos - cpos1[c];
        if(r > 1 && r == cpos1[c] - cpos2[c]
           && r == cpos2[c] - cpos3[c] && (r > 32 || r == cpos3[c] - cpos4[c])
           && (r > 10 || ((c == buf(r * 5 + 1)) && c == buf(r * 6 + 1)))) {
            if(r == rlen[1]) { ++rcount[0]; }
            else if(r == rlen[2]) { ++rcount[1]; }
            else if(rcount[0] > rcount[1]) { rlen[2] = r, rcount[1] = 1; }
            else { rlen[1] = r, rcount[0] = 1; }
        }

        // check candidate lengths
        for(int i = 0; i < 2; i++) {
            if(rcount[i] > max(0, 12 - (int) ilog2(rlen[i + 1]))) {
                if(rlen[0] != rlen[i + 1]) {
                    if((rlen[i + 1] > rlen[0]) && (rlen[i + 1] % rlen[0] == 0)) {
                        // maybe we found a multiple of the real record size..?
                        // in that case, it is probably an immediate multiple (2x).
                        // that is probably more likely the bigger the length, so
                        // check for small lengths too
                        if((rlen[0] > 32) && (rlen[i + 1] == rlen[0] * 2)) {
                            rcount[0] >>= 1;
                            rcount[1] >>= 1;
                            continue;
                        }
                    }
                    rlen[0] = rlen[i + 1];
                    rcount[i] = 0;
                } else {
                    // we found the same length again, that's positive reinforcement that
                    // this really is the correct record size, so give it a little boost
                    rcount[i] >>= 2;
                }

                // if the other candidate record length is orders of
                // magnitude larger, it will probably never have enough time
                // to increase its counter before it's reset again. and if
                // this length is not a multiple of the other, than it might
                // really be worthwhile to investigate it, so we won't set its
                // counter to 0
                if(rlen[i + 1] << 4 > rlen[1 + (i ^ 1)]) {
                    rcount[i ^ 1] = 0;
                }
            }
        }

        // Set 2 dimensional contexts
        assert(rlen[0] > 0);
        cm.set(c << 8 | (min(255, pos - cpos1[c]) / 4));
        cm.set(w << 9 | llog(pos - wpos1[w]) >> 2);
        cm.set(rlen[0] | buf(rlen[0]) << 10 | buf(rlen[0] * 2) << 18);

        cn.set(w | rlen[0] << 8);
        cn.set(d | rlen[0] << 16);
        cn.set(c | rlen[0] << 8);

        co.set(buf(1) << 8 | min(255, pos - cpos1[buf(1)]));
        co.set(buf(1) << 17 | buf(2) << 9 | llog(pos - wpos1[w]) >> 2);
        co.set(buf(1) << 8 | buf(rlen[0]));

        col = pos % rlen[0];
        cp.set(rlen[0] | buf(rlen[0]) << 10 | col << 18);
        cp.set(rlen[0] | buf(1) << 10 | col << 18);
        cp.set(col | rlen[0] << 12);

        /*
        Consider record structures that include fixed-length strings.
        These usually contain the text followed by either spaces or 0's,
        depending on whether they're to be trimmed or they're null-terminated.
        That means we can guess the length of the string field by looking
        for small repetitions of one of these padding bytes followed by a
        different byte. By storing the last position where this transition
        ocurred, and what was the padding byte, we are able to model these
        runs of padding bytes.
        Special care is taken to skip record structures of less than 9 bytes,
        since those may be little-endian 64 bit integers. If they contain
        relatively low values (<2^40), we may consistently get runs of 3 or
        even more 0's at the end of each record, and so we could assume that
        to be the general case. But with integers, we can't be reasonably sure
        that a number won't have 3 or more 0's just before a final non-zero MSB.
        And with such simple structures, there's probably no need to be fancy
        anyway
        */

        if((((U16) (c4 >> 8) == ((SPACE << 8) + SPACE)) && (c != SPACE)) ||
           (!(c4 >> 8) && c && ((padding != SPACE) || (pos - prevTransition > rlen[0])))) {
            prevTransition = pos;
            padding = (U8) d;
        }
        cp.set((rlen[0] > 8) * hash(min(min(0xFF, rlen[0]), pos - prevTransition), min(0x3FF, col),
                                    (w & 0xF0F0) | (w == ((padding << 8) | padding))));

        int last4 = (buf(rlen[0] * 4) << 24) | (buf(rlen[0] * 3) << 16) | (buf(rlen[0] * 2) << 8) | buf(rlen[0]);
        cp.set((last4 & 0xFF) | ((last4 & 0xF000) >> 4) | ((last4 & 0xE00000) >> 9) | ((last4 & 0xE0000000) >> 14) |
               ((col / max(1, rlen[0] / 16)) << 18));
        cp.set((last4 & 0xF8F8) | (col << 16));

        // update last context positions
        cpos4[c] = cpos3[c];
        cpos3[c] = cpos2[c];
        cpos2[c] = cpos1[c];
        cpos1[c] = pos;
        wpos1[w] = pos;

        mxCtx = (rlen[0] > 128) ? (min(0x7F, col / max(1, rlen[0] / 128))) : col;
    }
    cm.mix(m);
    cn.mix(m);
    co.mix(m);
    cp.mix(m);

    if(level > 7) {
        m.set((rlen[0] > 2) * ((bpos << 7) | mxCtx), 1024);
    }
}


// Model a 16/8-bit stereo/mono uncompressed .wav file.
// Based on 'An asymptotically Optimal Predictor for Stereo Lossless Audio Compression'
// by Florin Ghido.

static int S, D;
static int wmode;

inline int s2(int i) { return int(short(buf(i) + 256 * buf(i - 1))); }

inline int t2(int i) { return int(short(buf(i - 1) + 256 * buf(i))); }

inline int X1(int i) {
    switch(wmode) {
        case 0:
            return buf(i) - 128;
        case 1:
            return buf(i << 1) - 128;
        case 2:
            return s2(i << 1);
        case 3:
            return s2(i << 2);
        case 4:
            return (buf(i) ^ 128) - 128;
        case 5:
            return (buf(i << 1) ^ 128) - 128;
        case 6:
            return t2(i << 1);
        case 7:
            return t2(i << 2);
        default:
            return 0;
    }
}

inline int X2(int i) {
    switch(wmode) {
        case 0:
            return buf(i + S) - 128;
        case 1:
            return buf((i << 1) - 1) - 128;
        case 2:
            return s2((i + S) << 1);
        case 3:
            return s2((i << 2) - 2);
        case 4:
            return (buf(i + S) ^ 128) - 128;
        case 5:
            return (buf((i << 1) - 1) ^ 128) - 128;
        case 6:
            return t2((i + S) << 1);
        case 7:
            return t2((i << 2) - 2);
        default:
            return 0;
    }
}

void wavModel(Mixer &m, int info) {
    static int pr[3][2], n[2], counter[2];
    static double F[49][49][2], L[49][49];
    int j, k, l, i = 0;
    long double sum;
    const double a = 0.996, a2 = 1 / a;
    const int SC = 0x20000;
    static SmallStationaryContextMap scm1(SC), scm2(SC), scm3(SC), scm4(SC), scm5(SC), scm6(SC), scm7(SC);
    static ContextMap cm(MEM * 4, 10 + 1);
    static int bits, channels, w, ch;
    static int z1, z2, z3, z4, z5, z6, z7;

    if(!bpos && !blpos) {
        bits = ((info % 4) / 2) * 8 + 8;
        channels = info % 2 + 1;
        w = channels * (bits >> 3);
        wmode = info;
        if(channels == 1) { S = 48, D = 0; } else { S = 36, D = 12; }
        for(int j = 0; j < channels; j++) {
            for(k = 0; k <= S + D; k++) { for(l = 0; l <= S + D; l++) { F[k][l][j] = 0, L[k][l] = 0; }}
            F[1][0][j] = 1;
            n[j] = counter[j] = pr[2][j] = pr[1][j] = pr[0][j] = 0;
            z1 = z2 = z3 = z4 = z5 = z6 = z7 = 0;
        }
    }
    // Select previous samples and predicted sample as context
    if(!bpos && blpos >= w) {
        /*const int*/ ch = blpos % w;
        const int msb = ch % (bits >> 3);
        const int chn = ch / (bits >> 3);
        if(!msb) {
            z1 = X1(1), z2 = X1(2), z3 = X1(3), z4 = X1(4), z5 = X1(5);
            k = X1(1);
            for(l = 0; l <= min(S, counter[chn] - 1); l++) {
                F[0][l][chn] *= a;
                F[0][l][chn] += X1(l + 1) * k;
            }
            for(l = 1; l <= min(D, counter[chn]); l++) {
                F[0][l + S][chn] *= a;
                F[0][l + S][chn] += X2(l + 1) * k;
            }
            if(channels == 2) {
                k = X2(2);
                for(l = 1; l <= min(D, counter[chn]); l++) {
                    F[S + 1][l + S][chn] *= a;
                    F[S + 1][l + S][chn] += X2(l + 1) * k;
                }
                for(l = 1; l <= min(S, counter[chn] - 1); l++) {
                    F[l][S + 1][chn] *= a;
                    F[l][S + 1][chn] += X1(l + 1) * k;
                }
                z6 = X2(1) + X1(1) - X2(2), z7 = X2(1);
            } else { z6 = 2 * X1(1) - X1(2), z7 = X1(1); }
            if(++n[chn] == (256 >> level)) {
                if(channels == 1) {
                    for(k = 1; k <= S + D; k++) {
                        for(l = k; l <= S + D; l++) {
                            F[k][l][chn] = (F[k - 1][l - 1][chn] - X1(k) * X1(l)) * a2;
                        }
                    }
                } else {
                    for(k = 1; k <= S + D; k++) {
                        if(k != S + 1) {
                            for(l = k; l <= S + D; l++) {
                                if(l != S + 1) {
                                    F[k][l][chn] = (F[k - 1][l - 1][chn] - (k - 1 <= S ? X1(k) : X2(k - S)) *
                                                                           (l - 1 <= S ? X1(l) : X2(l - S))) * a2;
                                }
                            }
                        }
                    }
                }
                for(i = 1; i <= S + D; i++) {
                    sum = F[i][i][chn];
                    for(k = 1; k < i; k++) { sum -= L[i][k] * L[i][k]; }
                    sum = floor(sum + 0.5);
                    sum = 1 / sum;
                    if(sum > 0) {
                        L[i][i] = sqrt(sum);
                        for(j = (i + 1); j <= S + D; j++) {
                            sum = F[i][j][chn];
                            for(k = 1; k < i; k++) { sum -= L[j][k] * L[i][k]; }
                            sum = floor(sum + 0.5);
                            L[j][i] = sum * L[i][i];
                        }
                    } else { break; }
                }
                if(i > S + D && counter[chn] > S + 1) {
                    for(k = 1; k <= S + D; k++) {
                        F[k][0][chn] = F[0][k][chn];
                        for(j = 1; j < k; j++) { F[k][0][chn] -= L[k][j] * F[j][0][chn]; }
                        F[k][0][chn] *= L[k][k];
                    }
                    for(k = S + D; k > 0; k--) {
                        for(j = k + 1; j <= S + D; j++) { F[k][0][chn] -= L[j][k] * F[j][0][chn]; }
                        F[k][0][chn] *= L[k][k];
                    }
                }
                n[chn] = 0;
            }
            sum = 0;
            for(l = 1; l <= S + D; l++) { sum += F[l][0][chn] * (l <= S ? X1(l) : X2(l - S)); }
            pr[2][chn] = pr[1][chn];
            pr[1][chn] = pr[0][chn];
            pr[0][chn] = int(floor(sum));
            counter[chn]++;
        }
        const int y1 = pr[0][chn], y2 = pr[1][chn], y3 = pr[2][chn];
        int x1 = buf(1), x2 = buf(2), x3 = buf(3);
        if(wmode == 4 || wmode == 5) { x1 ^= 128, x2 ^= 128; }
        if(bits == 8) { x1 -= 128, x2 -= 128; }
        const int t = ((bits == 8) || ((!msb) ^ (wmode < 6)));
        i = ch << 4;
        if((msb) ^ (wmode < 6)) {
            cm.set(hash(++i, y1 & 0xff));
            cm.set(hash(++i, y1 & 0xff, ((z1 - y2 + z2 - y3) >> 1) & 0xff));
            cm.set(hash(++i, x1, y1 & 0xff));
            cm.set(hash(++i, x1, x2 >> 3, x3));
            if(bits == 8) {
                cm.set(hash(++i, y1 & 0xFE, ilog2(abs((int) (z1 - y2))) * 2 + (z1 > y2)));
            } else {
                cm.set(hash(++i, (y1 + z1 - y2) & 0xff));
            }
            cm.set(hash(++i, x1));
            cm.set(hash(++i, x1, x2));
            cm.set(hash(++i, z1 & 0xff));
            cm.set(hash(++i, (z1 * 2 - z2) & 0xff));
            cm.set(hash(++i, z6 & 0xff));
            cm.set(hash(++i, y1 & 0xFF, ((z1 - y2 + z2 - y3) / (bits >> 3)) & 0xFF));
        } else {
            cm.set(hash(++i, (y1 - x1 + z1 - y2) >> 8));
            cm.set(hash(++i, (y1 - x1) >> 8));
            cm.set(hash(++i, (y1 - x1 + z1 * 2 - y2 * 2 - z2 + y3) >> 8));
            cm.set(hash(++i, (y1 - x1) >> 8, (z1 - y2 + z2 - y3) >> 9));
            cm.set(hash(++i, z1 >> 12));
            cm.set(hash(++i, x1));
            cm.set(hash(++i, x1 >> 7, x2, x3 >> 7));
            cm.set(hash(++i, z1 >> 8));
            cm.set(hash(++i, (z1 * 2 - z2) >> 8));
            cm.set(hash(++i, y1 >> 8));
            cm.set(hash(++i, (y1 - x1) >> 6));
        }
        scm1.set(t * ch);
        scm2.set(t * ((z1 - x1 + y1) >> 9) & 0xff);
        scm3.set(t * ((z1 * 2 - z2 - x1 + y1) >> 8) & 0xff);
        scm4.set(t * ((z1 * 3 - z2 * 3 + z3 - x1) >> 7) & 0xff);
        scm5.set(t * ((z1 + z7 - x1 + y1 * 2) >> 10) & 0xff);
        scm6.set(t * ((z1 * 4 - z2 * 6 + z3 * 4 - z4 - x1) >> 7) & 0xff);
        scm7.set(t * ((z1 * 5 - z2 * 10 + z3 * 10 - z4 * 5 + z5 - x1 + y1) >> 9) & 0xff);
    }

    // Predict next bit
    scm1.mix(m);
    scm2.mix(m);
    scm3.mix(m);
    scm4.mix(m);
    scm5.mix(m);
    scm6.mix(m);
    scm7.mix(m);
    cm.mix(m);
    if(level >= 4) { recordModel(m); }
    static int col = 0;
    if(++col >= w * 8) { col = 0; }
    //m.set(3, 8);
    m.set(ch + 4 * ilog2(col & (bits - 1)), 4 * 8);
    m.set(col % bits < 8, 2);
    m.set(col % bits, bits);
    m.set(col, w * 8);
    m.set(c0, 256);
}

#include "jpegmodel.h"

// Model order 1-2 contexts with gaps.

void sparseModel(Mixer &m, int seenbefore, int howmany) {
    static ContextMap cm(MEM * 2, 42);
    if(bpos == 0) {
        cm.set(seenbefore);
        cm.set(howmany);
        cm.set(buf(1) | buf(5) << 8);
        cm.set(buf(1) | buf(6) << 8);
        cm.set(buf(3) | buf(6) << 8);
        cm.set(buf(4) | buf(8) << 8);
        cm.set(buf(1) | buf(3) << 8 | buf(5) << 16);
        cm.set(buf(2) | buf(4) << 8 | buf(6) << 16);
        cm.set(c4 & 0x00f0f0ff);
        cm.set(c4 & 0x00ff00ff);
        cm.set(c4 & 0xff0000ff);
        cm.set(c4 & 0x00f8f8f8);
        cm.set(c4 & 0xf8f8f8f8);
        cm.set(f4 & 0x00000fff);
        cm.set(f4);
        cm.set(c4 & 0x00e0e0e0);
        cm.set(c4 & 0xe0e0e0e0);
        cm.set(c4 & 0x810000c1);
        cm.set(c4 & 0xC3CCC38C);
        cm.set(c4 & 0x0081CC81);
        cm.set(c4 & 0x00c10081);
        for(int i = 1; i < 8; ++i) {
            cm.set(seenbefore | buf(i) << 8);
            cm.set((buf(i + 2) << 8) | buf(i + 1));
            cm.set((buf(i + 3) << 8) | buf(i + 1));
        }
    }
    cm.mix(m);
}

// Model for modelling distances between symbols

void distanceModel(Mixer &m) {
    static ContextMap cr(MEM, 3);
    if(bpos == 0) {
        static int pos00 = 0, pos20 = 0, posnl = 0;
        int c = c4 & 0xff;
        if(c == 0x00) { pos00 = pos; }
        if(c == 0x20) { pos20 = pos; }
        if(c == 0xff || c == '\r' || c == '\n') { posnl = pos; }
        cr.set(min(pos - pos00, 255) | (c << 8));
        cr.set(min(pos - pos20, 255) | (c << 8));
        cr.set(min(pos - posnl, 255) | ((c << 8) + 234567));
    }
    cr.mix(m);
}

// Model English text (words and columns/end of line)
static U32 frstchar = 0, spafdo = 0, spaces = 0, spacecount = 0, words = 0, wordcount = 0, wordlen = 0, wordlen1 = 0;

void wordModel(Mixer &m, Filetype filetype) {
    static U32 word0 = 0, word1 = 0, word2 = 0, word3 = 0, word4 = 0, word5 = 0;  // hashes
    static U32 xword0 = 0, xword1 = 0, xword2 = 0, cword0 = 0, ccword = 0;
    static U32 number0 = 0, number1 = 0;  // hashes
    static U32 text0 = 0;  // hash stream of letters
    static U32 lastLetter = 0, lastUpper = 0, wordGap = 0;
    static ContextMap cm(MEM * 16, 44 + 1);
    static int nl1 = -3, nl = -2, w = 0;  // previous, current newline position
    static U32 mask = 0;
    static Array<int> wpos(0x10000);  // last position of word

    // Update word hashes
    if(bpos == 0) {
        int c = c4 & 255, f = 0;
        if(spaces & 0x80000000) { --spacecount; }
        if(words & 0x80000000) { --wordcount; }
        spaces = spaces * 2;
        words = words * 2;
        lastUpper = min(lastUpper + 1, 63);
        lastLetter = min(lastLetter + 1, 63);
        if(c >= 'A' && c <= 'Z') { c += 'a' - 'A', lastUpper = 0; }
        if((c >= 'a' && c <= 'z') || c == 1 || c == 2 || (c >= 128 && (b2 != 3))) {
            if(!wordlen) {
                wordGap = lastLetter;
            }
            lastLetter = 0;
            ++words, ++wordcount;
            word0 ^= hash(word0, c, 0);
            text0 = text0 * 997 * 16 + c;
            wordlen++;
            wordlen = min(wordlen, 45);
            f = 0;
            w = word0 & (wpos.size() - 1);
        } else {
            if(word0) {
                word5 = word4;
                word4 = word3;
                word3 = word2;
                word2 = word1;
                word1 = word0;
                wordlen1 = wordlen;
                wpos[w] = blpos;
                if(c == ':' || c == '=') { cword0 = word0; }
                if(c == ']' && (frstchar != ':' || frstchar != '*')) { xword0 = word0; }
                ccword = 0;
                word0 = wordlen = 0;
                if((c == '.' || c == '!' || c == '?' || c == '}' || c == ')') && buf(2) != 10 &&
                   filetype != EXE) { f = 1; }
            }
            if((c4 & 0xFFFF) == 0x3D3D) { xword1 = word1, xword2 = word2; } // ==
            if((c4 & 0xFFFF) == 0x2727) { xword1 = word1, xword2 = word2; } // ''
            if(c == 32 || c == 10) {
                ++spaces, ++spacecount;
                if(c == 10) { nl1 = nl, nl = pos - 1; }
            } else if(c == '.' || c == '!' || c == '?' || c == ',' || c == ';' || c == ':') { spafdo = 0, ccword = c; }
            else {
                ++spafdo;
                spafdo = min(63, spafdo);
            }
        }
        if(c >= '0' && c <= '9') {
            number0 ^= hash(number0, c, 1);
        } else if(number0) {
            number1 = number0;
            number0 = 0, ccword = 0;
        }

        int col = min(255, pos - nl);
        int above = buf[nl1 + col];
        if(col <= 2) { frstchar = (col == 2 ? min(c, 96) : 0); }
        if(frstchar == '[' && c == 32) { if(buf(3) == ']' || buf(4) == ']') { frstchar = 96, xword0 = 0; }}
        cm.set(hash(513, spafdo, spaces, ccword));
        cm.set(hash(514, frstchar, c));
        cm.set(hash(515, col, frstchar));
        cm.set(hash(516, spaces, (words & 255)));
        cm.set(hash(256, number0, word2));
        cm.set(hash(257, number0, word1));
        cm.set(hash(258, number1, c, ccword));
        cm.set(hash(259, number0, number1));
        cm.set(hash(260, word0, number1));
        cm.set(hash(518, wordlen1, col));
        cm.set(hash(519, c, spacecount / 2));
        U32 h = wordcount * 64 + spacecount;
        cm.set(hash(520, c, h, ccword));
        cm.set(hash(517, frstchar, h));
        cm.set(hash(521, h, spafdo));

        U32 d = c4 & 0xf0ff;
        cm.set(hash(522, d, frstchar, ccword));
        h = word0 * 271;
        h = h + buf(1);
        cm.set(hash(262, h, 0));
        cm.set(hash(263, word0, 0));
        cm.set(hash(264, h, word1));
        cm.set(hash(265, word0, word1));
        cm.set(hash(266, h, word1, word2));
        cm.set(hash(268, text0 & 0xfffff, 0));
        cm.set(hash(269, word0, xword0));
        cm.set(hash(270, word0, xword1));
        cm.set(hash(271, word0, xword2));
        cm.set(hash(272, frstchar, xword2));
        cm.set(hash(273, word0, cword0));
        cm.set(hash(274, number0, cword0));
        cm.set(hash(275, h, word2));
        cm.set(hash(276, h, word3));
        cm.set(hash(277, h, word4));
        cm.set(hash(278, h, word5));
        cm.set(hash(279, h, word1, word3));
        cm.set(hash(280, h, word2, word3));
        if(f) {
            word5 = word4;
            word4 = word3;
            word3 = word2;
            word2 = word1;
            word1 = '.';
        }
        cm.set(hash(523, col, buf(1), above));
        cm.set(hash(524, buf(1), above));
        cm.set(hash(525, col, buf(1)));
        cm.set(hash(526, col, c == 32));
        cm.set(hash(281, w, llog(blpos - wpos[w]) >> 4));
        cm.set(hash(282, buf(1), llog(blpos - wpos[w]) >> 2));

        int fl = 0;
        if((c4 & 0xff) != 0) {
            if(isalpha(c4 & 0xff)) { fl = 1; }
            else if(ispunct(c4 & 0xff)) { fl = 2; }
            else if(isspace(c4 & 0xff)) { fl = 3; }
            else if((c4 & 0xff) == 0xff) { fl = 4; }
            else if((c4 & 0xff) < 16) { fl = 5; }
            else if((c4 & 0xff) < 64) { fl = 6; }
            else { fl = 7; }
        }
        mask = (mask << 3) | fl;
        cm.set(hash(528, mask, 0));
        cm.set(hash(529, mask, buf(1)));
        cm.set(hash(530, mask & 0xff, col));
        cm.set(hash(531, mask, buf(2), buf(3)));
        cm.set(hash(532, mask & 0x1ff, f4 & 0x00fff0));

        cm.set(hash(h, llog(wordGap), mask & 0x1FF,
                    ((wordlen1 > 3) << 6) |
                    ((wordlen > 0) << 5) |
                    ((spafdo == wordlen + 2) << 4) |
                    ((spafdo == wordlen + wordlen1 + 3) << 3) |
                    ((spafdo >= lastLetter + wordlen1 + wordGap) << 2) |
                    ((lastUpper < lastLetter + wordlen1) << 1) |
                    (lastUpper < wordlen + wordlen1 + wordGap)
        ));
    }
    cm.mix(m);
}

// The context is a byte string history that occurs within a
// 1 or 2 byte context.

void indirectModel(Mixer &m) {
    static ContextMap cm(MEM, 12);
    static U32 t1[256];
    static U16 t2[0x10000];
    static U16 t3[0x8000];
    static U16 t4[0x8000];

    if(!bpos) {
        U32 d = c4 & 0xffff, c = d & 255, d2 = (buf(1) & 31) + 32 * (buf(2) & 31) + 1024 * (buf(3) & 31);
        U32 d3 = (buf(1) >> 3 & 31) + 32 * (buf(3) >> 3 & 31) + 1024 * (buf(4) >> 3 & 31);
        U32 &r1 = t1[d >> 8];
        r1 = r1 << 8 | c;
        U16 &r2 = t2[c4 >> 8 & 0xffff];
        r2 = r2 << 8 | c;
        U16 &r3 = t3[(buf(2) & 31) + 32 * (buf(3) & 31) + 1024 * (buf(4) & 31)];
        r3 = r3 << 8 | c;
        U16 &r4 = t4[(buf(2) >> 3 & 31) + 32 * (buf(4) >> 3 & 31) + 1024 * (buf(5) >> 3 & 31)];
        r4 = r4 << 8 | c;
        const U32 t = c | t1[c] << 8;
        const U32 t0 = d | t2[d] << 16;
        const U32 ta = d2 | t3[d2] << 16;
        const U32 tc = d3 | t4[d3] << 16;
        cm.set(t);
        cm.set(t0);
        cm.set(ta);
        cm.set(tc);
        cm.set(t & 0xff00);
        cm.set(t0 & 0xff0000);
        cm.set(ta & 0xff0000);
        cm.set(tc & 0xff0000);
        cm.set(t & 0xffff);
        cm.set(t0 & 0xffffff);
        cm.set(ta & 0xffffff);
        cm.set(tc & 0xffffff);
    }
    cm.mix(m);
}

// Model using DMC.  The bitwise context is represented by a state graph,
// initilaized to a bytewise order 1 model as in
// http://plg.uwaterloo.ca/~ftp/dmc/dmc.c but with the following difference:
// - It uses integer arithmetic.
// - The threshold for cloning a state increases as memory is used up.
// - Each state maintains both a 0,1 count and a bit history (as in a
//   context model).  The 0,1 count is best for stationary data, and the
//   bit history for nonstationary data.  The bit history is mapped to
//   a probability adaptively using a StateMap.  The two computed probabilities
//   are combined.
// - When memory is used up the state graph is reinitialized to a bytewise
//   order 1 context as in the original DMC.  However, the bit histories
//   are not cleared.

struct DMCNode {  // 12 bytes
    unsigned int nx[2];  // next pointers
    U8 state;  // bit history
    unsigned int c0:12, c1:12;  // counts * 256
};

void dmcModel(Mixer &m) {
    static int top = 0, curr = 0;  // allocated, current node
    static Array<DMCNode> t(MEM * 2);  // state graph
    static StateMap sm;
    static int threshold = 256;

    // clone next state
    if(top > 0 && top < t.size()) {
        int next = t[curr].nx[y];
        int n = y ? t[curr].c1 : t[curr].c0;
        int nn = t[next].c0 + t[next].c1;
        if(n >= threshold * 2 && nn - n >= threshold * 3) {
            int r = n * 4096 / nn;
            assert(r >= 0 && r <= 4096);
            t[next].c0 -= t[top].c0 = t[next].c0 * r >> 12;
            t[next].c1 -= t[top].c1 = t[next].c1 * r >> 12;
            t[top].nx[0] = t[next].nx[0];
            t[top].nx[1] = t[next].nx[1];
            t[top].state = t[next].state;
            t[curr].nx[y] = top;
            ++top;
            if(top == MEM * 2) { threshold = 512; }
            if(top == MEM * 3) { threshold = 768; }
        }
    }

    // Initialize to a bytewise order 1 model at startup or when flushing memory
    if(top == t.size() && bpos == 1) { top = 0; }
    if(top == 0) {
        assert(t.size() >= 65536);
        for(int i = 0; i < 256; ++i) {
            for(int j = 0; j < 256; ++j) {
                if(i < 127) {
                    t[j * 256 + i].nx[0] = j * 256 + i * 2 + 1;
                    t[j * 256 + i].nx[1] = j * 256 + i * 2 + 2;
                } else {
                    t[j * 256 + i].nx[0] = (i - 127) * 256;
                    t[j * 256 + i].nx[1] = (i + 1) * 256;
                }
                t[j * 256 + i].c0 = 128;
                t[j * 256 + i].c1 = 128;
            }
        }
        top = 65536;
        curr = 0;
        threshold = 256;
    }

    // update count, state
    if(y) {
        if(t[curr].c1 < 3840) { t[curr].c1 += 256; }
    } else if(t[curr].c0 < 3840) { t[curr].c0 += 256; }
    t[curr].state = nex(t[curr].state, y);
    curr = t[curr].nx[y];

    // predict
    const int pr1 = sm.p(t[curr].state);
    const int n1 = t[curr].c1;
    const int n0 = t[curr].c0;
    const int pr2 = (n1 + 5) * 4096 / (n0 + n1 + 10);
    m.add(stretch(pr1));
    m.add(stretch(pr2));
}

void nestModel(Mixer &m) {
    static int ic = 0, bc = 0, pc = 0, vc = 0, qc = 0, lvc = 0, wc = 0;
    static ContextMap cm(MEM, 10);
    if(bpos == 0) {
        int c = c4 & 255, matched = 1, vv;
        const int lc = (c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c);
        if(lc == 'a' || lc == 'e' || lc == 'i' || lc == 'o' || lc == 'u') { vv = 1; }
        else if(lc >= 'a' && lc <= 'z') { vv = 2; }
        else if(lc == ' ' || lc == '.' || lc == ',' || lc == '\n') { vv = 3; }
        else if(lc >= '0' && lc <= '9') { vv = 4; }
        else if(lc == 'y') { vv = 5; }
        else if(lc == '\'') { vv = 6; } else { vv = (c & 32) ? 7 : 0; }
        vc = (vc << 3) | vv;
        if(vv != lvc) {
            wc = (wc << 3) | vv;
            lvc = vv;
        }
        switch(c) {
            case ' ':
                qc = 0;
                break;
            case '(':
                ic += 513;
                break;
            case ')':
                ic -= 513;
                break;
            case '[':
                ic += 17;
                break;
            case ']':
                ic -= 17;
                break;
            case '<':
                ic += 23;
                qc += 34;
                break;
            case '>':
                ic -= 23;
                qc /= 5;
                break;
            case ':':
                pc = 20;
                break;
            case '{':
                ic += 22;
                break;
            case '}':
                ic -= 22;
                break;
            case '|':
                pc += 223;
                break;
            case '"':
                pc += 0x40;
                break;
            case '\'':
                pc += 0x42;
                break;
            case '\n':
                pc = qc = 0;
                break;
            case '.':
                pc = 0;
                break;
            case '!':
                pc = 0;
                break;
            case '?':
                pc = 0;
                break;
            case '#':
                pc += 0x08;
                break;
            case '%':
                pc += 0x76;
                break;
            case '$':
                pc += 0x45;
                break;
            case '*':
                pc += 0x35;
                break;
            case '-':
                pc += 0x3;
                break;
            case '@':
                pc += 0x72;
                break;
            case '&':
                qc += 0x12;
                break;
            case ';':
                qc /= 3;
                break;
            case '\\':
                pc += 0x29;
                break;
            case '/':
                pc += 0x11;
                if(buf.size() > 1 && buf(1) == '<') { qc += 74; }
                break;
            case '=':
                pc += 87;
                break;
            default:
                matched = 0;
        }
        if(matched) { bc = 0; } else { bc += 1; }
        if(bc > 300) { bc = ic = pc = qc = 0; }


        cm.set((3 * vc + 77 * pc + 373 * ic + qc) & 0xffff);
        cm.set((31 * vc + 27 * pc + 281 * qc) & 0xffff);
        cm.set((13 * vc + 271 * ic + qc + bc) & 0xffff);
        cm.set((17 * pc + 7 * ic) & 0xffff);
        cm.set((13 * vc + ic) & 0xffff);
        cm.set((vc / 3 + pc) & 0xffff);
        cm.set((7 * wc + qc) & 0xffff);
        cm.set((vc & 0xffff) | ((c4 & 0xff) << 16));
        cm.set(((3 * pc) & 0xffff) | ((c4 & 0xff) << 16));
        cm.set((ic & 0xffff) | ((c4 & 0xff) << 16));

    }
    cm.mix(m);
}

// Model x86 code.  The contexts are sparse containing only those
// bits relevant to parsing (2 prefixes, opcode, and mod and r/m fields
// of modR/M byte).

inline int pref(int i) { return (buf(i) == 0x0f) + 2 * (buf(i) == 0x66) + 3 * (buf(i) == 0x67); }

// Get context at buf(i) relevant to parsing 32-bit x86 code
U32 execxt(int i, int x = 0) {
    int prefix = 0, opcode = 0, modrm = 0;
    if(i) { prefix += 4 * pref(i--); }
    if(i) { prefix += pref(i--); }
    if(i) { opcode += buf(i--); }
    if(i) { modrm += buf(i) & 0xc7; }
    return prefix | opcode << 4 | modrm << 12 | x << 20;
}

void exeModel(Mixer &m) {
    const int N = 14;
    static ContextMap cm(MEM, N);
    if(!bpos) {
        for(int i = 0; i < N; ++i) { cm.set(execxt(i + 1, buf(1) * (i > 6))); }
    }
    cm.mix(m);
}


// This combines all the context models with a Mixer.
int contextModel2() {
    static ContextMap cm(MEM * 32, 9);
    static RunContextMap rcm7(MEM), rcm9(MEM), rcm10(MEM);
    static Mixer m(881 + 8 * 3, 3095 + 768 + 1024 * (level > 7), 7 + (level > 7));
    static U32 cxt[16];  // order 0-11 contexts
    static Filetype ft2, filetype = DEFAULT;
    static int size = 0;  // bytes remaining in block
    static int info = 0;  // image width or audio type

    // Parse filetype and size
    if(bpos == 0) {
        --size;
        ++blpos;
        if(size == -1) { ft2 = (Filetype) buf(1); }
        if(size == -5 && ft2 != IMAGE1 && ft2 != IMAGE8 && ft2 != IMAGE8GRAY && ft2 != IMAGE24 && ft2 != IMAGE32 &&
           ft2 != AUDIO) {
            size = buf(4) << 24 | buf(3) << 16 | buf(2) << 8 | buf(1);
            if(ft2 == CD || ft2 == ZLIB || ft2 == BASE64 || ft2 == GIF) { size = 0; }
            blpos = 0;
        }
        if(size == -9) {
            size = buf(8) << 24 | buf(7) << 16 | buf(6) << 8 | buf(5);
            info = buf(4) << 24 | buf(3) << 16 | buf(2) << 8 | buf(1);
            blpos = 0;
        }
        if(!blpos) { filetype = ft2; }
        if(size == 0) { filetype = DEFAULT; }
    }

    m.update();
    m.add(256);

    // Test for special file types
    int ismatch = ilog(matchModel(m));  // Length of longest matching context
    if(filetype == IMAGE1) { im1bitModel(m, info); }
    if(filetype == IMAGE8) { return im8bitModel(m, info), m.p(); }
    if(filetype == IMAGE8GRAY) { return im8bitModel(m, info, 1), m.p(); }
    if(filetype == IMAGE24) { return im24bitModel(m, info), m.p(); }
    if(filetype == IMAGE32) { return im24bitModel(m, info, 1), m.p(); }
    if(filetype == AUDIO) { return wavModel(m, info), m.p(); }
    if(filetype == JPEG) { if(jpegModel(m)) { return m.p(); }}

    // Normal model
    if(bpos == 0) {
        int i;
        for(i = 15; i > 0; --i) {  // update order 0-11 context hashes
            cxt[i] = cxt[i - 1] * 257 + (c4 & 255) + 1;
        }
        for(i = 0; i < 7; ++i) {
            cm.set(cxt[i]);
        }
        rcm7.set(cxt[7]);
        cm.set(cxt[8]);
        rcm9.set(cxt[10]);
        rcm10.set(cxt[12]);
        cm.set(cxt[14]);
    }
    int order = cm.mix(m);

    rcm7.mix(m);
    rcm9.mix(m);
    rcm10.mix(m);

    if(level >= 4 && filetype != IMAGE1) {
        sparseModel(m, ismatch, order);
        distanceModel(m);
        recordModel(m);
        wordModel(m, filetype);
        indirectModel(m);
        dmcModel(m);
        nestModel(m);
        if(filetype == EXE) { exeModel(m); }
    }


    order = order - 2;
    if(order < 0) { order = 0; }

    U32 c1 = buf(1), c2 = buf(2), c3 = buf(3), c;

    m.set(8 + c1 + (bpos > 5) * 256 + (((c0 & ((1 << bpos) - 1)) == 0) || (c0 == ((2 << bpos) - 1))) * 512, 8 + 1024);
    m.set(c0, 256);
    m.set(order + 8 * (c4 >> 6 & 3) + 32 * (bpos == 0) + 64 * (c1 == c2) + 128 * (filetype == EXE), 256);
    m.set(c2, 256);
    m.set(c3, 256);
    m.set(ismatch, 256);

    if(bpos) {
        c = c0 << (8 - bpos);
        if(bpos == 1) { c += c3 / 2; }
        c = (min(bpos, 5)) * 256 + c1 / 32 + 8 * (c2 / 32) + (c & 192);
    } else { c = c3 / 128 + (c4 >> 31) * 2 + 4 * (c2 / 64) + (c1 & 240); }
    m.set(c, 1536);
    int pr = m.p();
    return pr;
}


#endif //PAQ8PX_MODELS_H
