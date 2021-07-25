#include "TranslateMapPlugin.h"
#include "ofxsCoords.h"
#include "../QuadrangleDistort/QuadrangleDistort.h"

using namespace QuadrangleDistort;


TranslateMapPlugin::TranslateMapPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
{
    _transClip = fetchClip(kTranslationsClip);
    assert(_transClip && (_transClip->getPixelComponents() == ePixelComponentRGB ||
    	    _transClip->getPixelComponents() == ePixelComponentRGBA));
    _srcClip = fetchClip(kSourceClip);
    assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentRGB ||
    	    _srcClip->getPixelComponents() == ePixelComponentRGBA));
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentRGB ||
    	    _dstClip->getPixelComponents() == ePixelComponentRGBA));
}

void TranslateMapPlugin::getClipPreferences(ClipPreferencesSetter &clipPreferences) {
    clipPreferences.setClipComponents(*_dstClip, _srcClip->getPixelComponents());
}

bool TranslateMapPlugin::isIdentity(const IsIdentityArguments &args, 
                                  Clip * &identityClip, double &/*identityTime*/
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
)
{
    if (_srcClip->isConnected() && !_transClip->isConnected()) {
        identityClip = _srcClip;
        return true;
    } else {
        return false;
    }
}

void TranslateMapPlugin::getRegionsOfInterest(const RegionsOfInterestArguments &args, RegionOfInterestSetter &rois) {
    rois.setRegionOfInterest(*_srcClip, _srcClip->getRegionOfDefinition(args.time));
    rois.setRegionOfInterest(*_transClip, _transClip->getRegionOfDefinition(args.time));
}

inline void addPixelValue(OfxPointD p, float* values, int componentCount, double weight, Image* outImg, double* ratioSums, OfxRectI window) {
    int floorX = floor(p.x);
    int floorY = floor(p.y);
    float* PIX;
    auto width = window.x2 - window.x1;
    auto height = window.y2 - window.y1;
    float* valuePtr;
    if (p.x == floorX && p.y == floorY) {
        if (
            floorX < window.x1
            || floorX >= window.x2
            || floorY < window.y1
            || floorY >= window.y2
        ) {return;}
        PIX = (float*)outImg->getPixelAddress(floorX, floorY);
        valuePtr = values;
        for (int c=0; c < componentCount; c++, PIX++, valuePtr++) {
            *PIX += *valuePtr;
        }
        ratioSums[
            (floorY - window.y1) * width + (floorX - window.x1)
        ]++;
        return;
    }

    // we actually need to affect neighbouring pixels
    auto weightX = p.x - floorX;
    auto weightY = p.y - floorY;
    int actualX, actualY;
    double finalWeight;
    for (int y=0; y < 2; y++) {
        for (int x=0; x < 2; x++) {
            actualX = floorX + x;
            actualY = floorY + y;
            if (
                actualX < window.x1
                || actualX >= window.x2
                || actualY < window.y1
                || actualY >= window.y2
            ) {continue;}
            finalWeight = (
                (x == 0 ? 1 - weightX : weightX)
                * (y == 0 ? 1 - weightY : weightY)
                * weight
            );
            PIX = (float*)outImg->getPixelAddress(actualX, actualY);
            valuePtr = values;
            for (int c=0; c < componentCount; c++, PIX++, valuePtr++) {
                *PIX += *valuePtr * finalWeight;
            }
            ratioSums[
                (actualY - window.y1) * width + (actualX - window.x1)
            ] += finalWeight;
        }
    }
}

