#ifndef PAQ8PX_CONTEXTMODEL2_H
#define PAQ8PX_CONTEXTMODEL2_H

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

#endif //PAQ8PX_CONTEXTMODEL2_H
