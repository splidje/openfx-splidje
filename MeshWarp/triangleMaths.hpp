#include "ofxCore.h"

namespace TriangleMaths {
    typedef struct Triangle {
        OfxPointD p1, p2, p3;
    } Triangle;

    typedef struct TriIndexer {
        unsigned int i1, i2, i3;
    } TriIndexer;

    typedef struct Edge {
        OfxPointD p1, p2;
    } Edge;

    typedef struct EdgeIndexer {
        unsigned int i1, i2;
    } EdgeIndexer;

    inline bool operator==(EdgeIndexer e1, EdgeIndexer e2) {
        return (e1.i1 == e2.i1 && e1.i2 == e2.i2
            || e1.i1 == e2.i2 && e1.i2 == e2.i1
        );
    }

    typedef struct BarycentricWeights {
        double w1, w2, w3;
    } BarycentricWeights;

    inline Triangle indexerToTri(TriIndexer idx, std::vector<OfxPointD> verticies) {
        return {verticies[idx.i1], verticies[idx.i2], verticies[idx.i3]};
    }

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

    inline bool circumCircleContains(Triangle tri, OfxPointD p) {
        const double ax = tri.p1.x;
        const double ay = tri.p1.y;
        const double bx = tri.p2.x;
        const double by = tri.p2.y;
        const double cx = tri.p3.x;
        const double cy = tri.p3.y;

        const double ab = ax * ax + ay * ay;
        const double cd = bx * bx + by * by;
        const double ef = cx * cx + cy * cy;

        const double circum_x = (ab * (cy - by) + cd * (ay - cy) + ef * (by - ay)) / (ax * (cy - by) + bx * (ay - cy) + cx * (by - ay));
        const double circum_y = (ab * (cx - bx) + cd * (ax - cx) + ef * (bx - ax)) / (ay * (cx - bx) + by * (ax - cx) + cy * (bx - ax));

        const OfxPointD circum = {circum_x / 2, circum_y / 2};
        const double dxA = ax - circum.x;
        const double dyA = ay - circum.y;
        const double circum_radius = dxA * dxA + dyA * dyA;
        const double dxP = p.x - circum.x;
        const double dyP = p.y - circum.y;
        const double dist = dxP * dxP + dyP * dyP;
        return dist <= circum_radius;
    }
    
    std::vector<TriIndexer> delaunay(std::vector<OfxPointD> &vertices)
    {
        std::vector<TriIndexer> triangles;

        // Determinate the super triangle
        double minX = vertices[0].x;
        double minY = vertices[0].y;
        double maxX = minX;
        double maxY = minY;

        for(std::size_t i = 0; i < vertices.size(); ++i) {
            if (vertices[i].x < minX) minX = vertices[i].x;
            if (vertices[i].y < minY) minY = vertices[i].y;
            if (vertices[i].x > maxX) maxX = vertices[i].x;
            if (vertices[i].y > maxY) maxY = vertices[i].y;
        }

        const double dx = maxX - minX;
        const double dy = maxY - minY;
        const double deltaMax = std::max(dx, dy);
        const double midx = (minX + maxX) / 2;
        const double midy = (minY + maxY) / 2;

        const OfxPointD p1 = {midx - 20 * deltaMax, midy - deltaMax};
        const OfxPointD p2 = {midx, midy + 20 * deltaMax};
        const OfxPointD p3 = {midx + 20 * deltaMax, midy - deltaMax};

        std::vector<OfxPointD> tempVertices;
        tempVertices.insert(tempVertices.begin(), vertices.begin(), vertices.end());
        tempVertices.push_back(p1);
        tempVertices.push_back(p2);
        tempVertices.push_back(p3);

        // Create a list of triangles, and add the supertriangle in it
        triangles.push_back({vertices.size(), vertices.size() + 1, vertices.size() + 2});

        for(int pi = 0; pi < vertices.size(); ++pi) {
            OfxPointD p = vertices[pi];

            std::vector<EdgeIndexer> polygon;

            for (auto t = begin(triangles); t != end(triangles);) {
                if (circumCircleContains(
                    indexerToTri(*t, vertices)
                    ,p
                )) {
                    polygon.push_back({t->i1, t->i2});
                    polygon.push_back({t->i2, t->i3});
                    polygon.push_back({t->i3, t->i1});
                    triangles.erase(t);
                }
                else {
                    ++t;
                }
            }

            for (auto e1 = begin(polygon); e1 != end(polygon);) {
                bool isGood = true;
                for (auto e2 = e1 + 1; e2 != end(polygon);) {
                    if (*e1 == *e2) {
                        isGood = false;
                        polygon.erase(e2);
                    }
                    else {
                        ++e2;
                    }
                }
                if (isGood) {
                    ++e1;
                }
                else {
                    polygon.erase(e1);
                }
            }

            for(const auto e : polygon)
                triangles.push_back({e.i1, e.i2, pi});
        }

        // remove super triangle
        for (auto t = begin(triangles); t != end(triangles);) {
            if (t->i1 < vertices.size() && t->i2 < vertices.size() && t->i3 < vertices.size()) {
                ++t;
            }
            else {
                triangles.erase(t);
            }
        }

        return triangles;
    }
}
