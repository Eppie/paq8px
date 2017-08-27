#ifndef PAQ8PX_SPARSEMODEL_H
#define PAQ8PX_SPARSEMODEL_H

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

#endif //PAQ8PX_SPARSEMODEL_H
