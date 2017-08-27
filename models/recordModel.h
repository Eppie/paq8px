#ifndef PAQ8PX_RECORDMODEL_H
#define PAQ8PX_RECORDMODEL_H

// Model 2-D data with fixed record length.  Also order 1-2 models
// that include the distance to the last match.

#define SPACE 0x20

void recordModel(Mixer &m) {
    static int cpos1[256], cpos2[256], cpos3[256], cpos4[256];
    static int wpos1[0x10000]; // buf(1..2) -> last position
    static int rlen[3] = {2, 3, 4}; // run length and 2 candidates
    static int rcount[2] = {0, 0}; // candidate counts
    static U8 padding = 0; // detected padding byte
    static int prevTransition = 0, col = 0, mxCtx = 0; // position of the last padding transition
    static ContextMap cm(32768, 3), cn(32768 / 2, 3), co(32768 * 2, 3), cp(MEM, 6);

    // Find record length
    if(!bpos) {
        int w = c4 & 0xffff, c = w & 255, d = w >> 8;
        int r = pos - cpos1[c];
        if(r > 1 && r == cpos1[c] - cpos2[c]
           && r == cpos2[c] - cpos3[c] && (r > 32 || r == cpos3[c] - cpos4[c])
           && (r > 10 || ((c == buf(r * 5 + 1)) && c == buf(r * 6 + 1)))) {
            if(r == rlen[1]) { ++rcount[0]; }
            else if(r == rlen[2]) { ++rcount[1]; }
            else if(rcount[0] > rcount[1]) { rlen[2] = r, rcount[1] = 1; }
            else { rlen[1] = r, rcount[0] = 1; }
        }

        // check candidate lengths
        for(int i = 0; i < 2; i++) {
            if(rcount[i] > max(0, 12 - (int) ilog2(rlen[i + 1]))) {
                if(rlen[0] != rlen[i + 1]) {
                    if((rlen[i + 1] > rlen[0]) && (rlen[i + 1] % rlen[0] == 0)) {
                        // maybe we found a multiple of the real record size..?
                        // in that case, it is probably an immediate multiple (2x).
                        // that is probably more likely the bigger the length, so
                        // check for small lengths too
                        if((rlen[0] > 32) && (rlen[i + 1] == rlen[0] * 2)) {
                            rcount[0] >>= 1;
                            rcount[1] >>= 1;
                            continue;
                        }
                    }
                    rlen[0] = rlen[i + 1];
                    rcount[i] = 0;
                } else {
                    // we found the same length again, that's positive reinforcement that
                    // this really is the correct record size, so give it a little boost
                    rcount[i] >>= 2;
                }

                // if the other candidate record length is orders of
                // magnitude larger, it will probably never have enough time
                // to increase its counter before it's reset again. and if
                // this length is not a multiple of the other, than it might
                // really be worthwhile to investigate it, so we won't set its
                // counter to 0
                if(rlen[i + 1] << 4 > rlen[1 + (i ^ 1)]) {
                    rcount[i ^ 1] = 0;
                }
            }
        }

        // Set 2 dimensional contexts
        assert(rlen[0] > 0);
        cm.set(c << 8 | (min(255, pos - cpos1[c]) / 4));
        cm.set(w << 9 | llog(pos - wpos1[w]) >> 2);
        cm.set(rlen[0] | buf(rlen[0]) << 10 | buf(rlen[0] * 2) << 18);

        cn.set(w | rlen[0] << 8);
        cn.set(d | rlen[0] << 16);
        cn.set(c | rlen[0] << 8);

        co.set(buf(1) << 8 | min(255, pos - cpos1[buf(1)]));
        co.set(buf(1) << 17 | buf(2) << 9 | llog(pos - wpos1[w]) >> 2);
        co.set(buf(1) << 8 | buf(rlen[0]));

        col = pos % rlen[0];
        cp.set(rlen[0] | buf(rlen[0]) << 10 | col << 18);
        cp.set(rlen[0] | buf(1) << 10 | col << 18);
        cp.set(col | rlen[0] << 12);

        /*
        Consider record structures that include fixed-length strings.
        These usually contain the text followed by either spaces or 0's,
        depending on whether they're to be trimmed or they're null-terminated.
        That means we can guess the length of the string field by looking
        for small repetitions of one of these padding bytes followed by a
        different byte. By storing the last position where this transition
        ocurred, and what was the padding byte, we are able to model these
        runs of padding bytes.
        Special care is taken to skip record structures of less than 9 bytes,
        since those may be little-endian 64 bit integers. If they contain
        relatively low values (<2^40), we may consistently get runs of 3 or
        even more 0's at the end of each record, and so we could assume that
        to be the general case. But with integers, we can't be reasonably sure
        that a number won't have 3 or more 0's just before a final non-zero MSB.
        And with such simple structures, there's probably no need to be fancy
        anyway
        */

        if((((U16) (c4 >> 8) == ((SPACE << 8) + SPACE)) && (c != SPACE)) ||
           (!(c4 >> 8) && c && ((padding != SPACE) || (pos - prevTransition > rlen[0])))) {
            prevTransition = pos;
            padding = (U8) d;
        }
        cp.set((rlen[0] > 8) * hash(min(min(0xFF, rlen[0]), pos - prevTransition), min(0x3FF, col),
                                    (w & 0xF0F0) | (w == ((padding << 8) | padding))));

        int last4 = (buf(rlen[0] * 4) << 24) | (buf(rlen[0] * 3) << 16) | (buf(rlen[0] * 2) << 8) | buf(rlen[0]);
        cp.set((last4 & 0xFF) | ((last4 & 0xF000) >> 4) | ((last4 & 0xE00000) >> 9) | ((last4 & 0xE0000000) >> 14) |
               ((col / max(1, rlen[0] / 16)) << 18));
        cp.set((last4 & 0xF8F8) | (col << 16));

        // update last context positions
        cpos4[c] = cpos3[c];
        cpos3[c] = cpos2[c];
        cpos2[c] = cpos1[c];
        cpos1[c] = pos;
        wpos1[w] = pos;

        mxCtx = (rlen[0] > 128) ? (min(0x7F, col / max(1, rlen[0] / 128))) : col;
    }
    cm.mix(m);
    cn.mix(m);
    co.mix(m);
    cp.mix(m);

    if(level > 7) {
        m.set((rlen[0] > 2) * ((bpos << 7) | mxCtx), 1024);
    }
}

#endif //PAQ8PX_RECORDMODEL_H
