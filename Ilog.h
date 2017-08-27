#ifndef PAQ8PX_ILOG_H
#define PAQ8PX_ILOG_H

// ilog(x) = round(log2(x) * 16), 0 <= x < 64K
class Ilog {
    Array<U8> t;
public:
    int operator()(U16 x) const { return t[x]; }

    Ilog();
} ilog;

// Compute lookup table by numerical integration of 1/x
Ilog::Ilog() : t(65536) {
    U32 x = 14155776;
    for(int i = 2; i < 65536; ++i) {
        x += 774541002 / (i * 2 - 1);  // numerator is 2^29/ln 2
        t[i] = x >> 24;
    }
}

// llog(x) accepts 32 bits
inline int llog(U32 x) {
    if(x >= 0x1000000) {
        return 256 + ilog(x >> 16);
    } else if(x >= 0x10000) {
        return 128 + ilog(x >> 8);
    } else {
        return ilog(x);
    }
}


#endif //PAQ8PX_ILOG_H
