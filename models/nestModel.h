#ifndef PAQ8PX_NESTMODEL_H
#define PAQ8PX_NESTMODEL_H

void nestModel(Mixer &m) {
    static int ic = 0, bc = 0, pc = 0, vc = 0, qc = 0, lvc = 0, wc = 0;
    static ContextMap cm(MEM, 10);
    if(bpos == 0) {
        int c = c4 & 255, matched = 1, vv;
        const int lc = (c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c);
        if(lc == 'a' || lc == 'e' || lc == 'i' || lc == 'o' || lc == 'u') { vv = 1; }
        else if(lc >= 'a' && lc <= 'z') { vv = 2; }
        else if(lc == ' ' || lc == '.' || lc == ',' || lc == '\n') { vv = 3; }
        else if(lc >= '0' && lc <= '9') { vv = 4; }
        else if(lc == 'y') { vv = 5; }
        else if(lc == '\'') { vv = 6; } else { vv = (c & 32) ? 7 : 0; }
        vc = (vc << 3) | vv;
        if(vv != lvc) {
            wc = (wc << 3) | vv;
            lvc = vv;
        }
        switch(c) {
            case ' ':
                qc = 0;
                break;
            case '(':
                ic += 513;
                break;
            case ')':
                ic -= 513;
                break;
            case '[':
                ic += 17;
                break;
            case ']':
                ic -= 17;
                break;
            case '<':
                ic += 23;
                qc += 34;
                break;
            case '>':
                ic -= 23;
                qc /= 5;
                break;
            case ':':
                pc = 20;
                break;
            case '{':
                ic += 22;
                break;
            case '}':
                ic -= 22;
                break;
            case '|':
                pc += 223;
                break;
            case '"':
                pc += 0x40;
                break;
            case '\'':
                pc += 0x42;
                break;
            case '\n':
                pc = qc = 0;
                break;
            case '.':
                pc = 0;
                break;
            case '!':
                pc = 0;
                break;
            case '?':
                pc = 0;
                break;
            case '#':
                pc += 0x08;
                break;
            case '%':
                pc += 0x76;
                break;
            case '$':
                pc += 0x45;
                break;
            case '*':
                pc += 0x35;
                break;
            case '-':
                pc += 0x3;
                break;
            case '@':
                pc += 0x72;
                break;
            case '&':
                qc += 0x12;
                break;
            case ';':
                qc /= 3;
                break;
            case '\\':
                pc += 0x29;
                break;
            case '/':
                pc += 0x11;
                if(buf.size() > 1 && buf(1) == '<') { qc += 74; }
                break;
            case '=':
                pc += 87;
                break;
            default:
                matched = 0;
        }
        if(matched) { bc = 0; } else { bc += 1; }
        if(bc > 300) { bc = ic = pc = qc = 0; }


        cm.set((3 * vc + 77 * pc + 373 * ic + qc) & 0xffff);
        cm.set((31 * vc + 27 * pc + 281 * qc) & 0xffff);
        cm.set((13 * vc + 271 * ic + qc + bc) & 0xffff);
        cm.set((17 * pc + 7 * ic) & 0xffff);
        cm.set((13 * vc + ic) & 0xffff);
        cm.set((vc / 3 + pc) & 0xffff);
        cm.set((7 * wc + qc) & 0xffff);
        cm.set((vc & 0xffff) | ((c4 & 0xff) << 16));
        cm.set(((3 * pc) & 0xffff) | ((c4 & 0xff) << 16));
        cm.set((ic & 0xffff) | ((c4 & 0xff) << 16));

    }
    cm.mix(m);
}

#endif //PAQ8PX_NESTMODEL_H
