#ifndef PAQ8PX_DISTANCEMODEL_H
#define PAQ8PX_DISTANCEMODEL_H
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

#endif //PAQ8PX_DISTANCEMODEL_H
