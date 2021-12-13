#include "CornerPinPluginMacros.h"
#include "CornerPinPlugin.h"
#include "ofxsCoords.h"
#include "../QuadrangleDistort/QuadrangleDistort.h"
#include <iostream>

using namespace QuadrangleDistort;


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

void CornerPinPlugin::getRegionsOfInterest(const RegionsOfInterestArguments &args, RegionOfInterestSetter &rois) {
    rois.setRegionOfInterest(*_srcClip, _srcClip->getRegionOfDefinition(args.time));
}

void CornerPinPlugin::changedParam(const InstanceChangedArgs &args, const std::string &paramName) {
    if (paramName == kParamFix) {
        Quadrangle quad;
        quad.edges[0].p = _bottomLeft->getValueAtTime(args.time);
        quad.edges[1].p = _bottomRight->getValueAtTime(args.time);
        quad.edges[2].p = _topRight->getValueAtTime(args.time);
        quad.edges[3].p = _topLeft->getValueAtTime(args.time);
        quad.initialise();
        quad.fix(NULL, NULL);
        _bottomLeft->setValueAtTime(args.time, quad.edges[0].p);
        _bottomRight->setValueAtTime(args.time, quad.edges[1].p);
        _topRight->setValueAtTime(args.time, quad.edges[2].p);
        _topLeft->setValueAtTime(args.time, quad.edges[3].p);
    }
}

void fromCanonical(OfxPointD p, OfxPointD renderScale, double par, OfxPointD* result) {
    result->x = p.x * renderScale.x / par;
    result->y = p.y * renderScale.y;
}

void toCanonical(OfxPointD p, OfxPointD renderScale, double par, OfxPointD* result) {
    result->x = par * p.x / renderScale.x;
    result->y = p.y / renderScale.y;
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
    quad.initialise();
    if (quad.zeroEdgeCount > 1) {
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

    OfxPointD p;
    OfxPointD srcPD;
    OfxPointI srcPI;
    auto_ptr<float> bilinSrcPix(new float[srcComponentCount]);
    std::vector<std::vector<OfxPointD>> intersections;
    std::vector<OfxPointD> polyPoints;
    OfxPointD canonPolyPoint;

    for (p.y=args.renderWindow.y1; p.y < args.renderWindow.y2; p.y++) {
        auto dstPix = (float*)dstImg->getPixelAddress(args.renderWindow.x1, p.y);
        if (!dstPix) {continue;}
        for (p.x=args.renderWindow.x1; p.x < args.renderWindow.x2; p.x++) {
            auto qPoint = QuadranglePixel(&quad, p);
            // 1917 579
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
