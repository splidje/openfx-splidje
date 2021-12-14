#include "ofxsImageEffect.h"
#include <set>

// our smallest distance for snapping and
// dealing with rounding errors in calculations.
// Assuming our alpha is a 32 bit float,
// it can only be 0-1.
// there are 126 * 2^23 + 2 possible values
// 1 / sqrt of that is the number of subpixels
// needed to make a 1x1 pixel have that possible area.
#define QUADRANGLEDISTORT_DELTA 0.000030759

using namespace OFX;


namespace QuadrangleDistort {
    bool vectorEqual(const OfxPointD a, const OfxPointD b);
    void vectorAdd(const OfxPointD a, const OfxPointD b, OfxPointD* res);
    void vectorSubtract(const OfxPointD a, const OfxPointD b, OfxPointD* res);
    void vectorSubtract(const OfxPointI a, const OfxPointD b, OfxPointD* res);
    void vectorDivide(const OfxPointD a, double b, OfxPointD* res);
    double vectorMagSq(const OfxPointD a);
    void vectorRotate90(const OfxPointD a, OfxPointD* res);
    double vectorDotProduct(const OfxPointD a, const OfxPointD b);
    void rectIntersect(const OfxRectI* a, const OfxRectI* b, OfxRectI* res);

    class Edge {
        public:

        OfxPointD p;
        OfxPointD vect;
        double length;
        OfxPointD norm;
        bool isInitialised;

        bool initialise();
        double crosses(const Edge* edge);
    };

    double triangleArea(Edge* edges[2]);
    double calcInsideNess(const OfxPointD p, const Edge* cutEdge);

    class Quadrangle {
        public:

        Edge edges[4];
        int zeroEdgeCount;

        void initialise();

        bool isValid();

        void bounds(OfxRectI *rect);

        void fix(const std::set<int>* lockedIndices, std::set<int>* changedIndices);
    };

    class Polygon {
        public:

        std::vector<Edge> edges;

        void addPoint(OfxPointD p);
        void addPoint(OfxPointI p);
        void close();
        void clear();
        void cut(Edge* cutEdge, Polygon* res);
        double area();

        private:

        void initialiseLastEdgeVect(OfxPointD toP);
    };

    class QuadranglePixel {
        public:

        Quadrangle* quadrangle;
        OfxPointD p;
        double intersection;
        Polygon intersectionPoly;

        QuadranglePixel(Quadrangle* quad, OfxPointD p);
        void calculateIdentityPoint(OfxPointD* idP);

        private:

        void calcIntersection();

        OfxPointD _fromP[4];
    };

    void bilinear(double x, double y, Image* img, float* outPix, int componentCount);
}
