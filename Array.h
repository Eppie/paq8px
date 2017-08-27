//
// Created by Epstein, Andrew (ELS-PHI) on 8/18/17.
//

#ifndef PAQ8PX_ARRAY_H
#define PAQ8PX_ARRAY_H


// Array<T> a(n); creates n elements of T initialized to 0 bits.
// Constructors for T are not called.
// Indexing is bounds checked if assertions are on.
// a.size() returns n.
// a.resize(n) changes size to n, padding with 0 bits or truncating.
// a.push_back(x) appends x and increases size by 1, reserving up to size*2.
// a.pop_back() decreases size by 1, does not free memory.
// Copy and assignment are not supported.

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include "utils.h"

using std::min;
using std::max;

typedef unsigned long long qword;
typedef unsigned int U32;

const U32 g_PageMask0 = 0x1000 - 1;
U32 g_PageMask = g_PageMask0;

template<class T>
T *VAlloc(qword size) {
    void *r;
    size *= sizeof(T);
    qword s = (size + g_PageMask) & (~qword(g_PageMask));
    Retry:
//printf( "s=%I64X sizeof(size_t)=%i\n", s, sizeof(size_t) );
#ifndef X64
    if(s >= (qword(1) << 32)) { r = 0; }
    else
#endif
    {
        r = malloc(s);
    }
//printf( "r=%08X\n", r );
    if((r == 0) && (g_PageMask != g_PageMask0)) {
        g_PageMask = g_PageMask0;
        s = size;
        goto Retry;
    }
    return (T *) r;
}


template<class T>
void VFree(T *p) {
    free(p);
}


template<class T>
class Array {
private:
    int n;     // user size
    int reserved;  // actual size
    char *ptr; // allocated memory, zeroed
    T *data;   // start of n elements of aligned data
    void create(int i);  // create with size i
public:
    explicit Array(int i = 0) { create(i); }

    ~Array();

    T &operator[](int i) {
#ifndef NDEBUG
        if(i < 0 || i >= n) { fprintf(stderr, "%d out of bounds %d\n", i, n), quit(); }
#endif
        return data[i];
    }

    const T &operator[](int i) const {
#ifndef NDEBUG
        if(i < 0 || i >= n) { fprintf(stderr, "%d out of bounds %d\n", i, n), quit(); }
#endif
        return data[i];
    }

    int size() const { return n; }

    void resize(int i);  // change size to i
    void pop_back() { if(n > 0) { --n; }}  // decrement size
    void push_back(const T &x);  // increment size, append x
private:
    Array(const Array &);  // no copy or assignment
    Array &operator=(const Array &);
};

template<class T>
void Array<T>::resize(int i) {
    if(i <= reserved) {
        n = i;
        return;
    }
    char *saveptr = ptr;
    T *savedata = data;
    int saven = n;
    create(i);
    if(saveptr) {
        if(savedata) {
            memcpy(data, savedata, sizeof(T) * min(i, saven));
//            programChecker.alloc(-n * sizeof(T)); // TODO
        }
        free(saveptr);
    }
}

template<class T>
void Array<T>::create(int i) {
    n = reserved = i;
    if(i <= 0) {
        data = 0;
        ptr = 0;
        return;
    }
    const int sz = n * sizeof(T);
//    programChecker.alloc(sz); // TODO
    char *r;

    if(sz > (1 << 24)) {
        r = ptr = VAlloc<char>(sz);
    } else {
        int flag = (sz >= 16);
        ptr = (char *) calloc(sz + (flag ? 15 : 0), 1);
        if(!ptr) {
            quit("Out of memory");
        }
        r = ptr;
        if(flag) {
            r = ptr + 15;
            r -= (r - ((char *) 0)) & 15;
        }
    }

    data = (T *) r; //ptr;
}

template<class T>
Array<T>::~Array() {
//    programChecker.alloc(-n * sizeof(T)); // TODO
    const int sz = n * sizeof(T);

    if(sz > (1 << 24)) {
        VFree(ptr);
    } else {
        free(ptr);
    }
}

template<class T>
void Array<T>::push_back(const T &x) {
    if(n == reserved) {
        int saven = n;
        resize(max(1, n * 2));
        n = saven;
    }
    data[n++] = x;
}

#endif //PAQ8PX_ARRAY_H
