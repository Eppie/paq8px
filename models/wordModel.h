#ifndef PAQ8PX_WORDMODEL_H
#define PAQ8PX_WORDMODEL_H

// Model English text (words and columns/end of line)
static U32 frstchar = 0, spafdo = 0, spaces = 0, spacecount = 0, words = 0, wordcount = 0, wordlen = 0, wordlen1 = 0;

void wordModel(Mixer &m, Filetype filetype) {
    static U32 word0 = 0, word1 = 0, word2 = 0, word3 = 0, word4 = 0, word5 = 0;  // hashes
    static U32 xword0 = 0, xword1 = 0, xword2 = 0, cword0 = 0, ccword = 0;
    static U32 number0 = 0, number1 = 0;  // hashes
    static U32 text0 = 0;  // hash stream of letters
    static U32 lastLetter = 0, lastUpper = 0, wordGap = 0;
    static ContextMap cm(MEM * 16, 44 + 1);
    static int nl1 = -3, nl = -2, w = 0;  // previous, current newline position
    static U32 mask = 0;
    static Array<int> wpos(0x10000);  // last position of word

    // Update word hashes
    if(bpos == 0) {
        int c = c4 & 255, f = 0;
        if(spaces & 0x80000000) { --spacecount; }
        if(words & 0x80000000) { --wordcount; }
        spaces = spaces * 2;
        words = words * 2;
        lastUpper = min(lastUpper + 1, 63);
        lastLetter = min(lastLetter + 1, 63);
        if(c >= 'A' && c <= 'Z') { c += 'a' - 'A', lastUpper = 0; }
        if((c >= 'a' && c <= 'z') || c == 1 || c == 2 || (c >= 128 && (b2 != 3))) {
            if(!wordlen) {
                wordGap = lastLetter;
            }
            lastLetter = 0;
            ++words, ++wordcount;
            word0 ^= hash(word0, c, 0);
            text0 = text0 * 997 * 16 + c;
            wordlen++;
            wordlen = min(wordlen, 45);
            f = 0;
            w = word0 & (wpos.size() - 1);
        } else {
            if(word0) {
                word5 = word4;
                word4 = word3;
                word3 = word2;
                word2 = word1;
                word1 = word0;
                wordlen1 = wordlen;
                wpos[w] = blpos;
                if(c == ':' || c == '=') { cword0 = word0; }
                if(c == ']' && (frstchar != ':' || frstchar != '*')) { xword0 = word0; }
                ccword = 0;
                word0 = wordlen = 0;
                if((c == '.' || c == '!' || c == '?' || c == '}' || c == ')') && buf(2) != 10 &&
                   filetype != EXE) { f = 1; }
            }
            if((c4 & 0xFFFF) == 0x3D3D) { xword1 = word1, xword2 = word2; } // ==
            if((c4 & 0xFFFF) == 0x2727) { xword1 = word1, xword2 = word2; } // ''
            if(c == 32 || c == 10) {
                ++spaces, ++spacecount;
                if(c == 10) { nl1 = nl, nl = pos - 1; }
            } else if(c == '.' || c == '!' || c == '?' || c == ',' || c == ';' || c == ':') { spafdo = 0, ccword = c; }
            else {
                ++spafdo;
                spafdo = min(63, spafdo);
            }
        }
        if(c >= '0' && c <= '9') {
            number0 ^= hash(number0, c, 1);
        } else if(number0) {
            number1 = number0;
            number0 = 0, ccword = 0;
        }

        int col = min(255, pos - nl);
        int above = buf[nl1 + col];
        if(col <= 2) { frstchar = (col == 2 ? min(c, 96) : 0); }
        if(frstchar == '[' && c == 32) { if(buf(3) == ']' || buf(4) == ']') { frstchar = 96, xword0 = 0; }}
        cm.set(hash(513, spafdo, spaces, ccword));
        cm.set(hash(514, frstchar, c));
        cm.set(hash(515, col, frstchar));
        cm.set(hash(516, spaces, (words & 255)));
        cm.set(hash(256, number0, word2));
        cm.set(hash(257, number0, word1));
        cm.set(hash(258, number1, c, ccword));
        cm.set(hash(259, number0, number1));
        cm.set(hash(260, word0, number1));
        cm.set(hash(518, wordlen1, col));
        cm.set(hash(519, c, spacecount / 2));
        U32 h = wordcount * 64 + spacecount;
        cm.set(hash(520, c, h, ccword));
        cm.set(hash(517, frstchar, h));
        cm.set(hash(521, h, spafdo));

        U32 d = c4 & 0xf0ff;
        cm.set(hash(522, d, frstchar, ccword));
        h = word0 * 271;
        h = h + buf(1);
        cm.set(hash(262, h, 0));
        cm.set(hash(263, word0, 0));
        cm.set(hash(264, h, word1));
        cm.set(hash(265, word0, word1));
        cm.set(hash(266, h, word1, word2));
        cm.set(hash(268, text0 & 0xfffff, 0));
        cm.set(hash(269, word0, xword0));
        cm.set(hash(270, word0, xword1));
        cm.set(hash(271, word0, xword2));
        cm.set(hash(272, frstchar, xword2));
        cm.set(hash(273, word0, cword0));
        cm.set(hash(274, number0, cword0));
        cm.set(hash(275, h, word2));
        cm.set(hash(276, h, word3));
        cm.set(hash(277, h, word4));
        cm.set(hash(278, h, word5));
        cm.set(hash(279, h, word1, word3));
        cm.set(hash(280, h, word2, word3));
        if(f) {
            word5 = word4;
            word4 = word3;
            word3 = word2;
            word2 = word1;
            word1 = '.';
        }
        cm.set(hash(523, col, buf(1), above));
        cm.set(hash(524, buf(1), above));
        cm.set(hash(525, col, buf(1)));
        cm.set(hash(526, col, c == 32));
        cm.set(hash(281, w, llog(blpos - wpos[w]) >> 4));
        cm.set(hash(282, buf(1), llog(blpos - wpos[w]) >> 2));

        int fl = 0;
        if((c4 & 0xff) != 0) {
            if(isalpha(c4 & 0xff)) { fl = 1; }
            else if(ispunct(c4 & 0xff)) { fl = 2; }
            else if(isspace(c4 & 0xff)) { fl = 3; }
            else if((c4 & 0xff) == 0xff) { fl = 4; }
            else if((c4 & 0xff) < 16) { fl = 5; }
            else if((c4 & 0xff) < 64) { fl = 6; }
            else { fl = 7; }
        }
        mask = (mask << 3) | fl;
        cm.set(hash(528, mask, 0));
        cm.set(hash(529, mask, buf(1)));
        cm.set(hash(530, mask & 0xff, col));
        cm.set(hash(531, mask, buf(2), buf(3)));
        cm.set(hash(532, mask & 0x1ff, f4 & 0x00fff0));

        cm.set(hash(h, llog(wordGap), mask & 0x1FF,
                    ((wordlen1 > 3) << 6) |
                    ((wordlen > 0) << 5) |
                    ((spafdo == wordlen + 2) << 4) |
                    ((spafdo == wordlen + wordlen1 + 3) << 3) |
                    ((spafdo >= lastLetter + wordlen1 + wordGap) << 2) |
                    ((lastUpper < lastLetter + wordlen1) << 1) |
                    (lastUpper < wordlen + wordlen1 + wordGap)
        ));
    }
    cm.mix(m);
}

#endif //PAQ8PX_WORDMODEL_H
