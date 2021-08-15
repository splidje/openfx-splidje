#include "TriangleMaths.h"
#include <iostream>
#include <cmath>
#include <algorithm>

using namespace TriangleMaths;


Triangle::Triangle() {}

Triangle::Triangle(std::vector<OfxPointD>* vertices, long i1, long i2, long i3)
{
    initialise(vertices, i1, i2, i3);
}

void Triangle::initialise(std::vector<OfxPointD>* vertices, long i1, long i2, long i3)
{
    this->i1 = i1;
    this->i2 = i2;
    this->i3 = i3;

    // points
    p1 = (*vertices)[i1];
    p2 = (*vertices)[i2];
    p3 = (*vertices)[i3];

    // bounds
    bounds.x1 = std::floor(std::min(p1.x, std::min(p2.x, p3.x)));
    bounds.x2 = std::ceil(std::max(p1.x, std::max(p2.x, p3.x))) + 1;
    bounds.y1 = std::floor(std::min(p1.y, std::min(p2.y, p3.y)));
    bounds.y2 = std::ceil(std::max(p1.y, std::max(p2.y, p3.y))) + 1;

    // circum circle
    auto ax = p1.x;
    auto ay = p1.y;
    auto bx = p2.x;
    auto by = p2.y;
    auto cx = p3.x;
    auto cy = p3.y;

    auto ab = ax * ax + ay * ay;
    auto cd = bx * bx + by * by;
    auto ef = cx * cx + cy * cy;

    auto circum_x = (ab * (cy - by) + cd * (ay - cy) + ef * (by - ay)) / (ax * (cy - by) + bx * (ay - cy) + cx * (by - ay));
    auto circum_y = (ab * (cx - bx) + cd * (ax - cx) + ef * (bx - ax)) / (ay * (cx - bx) + by * (ax - cx) + cy * (bx - ax));

    circum = {circum_x / 2, circum_y / 2};
    auto dxA = ax - circum.x;
    auto dyA = ay - circum.y;
    circum_radius = dxA * dxA + dyA * dyA;

    // barycentric
    w1Denom = ((p2.y - p3.y) * (p1.x - p3.x) + (p3.x - p2.x) * (p1.y - p3.y));
    w2Denom = ((p2.y - p3.y) * (p1.x - p3.x) + (p3.x - p2.x) * (p1.y - p3.y));
}

bool Triangle::boundsContains(OfxPointD p) {
    return p.x >= bounds.x1 && p.x < bounds.x2 && p.y >= bounds.y1 && p.y < bounds.y2;
}

bool TriangleMaths::operator==(EdgeIndexer e1, EdgeIndexer e2) {
    return (e1.i1 == e2.i1 && e1.i2 == e2.i2
        || e1.i1 == e2.i2 && e1.i2 == e2.i1
    );
}

BarycentricWeights Triangle::toBarycentric(OfxPointD p) {
    BarycentricWeights bw;
    bw.w1 = (
        ((p2.y - p3.y) * (p.x - p3.x) + (p3.x - p2.x) * (p.y - p3.y))
        / w1Denom
    );
    bw.w2 = (
        ((p3.y - p1.y) * (p.x - p3.x) + (p1.x - p3.x) * (p.y - p3.y))
        / w2Denom
    );
    bw.w3 = 1 - bw.w1 - bw.w2;
    return bw;
}

OfxPointD Triangle::fromBarycentric(BarycentricWeights bw) {
    return {
        bw.w1 * p1.x + bw.w2 * p2.x + bw.w3 * p3.x,
        bw.w1 * p1.y + bw.w2 * p2.y + bw.w3 * p3.y
    };
}

bool Triangle::circumCircleContains(OfxPointD p) {
    auto dxP = p.x - circum.x;
    auto dyP = p.y - circum.y;
    auto dist = dxP * dxP + dyP * dyP;
    auto result = dist <= circum_radius;
    return result;
}

