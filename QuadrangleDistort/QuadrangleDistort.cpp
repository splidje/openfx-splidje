#include "QuadrangleDistort.h"
#include <iostream>

using namespace QuadrangleDistort;


bool QuadrangleDistort::vectorEqual(const OfxPointD a, const OfxPointD b) {
    return a.x == b.x && a.y == b.y;
}

void QuadrangleDistort::vectorAdd(const OfxPointD a, const OfxPointD b, OfxPointD* res) {
    res->x = a.x + b.x;
    res->y = a.y + b.y;
}

void QuadrangleDistort::vectorSubtract(const OfxPointD a, const OfxPointD b, OfxPointD* res) {
    res->x = a.x - b.x;
    res->y = a.y - b.y;
}

void QuadrangleDistort::vectorSubtract(const OfxPointI a, const OfxPointD b, OfxPointD* res) {
    res->x = a.x - b.x;
    res->y = a.y - b.y;
}

void QuadrangleDistort::vectorDivide(const OfxPointD a, double b, OfxPointD* res) {
    res->x = a.x / b;
    res->y = a.y / b;
}

double QuadrangleDistort::vectorMagSq(const OfxPointD a) {
    return a.x * a.x + a.y * a.y;
}

void QuadrangleDistort::vectorRotate90(const OfxPointD a, OfxPointD* res) {
    res->x = -a.y;
    res->y = a.x;
}

double QuadrangleDistort::vectorDotProduct(const OfxPointD a, const OfxPointD b) {
    return a.x * b.x + a.y * b.y;
}

void QuadrangleDistort::rectIntersect(const OfxRectI* a, const OfxRectI* b, OfxRectI* res) {
    res->x1 = std::max(a->x1, b->x1);
    res->x2 = std::min(a->x2, b->x2);
    res->y1 = std::max(a->y1, b->y1);
    res->y2 = std::min(a->y2, b->y2);
}

// Edge

bool Edge::initialise() {
    if (
        vect.x >= -QUADRANGLEDISTORT_DELTA
        && vect.x <= QUADRANGLEDISTORT_DELTA
        && vect.y >= -QUADRANGLEDISTORT_DELTA
        && vect.y <= QUADRANGLEDISTORT_DELTA
    ) {
        isInitialised = false;
        return false;
    }
    length = sqrt(vectorMagSq(vect));
    if (length <= QUADRANGLEDISTORT_DELTA) {
        isInitialised = false;
        return false;
    }
    OfxPointD tang;
    vectorRotate90(vect, &tang);
    vectorDivide(tang, length, &norm);
    isInitialised = true;
    return true;
}

