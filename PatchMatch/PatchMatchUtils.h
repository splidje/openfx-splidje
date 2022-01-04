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

template<class T, class LHST, class RHST>
inline T addPoints(LHST lhs, RHST rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

inline OfxPointI operator+(OfxPointI lhs, OfxPointI rhs) {
    return addPoints<OfxPointI>(lhs, rhs);
}
inline OfxPointD operator+(OfxPointD lhs, OfxPointD rhs) {
    return addPoints<OfxPointD>(lhs, rhs);
}
inline OfxPointD operator+(OfxPointI lhs, OfxPointD rhs) {
    return addPoints<OfxPointD>(lhs, rhs);
}
inline OfxPointD operator+(OfxPointD lhs, OfxPointI rhs) {
    return addPoints<OfxPointD>(lhs, rhs);
}

inline OfxRectI& operator*=(OfxRectI& r, double f) {
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
    OfxRectD res{(double)r.x1, (double)r.y1, (double)r.x2, (double)r.y2};
    return res *= f;
}

inline OfxRectD operator*(const OfxRectD& r, const OfxPointD& f) {
    return {
        r.x1 * f.x
        ,r.x2 * f.x
        ,r.y1 * f.y
        ,r.y2 * f.y
    };
}

inline OfxPointD operator*(const OfxPointI& p, double f) {
    return {p.x * f, p.y * f};
}

inline OfxPointI round(const OfxPointD& p) {
    return {(int)round(p.x), (int)round(p.y)};
}

inline OfxRectI round(const OfxRectD& r) {
    return {
        (int)round(r.x1)
        ,(int)round(r.y1)
        ,(int)round(r.x2)
        ,(int)round(r.y2)
    };
}

template<class Point, class Rect>
inline bool insideRect(const Point& p, const Rect& rect) {
    return p.x >= rect.x1 && p.x < rect.x2 && p.y >= rect.y1 && p.y < rect.y2;
}

#endif
