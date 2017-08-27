#ifndef PAQ8PX_DMCMODEL_H
#define PAQ8PX_DMCMODEL_H

// Model using DMC. The bitwise context is represented by a state graph,
// initialized to a bytewise order 1 model as in
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

#endif //PAQ8PX_DMCMODEL_H