double Edge::crosses(const Edge* edge) {
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

double QuadrangleDistort::triangleArea(Edge* edges[2]) {
    assert(edges[0]->isInitialised);
    assert(edges[1]->isInitialised);
    // base x height over 2
    auto result = (
        0.5 * edges[0]->length
        * vectorDotProduct(edges[1]->vect, edges[0]->norm)
    );
    return result;
}

double QuadrangleDistort::calcInsideNess(const OfxPointD p, const Edge* cutEdge) {
    OfxPointD relP;
    vectorSubtract(p, cutEdge->p, &relP);
    auto insideNess = vectorDotProduct(relP, cutEdge->norm);
    if (insideNess > -QUADRANGLEDISTORT_DELTA && insideNess < QUADRANGLEDISTORT_DELTA) {
        insideNess = 0;
    }
    return insideNess;
}

// Quadrangle

void Quadrangle::initialise() {
    Edge* edge = edges;
    zeroEdgeCount = 0;
    _cachedConsts = false;
    for (int i=0; i < 4; i++, edge++) {
        vectorSubtract(
            edges[(i+1) % 4].p,
            edge->p,
            &edge->vect
        );
        if (!edge->initialise()) {
            zeroEdgeCount++;
        }
    }
}

bool Quadrangle::isValid() {
    if (zeroEdgeCount > 1) {
        return false;
    }
    for (auto i=0; i < 4; i++) {
        for (auto j=0; j < 2; j++) {
            auto oppIndex = (i + 2 + j) % 4;
            if (!edges[oppIndex].isInitialised) {
                continue;
            }
            if (calcInsideNess(edges[i].p, &edges[oppIndex]) < 0) {
                return false;
            }
        }
    }
    return true;
}

void Quadrangle::bounds(OfxRectI *rect) {
    rect->x2 = rect->x1 = edges[0].p.x;
    rect->y2 = rect->y1 = edges[0].p.y;
    for (int i=1; i < 4; i++) {
        rect->x1 = std::min(rect->x1, (int)floor(edges[i].p.x));
        rect->x2 = std::max(rect->x2, (int)ceil(edges[i].p.x));
        rect->y1 = std::min(rect->y1, (int)floor(edges[i].p.y));
        rect->y2 = std::max(rect->y2, (int)ceil(edges[i].p.y));
    }
}

void Quadrangle::fix(const std::set<int>* lockedIndices, std::set<int>* changedIndices) {
    assert(!lockedIndices || lockedIndices->count() <= 2);
    if (changedIndices) {
        changedIndices->clear();
    }
    for (auto i=0; i < 4; i++) {
        if (lockedIndices && lockedIndices->find(i) == lockedIndices->end()) {
            continue;
        }
        auto edgePtr = &edges[i];
        for (auto j=0; j < 2; j++) {
            auto oppIndex = (i + 2 + j) % 4;
            auto oppEdgePtr = &edges[oppIndex];
            if (calcInsideNess(edgePtr->p, oppEdgePtr) < 0) {
                auto adjEdgePtr = &edges[(oppIndex + 2) % 4];
                auto crosses = adjEdgePtr->crosses(oppEdgePtr);
                if (crosses >= 0 && crosses <= 1) {
                    edgePtr->p.x = adjEdgePtr->p.x + adjEdgePtr->vect.x * crosses;
                    edgePtr->p.y = adjEdgePtr->p.y + adjEdgePtr->vect.y * crosses;
                } else if (j == 0) {
                    edgePtr->p = edges[(i+5) % 4].p;
                } else if (j == 1) {
                    edgePtr->p = edges[(i+1) % 4].p;
                }
                if (changedIndices) {
                    changedIndices->insert(i);
                }
                initialise();
                break;
            }
        }
    }
}

void Quadrangle::setCurrentPixel(OfxPointD p) {
    _cachedConsts = false;
    _pixelP = p;
    auto fromPPtr = _fromP;
    for (int i=0; i < 4; i++, fromPPtr++) {
        vectorSubtract(p, edges[i].p, fromPPtr);
    }
}

double Quadrangle::calculatePixelIntersection(Polygon* poly) {
    // Minimum valid polygon would be a triangle
    if (zeroEdgeCount > 1) {
        return 0;
    }
    // First sweep
    double dists[4];
    int entirelyInsideCount = 0;
    for (int i=0; i < 4; i++) {
        // a zero edge
        if (!edges[i].isInitialised) {
            entirelyInsideCount++;
            continue;
        }
        dists[i] = vectorDotProduct(_fromP[i], edges[i].norm);
        // definitely entirely outside
        if (dists[i] <= -M_SQRT2) {
            return 0;
        }
        // definitely entirely inside
        if (dists[i] >= M_SQRT2) {
            entirelyInsideCount++;
        }
    }
    // definitely entirely inside all
    if (entirelyInsideCount == 4){
        return 1;
    }

    // Second sweep. Are all points around the pixel inside
    // dot product: dp1 = x1*x2 + y1*y2
    // add something to x1 and y1?
    // dp2 = (x1 + a)*x2 + (y1 + b)*y2
    // dp2 = x1*x2 + y1*y2 + a*x2 + b*y2
    // dp2 = dp1 + a*x2 + b*y2
    bool allInside = true;
    for (int i=0; i < 4 && allInside; i++) {
        if (!edges[i].isInitialised) {
            continue;
        }
        for (int j=0; j < 4 && allInside; j++) {
            allInside = (
                dists[i]
                + (j % 3 ? edges[i].norm.x : 0)
                + (j >> 1 ? edges[i].norm.y: 0)
            ) >= 0;
        }
    }
    if (allInside) {
        return 1;
    }

    // Cut up the pixel
    Polygon polies[2];
    polies[0].addPoint(_pixelP);
    OfxPointD nextP;
    // bottom right
    nextP.x = _pixelP.x + 1;
    nextP.y = _pixelP.y;
    polies[0].addPoint(nextP);
    // top right
    nextP.y += 1;
    polies[0].addPoint(nextP);
    // top left
    nextP.x = _pixelP.x;
    polies[0].addPoint(nextP);
    polies[0].close();
    Polygon *inPoly = polies;
    Polygon *outPoly = polies + 1;
    for (int i=0; i < 4; i++) {
        if (edges[i].isInitialised) {
            continue;
        }
        outPoly->clear();
        inPoly->cut(&edges[i], outPoly);
        std::swap(inPoly, outPoly);
    }
    if (poly) {
        *poly = *outPoly;
    }
    return outPoly->area();
}

void Quadrangle::calculatePixelIdentity(OfxPointD* idP) {
    if (!_cachedConsts) {
        _cacheConsts();
    }

    double denom;

    // https://www.wolframalpha.com/input/?i=solve+u*d+%2B+a*%28%28-h+%2B+u*-g%29+-+u*d%29+%3D+q%2C+u*D+%2B+a*%28%28-H+%2B+u*-G%29+-+u*D%29+%3D+Q%2C+for+u%2C+a
    if (_denom1 < -QUADRANGLEDISTORT_DELTA || _denom1 > QUADRANGLEDISTORT_DELTA) {
        // == X ==
        idP->x = (
            (
                sqrt(
                    pow(
                        -_d * _H - _d * _Q + _D * _h + _D * _q - _g * _Q + _G * _q,
                        2
                    )
                    - 4 * (_D * _g - _d * _G) * (_H * _q - _h * _Q)
                )
                + _d * _H + _d * _Q - _D * _h - _D * _q + _g * _Q - _G * _q
            )
            / _denom1
        );

        // == Y ==
        // https://www.wolframalpha.com/input/?i=solve+v*-h+%2B+b*%28%28d+%2B+v*f%29+-+v*-h%29+%3D+q%2C+v*-H+%2B+b*%28%28D+%2B+v*F%29+-+v*-H%29+%3D+Q%2C+for+v%2C+b
        if (_denom2 < -QUADRANGLEDISTORT_DELTA || _denom2 > QUADRANGLEDISTORT_DELTA) {
            idP->y = (
                (
                    sqrt(
                        pow(
                            - _d * _H + _D * _h - _f * _Q + _F * _q - _h * _Q + _H * _q,
                            2
                        )
                        - 4 * (_D * _q - _d * _Q) * (_F * _h - _f * _H)
                    )
                    + _d * _H - _D * _h + _f * _Q - _F * _q + _h * _Q - _H * _q
                )
                / _denom2
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
                edges[(3 + _orient) % 4].length
                * sqrt(
                    pow(-_h - idP->x * (_g + _d), 2)
                    + pow(-_H - idP->x * (_G + _D), 2)
                )
            );
            assert(denom != 0);
            idP->y = (_h * (idP->x * _d - _q) + _H * (idP->x * _D - _Q)) / denom;
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
        // d,D not parallel with f,F
        if (_denom3 < -QUADRANGLEDISTORT_DELTA || _denom3 > QUADRANGLEDISTORT_DELTA) {
            idP->y = (_d * _Q - _D * _q) / _denom3;
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
            edges[_orient].length
            * sqrt(
                pow(_d + idP->y * (_f + _h), 2)
                + pow(_D + idP->y * (_F + _H), 2)
            )
        );
        assert(denom != 0);
        idP->x = (_d * (_q + idP->y * _h) + _D * (_Q + idP->y * _H)) / denom;
    }

    // fix orientation
    if (!_orient) {
        return;
    }
    auto y = idP->y;
    if (_orient == 1) {
        idP->y = idP->x;
        idP->x = 1 - y;
    } else if (_orient == 2) {
        idP->y = 1 - y;
        idP->x = 1 - idP->x;
    } else if (_orient == 3) {
        idP->y = 1 - idP->x;
        idP->x = y;
    }
}

void Quadrangle::_cacheConsts() {
    assert(zeroEdgeCount <= 1);
    _orient = 0;
    // if we have a zero edge and it's not the 2nd,
    // we'll need to reorient
    if (zeroEdgeCount && edges[1].isInitialised) {
        if (!edges[2].isInitialised) {
            _orient = 1;
        } else if (!edges[3].isInitialised) {
            _orient = 2;
        } else if (!edges[0].isInitialised) {
            _orient = 3;
        }
    }
    _e0 = edges[_orient].vect;
    _e1 = edges[(1 + _orient) % 4].vect;
    _e2 = edges[(2 + _orient) % 4].vect;
    _e3 = edges[(3 + _orient) % 4].vect;
    _d = _e0.x;
    _D = _e0.y;
    _f = _e1.x;
    _F = _e1.y;
    _g = _e2.x;
    _G = _e2.y;
    _h = _e3.x;
    _H = _e3.y;
    _q = _fromP[_orient].x;
    _Q = _fromP[_orient].y;

    // https://www.wolframalpha.com/input/?i=solve+u*d+%2B+a*%28%28-h+%2B+u*-g%29+-+u*d%29+%3D+q%2C+u*D+%2B+a*%28%28-H+%2B+u*-G%29+-+u*D%29+%3D+Q%2C+for+u%2C+a
    _denom1 = 2 * (_D * _g - _d * _G);

    // https://www.wolframalpha.com/input/?i=solve+v*-h+%2B+b*%28%28d+%2B+v*f%29+-+v*-h%29+%3D+q%2C+v*-H+%2B+b*%28%28D+%2B+v*F%29+-+v*-H%29+%3D+Q%2C+for+v%2C+b
    _denom2 = 2 * (_F * _h - _f * _H);

    // length of q,Q's component in d,D's normal's direction:
    // (-Dq + dQ) / sqrt(d^2 + D^2)
    // over length of f,F's component in d,D's normal's direction:
    // (-D*f + d*F) / sqrt(d^2 + D^2)
    // denoms cancel out:
    // (-Dq + dQ) / (-Df + dF)
    // (dQ - Dq) / (dF - Df)
    _denom3 = _d * _F - _D * _f;
}


// Polygon

void Polygon::addPoint(OfxPointD p) {
    if (edges.size() > 0) {
        initialiseLastEdgeVect(p);
    }
    Edge edge;
    edge.p = p;
    edges.push_back(edge);
}

void Polygon::addPoint(OfxPointI p) {
    OfxPointD pD;
    pD.x = p.x;
    pD.y = p.y;
    addPoint(pD);
}

void Polygon::close() {
    initialiseLastEdgeVect(edges[0].p);
}

void Polygon::clear() {
    edges.clear();
}

void Polygon::cut(Edge* cutEdge, Polygon* res) {
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

double Polygon::area() {
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

void Polygon::initialiseLastEdgeVect(OfxPointD toP) {
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
