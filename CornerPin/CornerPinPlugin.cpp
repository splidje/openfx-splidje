#include "CornerPinPluginMacros.h"
#include "CornerPinPlugin.h"
#include "ofxsCoords.h"

// our smallest distance for snapping and
// dealing with rounding errors in calculations.
// Assuming our alpha is a 32 bit float,
// it can only be 0-1.
// there are 126 * 2^23 + 2 possible values
// 1 / sqrt of that is the number of subpixels
// needed to make a 1x1 pixel have that possible area.
#define DELTA 0.000030759


CornerPinPlugin::CornerPinPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
{
    _srcClip = fetchClip(kSourceClip);
    assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentRGB ||
    	    _srcClip->getPixelComponents() == ePixelComponentRGBA));
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentRGB ||
    	    _dstClip->getPixelComponents() == ePixelComponentRGBA));
    _bottomLeft = fetchDouble2DParam(kParamBottomLeft);
    _bottomRight = fetchDouble2DParam(kParamBottomRight);
    _topLeft = fetchDouble2DParam(kParamTopLeft);
    _topRight = fetchDouble2DParam(kParamTopRight);
}

bool CornerPinPlugin::isIdentity(const IsIdentityArguments &args, 
                                  Clip * &identityClip, double &/*identityTime*/
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
)
{
    return false;
}

inline bool vectorEqual(OfxPointD a, OfxPointD b) {
    return a.x == b.x && a.y == b.y;
}

inline void vectorAdd(OfxPointD a, OfxPointD b, OfxPointD* res) {
    res->x = a.x + b.x;
    res->y = a.y + b.y;
}

inline void vectorSubtract(OfxPointD a, OfxPointD b, OfxPointD* res) {
    res->x = a.x - b.x;
    res->y = a.y - b.y;
}

inline void vectorSubtract(OfxPointI a, OfxPointD b, OfxPointD* res) {
    res->x = a.x - b.x;
    res->y = a.y - b.y;
}

inline void vectorDivide(OfxPointD a, double b, OfxPointD* res) {
    res->x = a.x / b;
    res->y = a.y / b;
}

inline double vectorMagSq(OfxPointD a) {
    return a.x * a.x + a.y * a.y;
}

inline void vectorRotate90(OfxPointD a, OfxPointD* res) {
    res->x = -a.y;
    res->y = a.x;
}

inline double vectorDotProduct(OfxPointD a, OfxPointD b) {
    return a.x * b.x + a.y * b.y;
}

class Edge {
    public:

    OfxPointD p;
    OfxPointD vect;
    double length;
    OfxPointD norm;
    bool isInitialised = false;

    inline bool initialise() {
        if (vect.x == 0 && vect.y == 0) {return false;}
        length = sqrt(vectorMagSq(vect));
        if (length == 0) {return false;}
        OfxPointD tang;
        vectorRotate90(vect, &tang);
        vectorDivide(tang, length, &norm);
        isInitialised = true;
        return true;
    }

    inline double crosses(Edge* edge) {
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
        if (result > -DELTA && result < DELTA) {
            result = 0;
        } else if (result > 1 - DELTA && result < 1 + DELTA) {
            result = 1;
        }
        return result;
    }
};

inline double triangleArea(Edge* edges[2]) {
    assert(edges[0]->isInitialised);
    assert(edges[1]->isInitialised);
    // base x height over 2
    auto result = (
        0.5 * edges[0]->length
        * vectorDotProduct(edges[1]->vect, edges[0]->norm)
    );
    return result;
}

class Quadrangle {
    public:

    Edge edges[4];

    inline bool initialise() {
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
};

class Polygon {
    public:

    std::vector<Edge> edges;

    inline void addPoint(OfxPointD p) {
        if (edges.size() > 0) {
            _initialiseLastEdgeVect(p);
        }
        Edge edge;
        edge.p = p;
        edges.push_back(edge);
    }

    inline void addPoint(OfxPointI p) {
        OfxPointD pD;
        pD.x = p.x;
        pD.y = p.y;
        addPoint(pD);
    }

    inline void close() {
        _initialiseLastEdgeVect(edges[0].p);
    }

    inline void clear() {
        edges.clear();
    }

