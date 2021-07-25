#include "ofxsImageEffect.h"

// our smallest distance for snapping and
// dealing with rounding errors in calculations.
// Assuming our alpha is a 32 bit float,
// it can only be 0-1.
// there are 126 * 2^23 + 2 possible values
// 1 / sqrt of that is the number of subpixels
// needed to make a 1x1 pixel have that possible area.
#define QUADRANGLEDISTORT_DELTA 0.000030759


using namespace OFX;

// Declarations

namespace QuadrangleDistort {
    inline bool vectorEqual(OfxPointD a, OfxPointD b);
    inline void vectorAdd(OfxPointD a, OfxPointD b, OfxPointD* res);
    inline void vectorSubtract(OfxPointD a, OfxPointD b, OfxPointD* res);
    inline void vectorSubtract(OfxPointI a, OfxPointD b, OfxPointD* res);
    inline void vectorDivide(OfxPointD a, double b, OfxPointD* res);
    inline double vectorMagSq(OfxPointD a);
    inline void vectorRotate90(OfxPointD a, OfxPointD* res);
    inline double vectorDotProduct(OfxPointD a, OfxPointD b);

    class Edge {
        public:

        OfxPointD p;
        OfxPointD vect;
        double length;
        OfxPointD norm;
        bool isInitialised = false;

        inline bool initialise();
        inline double crosses(Edge* edge);
    };

    inline double triangleArea(Edge* edges[2]);
    inline double calcInsideNess(OfxPointD p, Edge* cutEdge);

    class Quadrangle {
        public:

        Edge edges[4];

        inline bool initialise();
    };

    class Polygon {
        public:

        std::vector<Edge> edges;

        inline void addPoint(OfxPointD p);
        inline void addPoint(OfxPointI p);
        inline void close();
        inline void clear();
        inline void cut(Edge* cutEdge, Polygon* res);
        inline double area();

        private:

        inline void initialiseLastEdgeVect(OfxPointD toP);
    };

    class QuadranglePixel {
        public:

        Quadrangle* quadrangle;
        OfxPointI p;
        double intersection;
        Polygon intersectionPoly;

        inline QuadranglePixel(Quadrangle* quad, OfxPointI p);
        inline void calculateIdentityPoint(OfxPointD* idP);

        private:

        inline void calcIntersection();

        OfxPointD _fromP[4];
    };

    void bilinear(double x, double y, Image* img, float* outPix, int componentCount);
}


// Definitions

using namespace QuadrangleDistort;

inline bool QuadrangleDistort::vectorEqual(OfxPointD a, OfxPointD b) {
    return a.x == b.x && a.y == b.y;
}

inline void QuadrangleDistort::vectorAdd(OfxPointD a, OfxPointD b, OfxPointD* res) {
    res->x = a.x + b.x;
    res->y = a.y + b.y;
}

inline void QuadrangleDistort::vectorSubtract(OfxPointD a, OfxPointD b, OfxPointD* res) {
    res->x = a.x - b.x;
    res->y = a.y - b.y;
}

inline void QuadrangleDistort::vectorSubtract(OfxPointI a, OfxPointD b, OfxPointD* res) {
    res->x = a.x - b.x;
    res->y = a.y - b.y;
}

inline void QuadrangleDistort::vectorDivide(OfxPointD a, double b, OfxPointD* res) {
    res->x = a.x / b;
    res->y = a.y / b;
}

inline double QuadrangleDistort::vectorMagSq(OfxPointD a) {
    return a.x * a.x + a.y * a.y;
}

inline void QuadrangleDistort::vectorRotate90(OfxPointD a, OfxPointD* res) {
    res->x = -a.y;
    res->y = a.x;
}

inline double QuadrangleDistort::vectorDotProduct(OfxPointD a, OfxPointD b) {
    return a.x * b.x + a.y * b.y;
}

// Edge

inline bool Edge::initialise() {
    if (vect.x == 0 && vect.y == 0) {return false;}
    length = sqrt(vectorMagSq(vect));
    if (length == 0) {return false;}
    OfxPointD tang;
    vectorRotate90(vect, &tang);
    vectorDivide(tang, length, &norm);
    isInitialised = true;
    return true;
}

