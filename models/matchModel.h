#ifndef PAQ8PX_MATCHMODEL_H
#define PAQ8PX_MATCHMODEL_H

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

#endif //PAQ8PX_MATCHMODEL_H