std::vector<Triangle> TriangleMaths::delaunay(std::vector<OfxPointD> &vertices)
{
    std::vector<Triangle> triangles;

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
    triangles.push_back(Triangle(&tempVertices, vertices.size(), vertices.size() + 1, vertices.size() + 2));

    for(unsigned long pi = 0; pi < vertices.size(); ++pi) {
        OfxPointD p = vertices[pi];

        std::vector<EdgeIndexer> polygon;

        for (auto t = triangles.begin(); t < triangles.end();) {
            if (t->circumCircleContains(p)) {
                polygon.push_back({t->i1, t->i2});
                polygon.push_back({t->i2, t->i3});
                polygon.push_back({t->i3, t->i1});
                triangles.erase(t);
            }
            else {
                ++t;
            }
        }

        for (auto e1 = polygon.begin(); e1 < polygon.end();) {
            bool isGood = true;
            for (auto e2 = e1 + 1; e2 < polygon.end();) {
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
            triangles.push_back(Triangle(&tempVertices, e.i1, e.i2, pi));
    }

    // remove super triangle
    for (auto t = triangles.begin(); t < triangles.end();) {
        if (t->i1 < vertices.size() && t->i2 < vertices.size() && t->i3 < vertices.size()) {
            ++t;
        }
        else {
            triangles.erase(t);
        }
    }

    return triangles;
}

std::vector<Edge> TriangleMaths::grahamScan(std::vector<OfxPointD> &vertices) {
    // Find lowest y point
    long lowestPointI = 0;
    OfxPointD lowestPoint = vertices[lowestPointI];
    long i = 1;
    for (auto ptr=vertices.begin() + 1; ptr < vertices.end(); ptr++, i++) {
        if (
            ptr->y < lowestPoint.y
            || ptr->y == lowestPoint.y
            && ptr->x < lowestPoint.x
        ) {
            lowestPointI = i;
            lowestPoint = *ptr;
        }
    }

    // calculate angle
    std::vector<std::pair<double, long>> vertAnglePairs(vertices.size() - 1);
    auto vertPtr = vertices.begin();
    auto vertAnglePtr = vertAnglePairs.begin();
    OfxPointD v;
    for (long i=0; vertAnglePtr < vertAnglePairs.end(); vertAnglePtr++, vertPtr++, i++) {
        // skip lowest point
        if (i == lowestPointI) {
            vertAnglePtr--;
            continue;
        }
        vertAnglePtr->second = i;
        v.x = vertPtr->x - lowestPoint.x;
        v.y = vertPtr->y - lowestPoint.y;
        // calculate angle from x-axis
        if (v.x == 0) {
            vertAnglePtr->first = M_PI_2;
        } else {
            vertAnglePtr->first = atan(v.y / v.x);
        }
        if (v.x < 0) {
            vertAnglePtr->first += M_PI;
        } else if (v.y < 0) {
            vertAnglePtr->first += 2 * M_PI;
        }
    }

    // sort in ascending order of angle
    std::sort(vertAnglePairs.begin(), vertAnglePairs.end());

    std::vector<long> stack({lowestPointI});
    for (auto ptr=vertAnglePairs.begin(); ptr < vertAnglePairs.end(); ptr++) {
        while (stack.size() > 1) {
            auto p1 = vertices[*(stack.end() - 2)];
            auto p2 = vertices[*(stack.end() - 1)];
            auto p3 = vertices[ptr->second];
            if (
                ((p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x))
                >= 0
            ) {break;}
            stack.pop_back();
        }
        stack.push_back(ptr->second);
    }

    // create Edges
    std::vector<Edge> edges(stack.size());
    auto edgePtr = edges.begin();
    for (auto ptr=stack.begin(); ptr < stack.end(); ptr++, edgePtr++) {
        auto nextPtr = ptr + 1;
        if (nextPtr == stack.end()) {
            nextPtr = stack.begin();
        }
        edgePtr->initialise(&vertices, *ptr, *nextPtr);
    }

    return edges;
}

Edge::Edge() {}

Edge::Edge(std::vector<OfxPointD>* vertices, long i1, long i2)
{
    initialise(vertices, i1, i2);
}

void Edge::initialise(std::vector<OfxPointD>* vertices, long i1, long i2) {
    indexer.i1 = i1;
    indexer.i2 = i2;
    p1 = (*vertices)[indexer.i1];
    p2 = (*vertices)[indexer.i2];
    bounds.x1 = std::floor(std::min(p1.x, p2.x));
    bounds.x2 = std::ceil(std::max(p1.x, p2.x));
    bounds.y1 = std::floor(std::min(p1.y, p2.y));
    bounds.y2 = std::ceil(std::max(p1.y, p2.y));
    vect.x = p2.x - p1.x;
    vect.y = p2.y - p1.y;
    magnitude = sqrt(vect.x * vect.x + vect.y * vect.y);
    vect.x /= magnitude;
    vect.y /= magnitude;
    norm.x = vect.y;
    norm.y = -vect.x;
}

double Edge::vectComp(OfxPointD p) {
    return (p.x - p1.x) * vect.x + (p.y - p1.y) * vect.y;
}

double Edge::normComp(OfxPointD p) {
    return (p.x - p1.x) * norm.x + (p.y - p1.y) * norm.y;
}