inline double Edge::crosses(Edge* edge) {
    auto denom = vect.y * edge->vect.x - vect.x * edge->vect.y;
    if (denom == 0) {
        return INFINITY;
    }
    auto result (
        (
            edge->vect.y * (p.x - edge->p.x)
            + edge->vect.x * (edge->p.y - p.y)
        )
        / denom
    );
    if (result > -QUADRANGLEDISTORT_DELTA && result < QUADRANGLEDISTORT_DELTA) {
        result = 0;
    } else if (result > 1 - QUADRANGLEDISTORT_DELTA && result < 1 + QUADRANGLEDISTORT_DELTA) {
        result = 1;
    }
    return result;
}

inline double QuadrangleDistort::triangleArea(Edge* edges[2]) {
    assert(edges[0]->isInitialised);
    assert(edges[1]->isInitialised);
    // base x height over 2
    auto result = (
        0.5 * edges[0]->length
        * vectorDotProduct(edges[1]->vect, edges[0]->norm)
    );
    return result;
}

inline double QuadrangleDistort::calcInsideNess(OfxPointD p, Edge* cutEdge) {
    OfxPointD relP;
    vectorSubtract(p, cutEdge->p, &relP);
    auto insideNess = vectorDotProduct(relP, cutEdge->norm);
    if (insideNess > -QUADRANGLEDISTORT_DELTA && insideNess < QUADRANGLEDISTORT_DELTA) {
        insideNess = 0;
    }
    return insideNess;
}

// Quadrangle

inline bool Quadrangle::initialise() {
    Edge* edge = edges;
    for (int i=0; i < 4; i++, edge++) {
        vectorSubtract(
            edges[(i+1) % 4].p,
            edge->p,
            &edge->vect
        );
        if (!edge->initialise()) {return false;}
    }
    return true;
}

// Polygon

inline void Polygon::addPoint(OfxPointD p) {
    if (edges.size() > 0) {
        initialiseLastEdgeVect(p);
    }
    Edge edge;
    edge.p = p;
    edges.push_back(edge);
}

inline void Polygon::addPoint(OfxPointI p) {
    OfxPointD pD;
    pD.x = p.x;
    pD.y = p.y;
    addPoint(pD);
}

inline void Polygon::close() {
    initialiseLastEdgeVect(edges[0].p);
}

inline void Polygon::clear() {
    edges.clear();
}

inline void Polygon::cut(Edge* cutEdge, Polygon* res) {
    if (edges.size() == 0) {return;}
    // establish whether the first edge's first point is inside
    double insideNess = 0;
    OfxPointD crossPoint;
    for (auto edgeIter=edges.begin(); edgeIter < edges.end(); edgeIter++) {
        auto edge = *edgeIter;
        // lost track of whether we're inside or not
        if (insideNess == 0) {
            insideNess = calcInsideNess(edge.p, cutEdge);
        }
        if (insideNess >= 0) {
            res->addPoint(edge.p);
        }
        // no point in establishing a cross if the edge has snapped
        // to the point
        if (insideNess == 0) {continue;}
        auto cross = edge.crosses(cutEdge);
        if (cross < 0 || cross >= 1) {continue;}
        if (cross == 0) {
            insideNess = 0;
            res->addPoint(edge.p);
        }
        else {
            insideNess = -insideNess;
            crossPoint.x = edge.p.x + edge.vect.x * cross;
            crossPoint.y = edge.p.y + edge.vect.y * cross;
            res->addPoint(crossPoint);
        }
    }
    if (res->edges.size() > 0) {
        res->close();
    }
}

inline double Polygon::area() {
    if (edges.size() < 3) {
        return 0;
    }
    // start with tri made with first 2 edges
    Edge* tri[2] = {&edges[0], &edges[1]};
    double total = triangleArea(tri);
    Edge secondEdge;
    // keep making triangles using next edge and one made connecting with start
    for (auto edgeIter = edges.begin() + 2; (edgeIter + 1) < edges.end(); edgeIter++) {
        tri[0] = &*edgeIter;
        // if next edge isn't the last,
        // we'll need to create a new edge to the first point
        if (edgeIter + 2 < edges.end()) {
            // create an edge from the end of this edge (i.e. start of next)
            // to the first point
            secondEdge.p = (edgeIter + 1)->p;
            vectorSubtract(edges[0].p, secondEdge.p, &secondEdge.vect);
            if (!secondEdge.initialise()) {
                continue;
            }
            tri[1] = &secondEdge;
        }
        // if next edge is the last, then it already goes to the start point
        else {
            tri[1] = &edges.back();
        }
        total += triangleArea(tri);
    }
    return total;
}

