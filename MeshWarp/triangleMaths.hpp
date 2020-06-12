#include "ofxCore.h"

namespace TriangleMaths {
    typedef struct Triangle {
        OfxPointD p1, p2, p3;
    } Triangle;

    typedef struct BarycentricWeights {
        double w1, w2, w3;
    } BarycentricWeights;

    inline BarycentricWeights toBarycentric(OfxPointD p, Triangle tri) {
        BarycentricWeights bw;
        bw.w1 = (
            ((tri.p2.y - tri.p3.y) * (p.x - tri.p3.x) + (tri.p3.x - tri.p2.x) * (p.y - tri.p3.y))
            / ((tri.p2.y - tri.p3.y) * (tri.p1.x - tri.p3.x) + (tri.p3.x - tri.p2.x) * (tri.p1.y - tri.p3.y))
        );
        bw.w2 = (
            ((tri.p3.y - tri.p1.y) * (p.x - tri.p3.x) + (tri.p1.x - tri.p3.x) * (p.y - tri.p3.y))
            / ((tri.p2.y - tri.p3.y) * (tri.p1.x - tri.p3.x) + (tri.p3.x - tri.p2.x) * (tri.p1.y - tri.p3.y))
        );
        bw.w3 = 1 - bw.w1 - bw.w2;
        return bw;
    }

    inline OfxPointD fromBarycentric(BarycentricWeights bw, Triangle tri) {
        return {
            bw.w1 * tri.p1.x + bw.w2 * tri.p2.x + bw.w3 * tri.p3.x,
            bw.w1 * tri.p1.y + bw.w2 * tri.p2.y + bw.w3 * tri.p3.y
        };
    }
}
