#include "BoneWarpPlugin.h"
#include "BoneWarpMacros.h"
#include "ofxsCoords.h"
#include <iostream>


BoneWarpPlugin::BoneWarpPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
{
    _srcClip = fetchClip(kSourceClip);
    assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentRGB ||
    	    _srcClip->getPixelComponents() == ePixelComponentRGBA));
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentRGB ||
    	    _dstClip->getPixelComponents() == ePixelComponentRGBA));
    for (int i = 0; i < 2; i++) {
        _jointFromCentres[i] = fetchDouble2DParam(kParamJointFromCentre(i));
        _jointToCentres[i] = fetchDouble2DParam(kParamJointToCentre(i));
        _jointRadii[i] = fetchDoubleParam(kParamJointRadius(i));
    }
}

bool BoneWarpPlugin::isIdentity(const IsIdentityArguments &args, 
                                  Clip * &identityClip, double &/*identityTime*/
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
)
{
    bool same = true;
    for (int i=0; i < 2; i++){
        auto fromCentre = _jointFromCentres[i]->getValueAtTime(args.time);
        auto toCentre = _jointToCentres[i]->getValueAtTime(args.time);
        if (fromCentre.x != toCentre.x || fromCentre.y != toCentre.y) {
            same = false;
            break;
        }
        if (!same) {break;}
    }
    if (same) {
        identityClip = _srcClip;
        return true;
    }
    return false;
}

inline bool vectorEquals(OfxPointD a, OfxPointD b) {
    return a.x == b.x && a.y == b.y;
}

inline OfxPointD vectorAdd(OfxPointD a, OfxPointD b) {
    OfxPointD res;
    res.x = a.x + b.x;
    res.y = a.y + b.y;
    return res;
}

inline OfxPointD vectorSubtract(OfxPointD a, OfxPointD b) {
    OfxPointD res;
    res.x = a.x - b.x;
    res.y = a.y - b.y;
    return res;
}

inline OfxPointD vectorMultiply(OfxPointD a, double multiplier) {
    OfxPointD res;
    res.x = a.x * multiplier;
    res.y = a.y * multiplier;
    return res;
}

inline OfxPointD vectorDivide(OfxPointD a, double denom) {
    OfxPointD res;
    res.x = a.x / denom;
    res.y = a.y / denom;
    return res;
}

inline double dotProduct(OfxPointD a, OfxPointD b) {
    return a.x * b.x + a.y * b.y;
}

inline double magnitudeSq(OfxPointD a) {
    return a.x * a.x + a.y * a.y;
}

inline double magnitude(OfxPointD a) {
    return sqrt(magnitudeSq(a));
}

inline OfxPointD normalise(OfxPointD a) {
    return vectorDivide(a, magnitude(a));
}

class Sausage {
    public:

    OfxPointD p1, p2;
    double radius;

    Sausage(OfxPointD p1, OfxPointD p2, double radius)
        : p1(p1), p2(p2), radius(radius) {
            _p1p2 = vectorSubtract(p2, p1);
            _dSq = magnitudeSq(_p1p2);
            _d = sqrt(_dSq);
            _p1p2Norm = vectorDivide(_p1p2, _d);
            _p1p2NormTang.x = -_p1p2Norm.y;
            _p1p2NormTang.y = _p1p2Norm.x;
            _radiusSq = radius * radius;
    }

    inline bool isInside(OfxPointD a) {
        if (_isInsideRect(a)) {
            return true;
        }
        return _isInsideOneCircle(a);
    }