// the overridden render function
void TranslateMapPlugin::render(const RenderArguments &args)
{
    auto_ptr<Image> transImg(
        _transClip->fetchImage(args.time, _transClip->getRegionOfDefinition(args.time))
    );
    if (!transImg.get()) {return;}
    auto transROD = transImg->getRegionOfDefinition();
    auto trans_component_count = transImg->getPixelComponentCount();
    if (trans_component_count < 2) {return;}
    auto_ptr<Image> srcImg(_srcClip->fetchImage(args.time));
    auto srcROD = srcImg->getRegionOfDefinition();
    auto srcComponentCount = srcImg->getPixelComponentCount();
    auto srcPar = srcImg->getPixelAspectRatio();
    auto_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    auto dstComponentCount = dstImg->getPixelComponentCount();
    auto componentCount = std::min(
        srcComponentCount, dstComponentCount
    );

    float* outPIX;

    // start entire output at 0
    // initialise all the sums of ratios for each output pixel
    auto windowWidth = args.renderWindow.x2 - args.renderWindow.x1;
    auto windowHeight = args.renderWindow.y2 - args.renderWindow.y1;
    auto_ptr<double> ratioSums(new double[windowHeight * windowWidth]);
    auto ratioSumsPtr = ratioSums.get();
    for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++, ratioSumsPtr++) {
            *ratioSumsPtr = 0;
            outPIX = (float*)dstImg->getPixelAddress(x, y);
            for (int c=0; c < dstComponentCount; c++, outPIX++) {
                *outPIX = 0;
            }
        }
    }

    OfxPointD p;
    OfxPointD transVect;
    OfxPointD prevTransVect;
    OfxPointD cornerPoint;
    bool allSame;
    Quadrangle quad;
    OfxRectI quadBounds;
    OfxRectI intersectBounds;
    OfxPointD transPoint;
    float* transPIX;
    OfxPointD srcPoint;
    double intersection;
    auto_ptr<float> values(new float[componentCount]);
    for (p.y=srcROD.y1; p.y < srcROD.y2; p.y++) {
        for (p.x=srcROD.x1; p.x < srcROD.x2; p.x++) {
            // establish quadrangle points
            allSame = true;
            auto edgePtr = quad.edges;
            for (int i=0; i < 4; i++, edgePtr++) {
                cornerPoint.x = p.x + (i % 3); // 1 and 2
                cornerPoint.y = p.y + (i >> 1); // 2 and 3
                transPIX = (float*)transImg->getPixelAddressNearest(
                    cornerPoint.x, cornerPoint.y
                );
                transVect.x = transPIX[0] * args.renderScale.x / srcPar;
                transVect.y = transPIX[1] * args.renderScale.y;
                if (i > 0) {
                    // so far all same and same as last one
                    allSame = (
                        allSame
                        && transVect.x == prevTransVect.x
                        && transVect.y == prevTransVect.y
                    );
                }
                prevTransVect = transVect;
                vectorAdd(cornerPoint, transVect, &edgePtr->p);
            }

            // all the same? There's no distortion,
            // we just know whence to draw this pixel
            // Also if the quad is invalid, do the same.
            // I have good reason to believe initialise won't be
            // call if allSame is true
            if (allSame || !quad.initialise()) {
                auto srcPIX = (float*)srcImg->getPixelAddressNearest(p.x, p.y);
                addPixelValue(
                    quad.edges[0].p, srcPIX, componentCount, 1,
                    dstImg.get(), ratioSums.get(), args.renderWindow
                );
                continue;
            }

            // it's distorted, let's bblaaay
            // go through every pixel inside the smallest rect
            // containing this quadrangle, intersected with render window
            quad.bounds(&quadBounds);
            rectIntersect(&quadBounds, &args.renderWindow, &intersectBounds);
            for (transPoint.y=intersectBounds.y1; transPoint.y < intersectBounds.y2; transPoint.y++) {
                for (transPoint.x=intersectBounds.x1; transPoint.x < intersectBounds.x2; transPoint.x++) {
                    QuadranglePixel quadPix(&quad, transPoint);
                    if (quadPix.intersection <= 0) {continue;}
                    quadPix.calculateIdentityPoint(&srcPoint);
                    if (
                        IsNaN(srcPoint.x)
                        || IsNaN(srcPoint.y)
                        || srcPoint.x < 0
                        || srcPoint.x >= 1
                        || srcPoint.y < 0
                        || srcPoint.y >= 1
                    ) {continue;}
                    srcPoint.x += p.x;
                    srcPoint.y += p.y;
                    bilinear(
                        srcPoint.x, srcPoint.y, srcImg.get(), values.get(), componentCount
                    );
                    addPixelValue(
                        transPoint, values.get(), componentCount, quadPix.intersection,
                        dstImg.get(), ratioSums.get(), args.renderWindow
                    );
                }
            }
        }
    }

    ratioSumsPtr = ratioSums.get();
    for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++, ratioSumsPtr++) {
            if (*ratioSumsPtr == 0) {continue;}
            outPIX = (float*)dstImg->getPixelAddress(x, y);
            for (int c=0; c < componentCount; c++, outPIX++) {
                *outPIX /= *ratioSumsPtr;
            }
        }
    }
}
