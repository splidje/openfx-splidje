#ifndef PATCHMATCHUTILS_H
#define PATCHMATCHUTILS_H

#include "ofxCore.h"

template <typename T, class Rect>
inline T rectWidth(Rect r) {return r.x2 - r.x1;}
template <typename T, class Rect>
inline T rectHeight(Rect r) {return r.y2 - r.y1;}
inline double rectWidth(OfxRectD r) {return rectWidth<double>(r);}
inline int rectWidth(OfxRectI r) {return rectWidth<int>(r);}
inline double rectHeight(OfxRectD r) {return rectHeight<double>(r);}
inline int rectHeight(OfxRectI r) {return rectHeight<int>(r);}

inline OfxRectD& operator*=(OfxRectI& r, double f) {
    r.x1 *= f;
    r.y1 *= f;
    r.x2 *= f;
    r.y2 *= f;
    return r;
}

inline OfxRectD& operator*=(OfxRectD& r, double f) {
    r.x1 *= f;
    r.y1 *= f;
    r.x2 *= f;
    r.y2 *= f;
    return r;
}

inline OfxRectD operator*(const OfxRectI& r, double f) {
    OfxRectI res = r;
    return res *= f;
}

inline OfxRectD operator*(const OfxRectI& r, const OfxPointD& f) {
    return {
        r.x1 * f.x
        ,r.x2 * f.x
        ,r.y1 * f.y
        ,r.y2 * f.y
    }
}

inline OfxPointD operator*(const OfxPointI& p, double f) {
    return {p.x * f, p.y * f};
}

inline OfxPointI round(const OfxPointD& p) {
    return {(int)round(p.x), (int)round(p.y)};
}

template<class Point, class Rect>
inline bool insideRect(const Point& p, const Rect& rect) {
    return p.x >= rect.x1 && p.x < rect.x2 && p.y >= rect.y1 && p.y < rect.y2;
}

#endif
