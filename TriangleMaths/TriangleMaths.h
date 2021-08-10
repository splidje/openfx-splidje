#include "ofxCore.h"
#include <vector>

namespace TriangleMaths {
    typedef struct Triangle {
        OfxPointD p1, p2, p3;
    } Triangle;

    typedef struct TriIndexer {
        unsigned long i1, i2, i3;
    } TriIndexer;

    typedef struct Edge {
        OfxPointD p1, p2;
    } Edge;

    typedef struct EdgeIndexer {
        unsigned long i1, i2;
    } EdgeIndexer;

    bool operator==(EdgeIndexer e1, EdgeIndexer e2);

    typedef struct BarycentricWeights {
        double w1, w2, w3;
    } BarycentricWeights;

    Triangle indexerToTri(TriIndexer idx, std::vector<OfxPointD> verticies);

    BarycentricWeights toBarycentric(OfxPointD p, Triangle tri);

    OfxPointD fromBarycentric(BarycentricWeights bw, Triangle tri);

    bool circumCircleContains(Triangle tri, OfxPointD p);
    
    std::vector<TriIndexer> delaunay(std::vector<OfxPointD> &vertices);
}