    inline OfxPointD lineFromP2Intersects(OfxPointD a) {
        auto vect = vectorSubtract(a, p2);
        auto dist = dotProduct(vect, _p1p2Norm);
        // outside of rect, p2's side.
        // find where crosses p2's circle
        if (dist >= 0) {
            return _lineFromP2IntersectsP2Circle(vect);
        }
        auto dFromSpine = abs(dotProduct(vect, _p1p2NormTang));
        // we're on the spine, must mean we intersect circle at p1
        if (!dFromSpine) {
            // return a;
            return _lineFromP2IntersectsP1Circle(vect);
        }
        // How much to scale vect to hit a side
        auto toEdgeMult = (radius / dFromSpine);
        // Would this scale take us beyond the end at p1?
        if (toEdgeMult > (_d / -dist)) {
            // return a;
            return _lineFromP2IntersectsP1Circle(vect);
        }
        // we can happily hit the side
        return vectorAdd(
            p2,
            vectorMultiply(vect, toEdgeMult)
        );
    }

    private:

    OfxPointD _p1p2, _p1p2Norm, _p1p2NormTang;
    double _dSq, _d, _radiusSq;

    inline bool _isInsideRect(OfxPointD a) {
        // right side and distance from p1?
        auto ap1 = vectorSubtract(a, p1);
        auto p1p2Comp = dotProduct(ap1, _p1p2Norm);
        if (p1p2Comp < 0 || p1p2Comp > _d) {
            return false;
        }
        // so far so good. Right distance from line?
        return abs(dotProduct(ap1, _p1p2NormTang)) <= radius;
    }

    inline bool _isInsideOneCircle(OfxPointD a) {
        return (
            magnitudeSq(vectorSubtract(a, p1)) <= _radiusSq
            || magnitudeSq(vectorSubtract(a, p2)) <= _radiusSq
        );
    }

    inline OfxPointD _lineFromP2IntersectsP2Circle(OfxPointD vect) {
        return vectorAdd(
            p2,
            vectorMultiply(vect, radius / magnitude(vect))
        );
    }

    inline OfxPointD _lineFromP2IntersectsP1Circle(OfxPointD vect) {
        // from: http://www.ambrsoft.com/TrigoCalc/Circles2/circlrLine_.htm
        OfxPointD res = vectorAdd(p2, vect);
        if (!vect.x) {
            return res;
        }
        // eq for line:
        // (res.x - p2.x) / vect.x == (res.y - p2.y) / vect.y
        // res.y = vect.y * (res.x - p2.x) / vect.x + p2.y
        // res.y = (vect.y / vect.x) * res.x - p2.x * vect.y / vect.x + p2.y
        auto m = vect.y / vect.x;
        auto d = p2.y - p2.x * vect.y / vect.x;
        // eq for p1 circle:
        // (res.x - p1.x)^2 + (res.y - p1.y)^2 = radius^2
        auto a = p1.x;
        auto b = p1.y;
        auto thetaLastBit = (b - m * a - d);
        auto theta = radius * radius * (1 + m * m) - thetaLastBit * thetaLastBit;
        // std::cout << theta << std::endl;
        auto thetaSqrt = sqrt(theta);
        if (!(1 + m * m)) {
            return res;
        }
        double maxD = -1;
        double minRadDiff = -1;
        OfxPointD cand;
        double multY = 1;
        for (int y = 0; y < 2; y++, multY*=-1) {
            double multX = 1;
            for (int x = 0; x < 2; x++, multX*=-1) {
                cand.x = (a + b * m - d * m + multX * thetaSqrt) / (1 + m * m);
                cand.y = (d + a * m + b * m * m + multY * thetaSqrt) / (1 + m * m);
                auto candD = magnitude(vectorSubtract(cand, p2));
                if (maxD == -1 || candD > maxD) {
                    auto candRadDiff = abs(
                        magnitudeSq(vectorSubtract(cand, p1)) - _radiusSq
                    );
                    if (minRadDiff == -1 || candRadDiff < minRadDiff) {
                        minRadDiff = candRadDiff;
                        maxD = candD;
                        res = cand;
                    }
                }
            }
        }
        return res;
    }
};