    inline void cut(Edge* cutEdge, Polygon* res) {
        if (edges.size() == 0) {return;}
        // establish whether the first edge's first point is inside
        double insideNess = 0;
        OfxPointD crossPoint;
        for (auto edgeIter=edges.begin(); edgeIter < edges.end(); edgeIter++) {
            auto edge = *edgeIter;
            // lost track of whether we're inside or not
            if (insideNess == 0) {
                insideNess = _calcInsideNess(edge.p, cutEdge);
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

    inline double area() {
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

    private:

    inline void _initialiseLastEdgeVect(OfxPointD toP) {
        auto lastEdge = &edges.back();
        vectorSubtract(toP, lastEdge->p, &lastEdge->vect);
        if (!lastEdge->initialise()) {
            // penultimate edge now becomes last edge.
            // redirect it to this point
            edges.pop_back();
            if (edges.size() > 0) {
                _initialiseLastEdgeVect(toP);
                return;
            }
        }
    }

    static inline double _calcInsideNess(OfxPointD p, Edge* cutEdge) {
        OfxPointD relP;
        vectorSubtract(p, cutEdge->p, &relP);
        auto insideNess = vectorDotProduct(relP, cutEdge->norm);
        if (insideNess > -DELTA && insideNess < DELTA) {
            insideNess = 0;
        }
        return insideNess;
    }
};

class QuadranglePixel {
    public:

    Quadrangle* quadrangle;
    OfxPointI p;
    double intersection;
    Polygon intersectionPoly;

    inline QuadranglePixel(Quadrangle* quad, OfxPointI p)
    : quadrangle(quad), p(p) {
        auto fromPPtr = _fromP;
        for (int i=0; i < 4; i++, fromPPtr++) {
            vectorSubtract(p, quadrangle->edges[i].p, fromPPtr);
        }
        _calcIntersection();
    }

    inline void calculateIdentityPoint(OfxPointD* idP) {
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

    private:

    inline void _calcIntersection() {
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

    OfxPointD _fromP[4];
};

void fromCanonical(OfxPointD p, OfxPointD renderScale, double par, OfxPointD* result) {
    result->x = p.x * renderScale.x / par;
    result->y = p.y * renderScale.y;
}

void toCanonical(OfxPointD p, OfxPointD renderScale, double par, OfxPointD* result) {
    result->x = par * p.x / renderScale.x;
    result->y = p.y / renderScale.y;
}

void bilinear(double x, double y, Image* img, float* outPix, int componentCount) {
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

// the overridden render function
void CornerPinPlugin::render(const RenderArguments &args)
{
    auto_ptr<Image> srcImg(_srcClip->fetchImage(args.time));
    auto srcComponentCount = srcImg->getPixelComponentCount();
    auto_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    auto dstComponentCount = dstImg->getPixelComponentCount();

    auto srcROD = srcImg->getRegionOfDefinition();
    auto width = srcROD.x2 - srcROD.x1;
    auto height = srcROD.y2 - srcROD.y1;

    auto par = srcImg->getPixelAspectRatio();
    Quadrangle quad;
    fromCanonical(_bottomLeft->getValueAtTime(args.time), args.renderScale, par, &quad.edges[0].p);
    fromCanonical(_bottomRight->getValueAtTime(args.time), args.renderScale, par, &quad.edges[1].p);
    fromCanonical(_topRight->getValueAtTime(args.time), args.renderScale, par, &quad.edges[2].p);
    fromCanonical(_topLeft->getValueAtTime(args.time), args.renderScale, par, &quad.edges[3].p);
    if (!quad.initialise()) {
        for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
            auto dstPix = (float*)dstImg->getPixelAddress(args.renderWindow.x1, y);
            for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
                for (int c=0; c < dstComponentCount; c++, dstPix++) {
                    *dstPix = 0;
                }
            }
        }
        return;
    }

    OfxPointI p;
    OfxPointD srcPD;
    OfxPointI srcPI;
    auto_ptr<float> bilinSrcPix(new float[srcComponentCount]);
    std::vector<std::vector<OfxPointD>> intersections;
    std::vector<OfxPointD> polyPoints;
    OfxPointD canonPolyPoint;

    for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        auto dstPix = (float*)dstImg->getPixelAddress(args.renderWindow.x1, y);
        for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            p.x = x;
            p.y = y;
            auto qPoint = QuadranglePixel(&quad, p);
            if (qPoint.intersection > 0) {
                if (qPoint.intersection < 1) {
                    polyPoints.clear();
                    for (
                        auto iter = qPoint.intersectionPoly.edges.begin();
                        iter < qPoint.intersectionPoly.edges.end();
                        iter++
                    ) {
                        toCanonical(iter->p, args.renderScale, par, &canonPolyPoint);
                        polyPoints.push_back(canonPolyPoint);
                    }
                    intersections.push_back(polyPoints);
                }
                qPoint.calculateIdentityPoint(&srcPD);
                srcPI.x = round(srcPD.x * (width - 1) + srcROD.x1);
                srcPI.y = round(srcPD.y * (height - 1) + srcROD.y1);
                bilinear(
                    srcPD.x * (width - 1) + srcROD.x1,
                    srcPD.y * (height - 1) + srcROD.y1,
                    srcImg.get(),
                    bilinSrcPix.get(),
                    srcComponentCount
                );
                for (int c=0; c < dstComponentCount; c++, dstPix++) {
                    if (c == 3) {
                        *dstPix = qPoint.intersection;
                    }
                    else if (c < srcComponentCount) {
                        *dstPix = bilinSrcPix.get()[c];
                        // if (c == 0) {
                        //     *dstPix = srcPD.x;
                        // } else if (c == 1) {
                        //     *dstPix = srcPD.y;
                        // } else {
                        //     *dstPix = 0;
                        // }
                    } else {
                        *dstPix = 0;
                    }                    
                }
            } else {
                for (int i=0; i < dstComponentCount; i++, dstPix++) {
                    *dstPix = 0;
                }
            }
        }
    }
    setIntersections(intersections);
    redrawOverlays();
}

std::vector<std::vector<OfxPointD>> CornerPinPlugin::getIntersections() {
    _intersectionsLock.lock();
    auto result = _intersections;
    _intersectionsLock.unlock();
    return result;
}

void CornerPinPlugin::setIntersections(std::vector<std::vector<OfxPointD>> intersections) {
    _intersectionsLock.lock();
    _intersections.assign(intersections.begin(), intersections.end());
    _intersectionsLock.unlock();
}