inline void Polygon::initialiseLastEdgeVect(OfxPointD toP) {
    auto lastEdge = &edges.back();
    vectorSubtract(toP, lastEdge->p, &lastEdge->vect);
    if (!lastEdge->initialise()) {
        // penultimate edge now becomes last edge.
        // redirect it to this point
        edges.pop_back();
        if (edges.size() > 0) {
            initialiseLastEdgeVect(toP);
            return;
        }
    }
}

// QuadranglePixel

inline QuadranglePixel::QuadranglePixel(Quadrangle* quad, OfxPointI p)
: quadrangle(quad), p(p) {
    auto fromPPtr = _fromP;
    for (int i=0; i < 4; i++, fromPPtr++) {
        vectorSubtract(p, quadrangle->edges[i].p, fromPPtr);
    }
    calcIntersection();
}

inline void QuadranglePixel::calculateIdentityPoint(OfxPointD* idP) {
    auto e0 = quadrangle->edges[0].vect;
    auto e1 = quadrangle->edges[1].vect;
    auto e2 = quadrangle->edges[2].vect;
    auto e3 = quadrangle->edges[3].vect;
    auto d = e0.x;
    auto D = e0.y;
    auto f = e1.x;
    auto F = e1.y;
    auto g = e2.x;
    auto G = e2.y;
    auto h = e3.x;
    auto H = e3.y;
    auto q = _fromP[0].x;
    auto Q = _fromP[0].y;

    double denom;

    // https://www.wolframalpha.com/input/?i=solve+u*d+%2B+a*%28%28-h+%2B+u*-g%29+-+u*d%29+%3D+q%2C+u*D+%2B+a*%28%28-H+%2B+u*-G%29+-+u*D%29+%3D+Q%2C+for+u%2C+a
    denom = 2 * (D * g - d * G);
    if (denom != 0) {
        // == X ==
        idP->x = (
            (
                sqrt(
                    pow(
                        -d * H - d * Q + D * h + D * q - g * Q + G * q,
                        2
                    )
                    - 4 * (D * g - d * G) * (H * q - h * Q)
                )
                + d * H + d * Q - D * h - D * q + g * Q - G * q
            )
            / denom
        );

        // == Y ==
        // https://www.wolframalpha.com/input/?i=solve+v*-h+%2B+b*%28%28d+%2B+v*f%29+-+v*-h%29+%3D+q%2C+v*-H+%2B+b*%28%28D+%2B+v*F%29+-+v*-H%29+%3D+Q%2C+for+v%2C+b
        denom = 2 * (F * h - f * H);
        if (denom != 0) {
            idP->y = (
                (
                    sqrt(
                        pow(
                            - d * H + D * h - f * Q + F * q - h * Q + H * q,
                            2
                        )
                        - 4 * (D * q - d * Q) * (F * h - f * H)
                    )
                    + d * H - D * h + f * Q - F * q + h * Q - H * q
                )
                / denom
            );
        }
        // f,F and h,H are parallel
        else {
            // (u[d,D] -> q,Q)'s component length in -h,-H's direction
            // over the length of the line made by u([d,D]) -> [-h,-H] + u([-g,-G])
            // that line:
            // (-h + u*-g) - ud
            // -h - ug - ud
            // -h - u(g + d)
            // result:
            // ((q - ud)*-h + (Q - uD)*-H) / (<-h,-H length> * sqrt((-h - u(g + d))^2 + (-H - u(G + D))^2))
            // (h(ud - q) + H(uD - Q)) / (<-h,-H length> * sqrt((-h - u(g + d))^2 + (-H - u(G + D))^2))
            denom = (
                quadrangle->edges[3].length
                * sqrt(
                    pow(-h - idP->x * (g + d), 2)
                    + pow(-H - idP->x * (G + D), 2)
                )
            );
            assert(denom != 0);
            idP->y = (h * (idP->x * d - q) + H * (idP->x * D - Q)) / denom;
        }
    }
    // d,D and g,G are parallel
    else {
        // == Y ==
        // length of q,Q's component in d,D's normal's direction:
        // (-Dq + dQ) / sqrt(d^2 + D^2)
        // over length of f,F's component in d,D's normal's direction:
        // (-D*f + d*F) / sqrt(d^2 + D^2)
        // denoms cancel out:
        // (-Dq + dQ) / (-Df + dF)
        // (dQ - Dq) / (dF - Df)
        denom = d * F - D * f;
        // d,D not parallel with f,F
        if (denom != 0) {
            idP->y = (d * Q - D * q) / denom;
        }
        // if parallel, it's crushed
        else {
            idP->y = 0.5;
        }

        // == X ==
        // (v[-h,-H] -> q,Q)'s component length in d,D's direction
        // over the length of the vector made by v([-h,-H]) -> [d,D] + v([f,F])
        // that line:
        // ((d + vf) - v*-h)
        // d + v(f + h)
        // result:
        // (d(q - v*-h) + D(Q - v*-H)) / (<length of d,D> * sqrt((d + v(f + h))^2 + (D + v(F + H))^2))
        // (d(q + vh) + D(Q + vH)) / (<length of d,D> * sqrt((d + v(f + h))^2 + (D + v(F + H))^2))
        denom = (
            quadrangle->edges[0].length
            * sqrt(
                pow(d + idP->y * (f + h), 2)
                + pow(D + idP->y * (F + H), 2)
            )
        );
        assert(denom != 0);
        idP->x = (d * (q + idP->y * h) + D * (Q + idP->y * H)) / denom;
    }
}

