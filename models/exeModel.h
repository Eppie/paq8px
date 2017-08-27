#ifndef PAQ8PX_EXEMODEL_H
#define PAQ8PX_EXEMODEL_H

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

#endif //PAQ8PX_EXEMODEL_H
