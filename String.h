#ifndef PAQ8PX_STRING_H
#define PAQ8PX_STRING_H

#include <cassert>
#include "Array.h"

// A tiny subset of std::string
// size() includes NUL terminator.

class String : public Array<char> {
public:
    const char *c_str() const { return &(*this)[0]; }

    void operator=(const char *s) {
        resize(strlen(s) + 1);
        strcpy(&(*this)[0], s);
    }

    void operator+=(const char *s) {
        assert(s);
        pop_back();
        while(*s) { push_back(*s++); }
        push_back(0);
    }

    String(const char *s = "") : Array<char>(1) {
        (*this) += s;
    }
};


#endif //PAQ8PX_STRING_H
