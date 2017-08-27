#ifndef PAQ8PX_INDIRECTMODEL_H
#define PAQ8PX_INDIRECTMODEL_H

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

#endif //PAQ8PX_INDIRECTMODEL_H
