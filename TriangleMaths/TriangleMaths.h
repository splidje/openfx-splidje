#include "ofxCore.h"
#include <vector>

namespace TriangleMaths {
    typedef struct BarycentricWeights {
        double w1, w2, w3;
    } BarycentricWeights;
   
    class Triangle {
        public:
        unsigned long i1, i2, i3;
        OfxPointD p1, p2, p3;
        OfxRectI bounds;
        OfxPointD circum;
        double circum_radius;
        double w1Denom;
        double w2Denom;

        Triangle();
        Triangle(std::vector<OfxPointD>* vertices, long i1, long i2, long i3);

        void initialise(std::vector<OfxPointD>* vertices, long i1, long i2, long i3);

        bool boundsContains(OfxPointD p);
        bool circumCircleContains(OfxPointD p);
        BarycentricWeights toBarycentric(OfxPointD p);
        OfxPointD fromBarycentric(BarycentricWeights bw);
    };

    typedef struct EdgeIndexer {
        unsigned long i1, i2;
    } EdgeIndexer;

    class Edge {
        public:
        EdgeIndexer indexer;
        OfxPointD p1, p2;
        OfxRectI bounds;
        double magnitude;
        OfxPointD vect;
        OfxPointD norm;

        Edge();
        Edge(std::vector<OfxPointD>* vertices, long i1, long i2);
        void initialise(std::vector<OfxPointD>* vertices, long i1, long i2);

        double vectComp(OfxPointD p);
        double normComp(OfxPointD p);
    };

    bool operator==(EdgeIndexer e1, EdgeIndexer e2);

    std::vector<Triangle> delaunay(std::vector<OfxPointD> &vertices);

    std::vector<Edge> grahamScan(std::vector<OfxPointD> &vertices);
}