inline void QuadranglePixel::calcIntersection() {
    // First sweep
    double dists[4];
    int entirelyInsideCount = 0;
    for (int i=0; i < 4; i++) {
        dists[i] = vectorDotProduct(_fromP[i], quadrangle->edges[i].norm);
        // definitely entirely outside
        if (dists[i] <= -M_SQRT2) {
            intersection = 0;
            return;
        }
        // definitely entirely inside
        if (dists[i] >= M_SQRT2) {
            entirelyInsideCount++;
        }
    }
    // definitely entirely inside all
    if (entirelyInsideCount == 4){
        intersection = 1;
        return;
    }

    // Second sweep. Are all points around the pixel inside
    // dot product: dp1 = x1*x2 + y1*y2
    // add something to x1 and y1?
    // dp2 = (x1 + a)*x2 + (y1 + b)*y2
    // dp2 = x1*x2 + y1*y2 + a*x2 + b*y2
    // dp2 = dp1 + a*x2 + b*y2
    bool allInside = true;
    for (int i=0; i < 4 && allInside; i++) {
        for (int j=1; j < 4 && allInside; j++) {
            allInside = (
                dists[i]
                + (j % 3 ? quadrangle->edges[i].norm.x : 0)
                + (j >> 1 ? quadrangle->edges[i].norm.y: 0)
            ) >= 0;
        }
    }
    if (allInside) {
        intersection = 1;
        return;
    }

    // Cut up the pixel
    Polygon polies[2];
    polies[0].addPoint(p);
    OfxPointD nextP;
    // bottom right
    nextP.x = p.x + 1;
    nextP.y = p.y;
    polies[0].addPoint(nextP);
    // top right
    nextP.y += 1;
    polies[0].addPoint(nextP);
    // top left
    nextP.x = p.x;
    polies[0].addPoint(nextP);
    polies[0].close();
    Polygon *outPoly;
    for (int i=0; i < 4; i++) {
        outPoly = polies + 1 - (i % 2);
        outPoly->clear();
        polies[i % 2].cut(&quadrangle->edges[i], outPoly);
    }
    intersection = outPoly->area();
    intersectionPoly = *outPoly;
}

void QuadrangleDistort::bilinear(double x, double y, Image* img, float* outPix, int componentCount) {
    auto floorX = floor(x);
    auto floorY = floor(y);
    double xWeights[2];
    xWeights[1] = x - floorX;
    xWeights[0] = 1 - xWeights[1];
    double yWeights[2];
    yWeights[1] = y - floorY;
    yWeights[0] = 1 - yWeights[1];
    float* pix;
    for (int c=0; c < componentCount; c++) {outPix[c] = 0;}
    for (int y=0; y < 2; y++) {
        for (int x=0; x < 2; x++) {
            pix = (float*)img->getPixelAddressNearest(floorX + x, floorY + y);
            for (int c=0; c < componentCount; c++, pix++) {
                outPix[c] += xWeights[x] * yWeights[y] * *pix;
            }
        }
    }
}