void BoneWarpPlugin::render(const OFX::RenderArguments &args) {
    OfxPointD fromCentres[2];
    OfxPointD toCentres[2];
    double radii[2];

    auto par = _srcClip->getPixelAspectRatio();
    auto numComponents = _srcClip->getPixelComponentCount();

    for (int i=0; i < 2; i++) {
        Coords::toPixelSub(
            _jointFromCentres[i]->getValueAtTime(args.time), args.renderScale, par, fromCentres + i
        );
        Coords::toPixelSub(
            _jointToCentres[i]->getValueAtTime(args.time), args.renderScale, par, toCentres + i
        );
        // TODO: model an ellipse to support x,y render scale being different
        radii[i] = _jointRadii[i]->getValueAtTime(args.time) * args.renderScale.x;
    }

    Sausage sausage(fromCentres[0], toCentres[0], radii[0]);

    auto_ptr<Image> srcImg(_srcClip->fetchImage(args.time));
    auto_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    
    OfxPointD p;
    OfxPointD srcCoords;
    OfxPointD edgeCoords;
    OfxPointD srcCoordsFloor;
    OfxPointD floorWeight;
    float* dstPIX;
    float* srcFloorPIX;
    float* srcFloorX1PIX;
    float* srcFloorY1PIX;
    float* srcFloorXY1PIX;

    for (auto y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (auto x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            dstPIX = (float*)dstImg->getPixelAddressNearest(x, y);

            p.x = x;
            p.y = y;

            if (sausage.isInside(p)) {
                if (vectorEquals(p, sausage.p2)) {
                    srcCoords = sausage.p1;
                }
                else {
                    edgeCoords = sausage.lineFromP2Intersects(p);
                    auto proportion = (
                        magnitude(vectorSubtract(p, sausage.p2))
                        / magnitude(vectorSubtract(edgeCoords, sausage.p2))
                    );
                    srcCoords = vectorAdd(
                        sausage.p1,
                        vectorMultiply(
                            vectorSubtract(edgeCoords, sausage.p1),
                            proportion
                        )
                    );
                }
            }
            else {
                srcCoords = p;
            }
            srcCoordsFloor.x = floor(srcCoords.x);
            srcCoordsFloor.y = floor(srcCoords.y);
            // if (x == 1280 && y >= 836 && y <= 836) {
            //     std::cout << srcCoords.x << " " << srcCoords.y << std::endl;
            //     std::cout << srcCoordsFloor.x << " " << srcCoordsFloor.y << std::endl;
            // }
            srcFloorPIX = (float*)srcImg->getPixelAddressNearest(
                srcCoordsFloor.x, srcCoordsFloor.y
            );
            if (srcCoordsFloor.x == srcCoords.x && srcCoordsFloor.y == srcCoords.y) {
                for (int c=0; c < numComponents; c++, srcFloorPIX++, dstPIX++) {
                    *dstPIX = *srcFloorPIX;
                }
            }
            else {
                floorWeight.x = 1 - (
                    srcCoords.x - srcCoordsFloor.x
                );
                floorWeight.y = 1 - (
                    srcCoords.y - srcCoordsFloor.y
                );
                if (x == 1651 && y >= 332 && y <= 334) {
                    std::cout << floorWeight.x << " " << floorWeight.y << std::endl;
                }
                srcFloorX1PIX = (float*)srcImg->getPixelAddressNearest(
                    srcCoordsFloor.x + 1, srcCoordsFloor.y
                );
                srcFloorY1PIX = (float*)srcImg->getPixelAddressNearest(
                    srcCoordsFloor.x, srcCoordsFloor.y + 1
                );
                srcFloorXY1PIX = (float*)srcImg->getPixelAddressNearest(
                    srcCoordsFloor.x + 1, srcCoordsFloor.y + 1
                );
                for (int c=0; c < numComponents; c++, srcFloorPIX++, srcFloorX1PIX++, srcFloorY1PIX++, srcFloorXY1PIX++, dstPIX++) {
                    auto bottom = *srcFloorPIX * floorWeight.x + *srcFloorX1PIX * (1 - floorWeight.x);
                    auto top = *srcFloorY1PIX * floorWeight.x + *srcFloorXY1PIX * (1 - floorWeight.x);
                    *dstPIX = bottom * floorWeight.y + top * (1 - floorWeight.y);
                }
            }
        }
    }
}
