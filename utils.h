#ifndef PAQ8PX_UTILS_H_H
#define PAQ8PX_UTILS_H_H

#define nex(state, sel) State_table[state][sel]

// min, max functions
inline int min(int a, int b) { return a < b ? a : b; }

inline int max(int a, int b) { return a < b ? b : a; }

// Error handler: print message if any, and exit
void quit(const char *message = 0) {
    throw message;
}

// strings are equal ignoring case?
int equals(const char *a, const char *b) {
    assert(a && b);
    while(*a && *b) {
        int c1 = *a;
        if(c1 >= 'A' && c1 <= 'Z') {
            c1 += 'a' - 'A';
        }
        int c2 = *b;
        if(c2 >= 'A' && c2 <= 'Z') {
            c2 += 'a' - 'A';
        }
        if(c1 != c2) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == *b;
}

// Hash 2-5 ints.
inline U32 hash(U32 a, U32 b, U32 c = 0xffffffff, U32 d = 0xffffffff, U32 e = 0xffffffff) {
    U32 h = a * 200002979u + b * 30005491u + c * 50004239u + d * 70004807u + e * 110002499u;
    return h ^ h >> 9 ^ a >> 2 ^ b >> 3 ^ c >> 4 ^ d >> 5 ^ e >> 6;
}

inline unsigned BitCount(unsigned v) {
    v -= ((v >> 1) & 0x55555555);
    v = ((v >> 2) & 0x33333333) + (v & 0x33333333);
    v = ((v >> 4) + v) & 0x0f0f0f0f;
    v = ((v >> 8) + v) & 0x00ff00ff;
    v = ((v >> 16) + v) & 0x0000ffff;
    return v;
}

inline unsigned ilog2(unsigned x) {
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    x = x | (x >> 8);
    x = x | (x >> 16);
    return BitCount(x >> 1);
}

#endif //PAQ8PX_UTILS_H_H
