#ifndef PAQ8PX_IM1BITMODEL_H
#define PAQ8PX_IM1BITMODEL_H

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

#endif //PAQ8PX_IM1BITMODEL_H
