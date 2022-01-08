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
    _approximate = fetchBooleanParam(kParamApproximate);
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

typedef std::vector<Quadrangle> quad_list_t;

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
    auto dstROD = dstImg->getRegionOfDefinition();
    auto dstComponentCount = dstImg->getPixelComponentCount();
    auto componentCount = std::min(
        srcComponentCount, dstComponentCount
    );

    float* outPIX;

    auto approximate = _approximate->getValueAtTime(args.time);

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

    // go through trans rendering quads
    Quadrangle quad;
    OfxRectI quadBounds;
    OfxRectI quadIntersection;
    std::set<int> locked = {0};
    auto_ptr<float> srcQuadPix(new float[4 * srcComponentCount]);
    Polygon intersectionPoly;
    OfxPointD identityP;
    for (auto y=transROD.y1; y < transROD.y2; y++) {
        for (auto x=transROD.x1; x < transROD.x2; x++) {
            if (abort()) {return;}
            if (approximate) {
                auto transPix = (float*)transImg->getPixelAddressNearest(x, y);
                int dstX = round(x + transPix[0] * args.renderScale.x / srcPar);
                int dstY = round(y + transPix[1] * args.renderScale.y);
                if (dstX < dstROD.x1 || dstX >= dstROD.x2 || dstY < dstROD.y1 || dstY >= dstROD.y2) {
                    continue;
                }
                auto srcPix = (float*)srcImg->getPixelAddressNearest(x, y);
                auto dstPix = (float*)dstImg->getPixelAddressNearest(dstX, dstY);
                ratioSums.get()[(
                    (dstY - args.renderWindow.y1) * windowWidth
                    + (dstX - args.renderWindow.x1)
                )] += 1;
                for (auto c=0; c < componentCount; c++, srcPix++, dstPix++) {
                    *dstPix = *srcPix;
                }
            } else {
                for (auto i=0; i < 4; i++) {
                    auto cornX = x + ((i & 1) ^ (i >> 1));
                    auto cornY = y + (i >> 1);
                    auto transPix = (float*)transImg->getPixelAddressNearest(
                        cornX
                        ,cornY
                    );
                    quad.edges[i].p = {
                        cornX + transPix[0] * args.renderScale.x / srcPar
                        ,cornY + transPix[1] * args.renderScale.y
                    };
                    // if (x == 0 && y == 0) {
                    //     std::cout << quad.edges[]
                    // }
                }
                quad.bounds(&quadBounds);
                if (!Coords::rectIntersection(quadBounds, args.renderWindow, &quadIntersection)) {
                    continue;
                }
                quad.initialise();
                quad.fix(&locked, NULL);
                auto srcQuadPixPtr = srcQuadPix.get();
                for (auto sY=0; sY < 2; sY++) {
                    for (auto sX=0; sX < 2; sX++) {
                        auto srcPix = (float*)srcImg->getPixelAddressNearest(
                            x + sX
                            ,y + sY
                        );
                        for (auto c=0; c < srcComponentCount; c++, srcPix++, srcQuadPixPtr++) {
                            *srcQuadPixPtr = *srcPix;
                        }
                    }
                }
                for (auto rY=quadIntersection.y1; rY < quadIntersection.y2; rY++) {
                    ratioSumsPtr = (
                        ratioSums.get()
                        + (rY - args.renderWindow.y1) * windowWidth
                        + (quadIntersection.x1 - args.renderWindow.x1)
                    );
                    for (auto rX=quadIntersection.x1; rX < quadIntersection.x2; rX++, ratioSumsPtr++) {
                        if (abort()) {return;}
                        quad.setCurrentPixel({(double)rX, (double)rY});
                        auto intersection = quad.calculatePixelIntersection(&intersectionPoly);
                        if (intersection == 0) {continue;}
                        *ratioSumsPtr += intersection;
                        quad.calculatePixelIdentity(&identityP);
                        identityP = {
                            std::max(1.0,  std::min(0.0, identityP.x))
                            ,std::max(1.0,  std::min(0.0, identityP.y))
                        };
                        auto dstPix = (float*)dstImg->getPixelAddress(rX, rY);
                        auto srcQuadPixPtr = srcQuadPix.get();
                        double w;
                        for (auto sY=0; sY < 2; sY++) {
                            for (auto sX=0; sX < 2; sX++) {
                                for (auto c=0; c < srcComponentCount; c++, srcQuadPixPtr++) {
                                    if (c >= dstComponentCount) {continue;}
                                    if (sX == 0) {w = 1 - identityP.x;}
                                    else {w = identityP.x;}
                                    if (sY == 0) {w *= 1 - identityP.y;}
                                    else {w *= identityP.y;}
                                    dstPix[c] += *srcQuadPixPtr * w * intersection;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    ratioSumsPtr = ratioSums.get();
    for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++, ratioSumsPtr++) {
            if (abort()) {return;}
            if (*ratioSumsPtr == 0) {continue;}
            outPIX = (float*)dstImg->getPixelAddress(x, y);
            for (int c=0; c < componentCount; c++, outPIX++) {
                *outPIX /= *ratioSumsPtr;
            }
        }
    }

    // // break render window into regions and group quads with intersecting bounds
    // auto windowWidth = args.renderWindow.x2 - args.renderWindow.x1;
    // auto windowHeight = args.renderWindow.y2 - args.renderWindow.y1;
    // auto regionsWidth = (int)std::ceil(windowWidth / 128.0);
    // auto regionsHeight = (int)std::ceil(windowHeight / 128.0);
    // auto numRegions = regionsWidth * regionsHeight;
    // std::cout << (regionsWidth * regionsHeight) << std::endl;
    // auto_ptr<auto_ptr<quad_list_t>> regions(new auto_ptr<quad_list_t>[numRegions]);
    // auto regPtr = regions.get();
    // for (auto i=0; i < numRegions; i++, regPtr++) {std::cout << regPtr->get() << std::endl;}
    // Quadrangle quad;
    // OfxRectI quadBounds;
    // OfxRectI quadRegions;
    // for (auto y=transROD.y1; y < transROD.y2; y++) {
    //     for (auto x=transROD.x1; x < transROD.x2; x++) {
    //         if (abort()) {return;}
    //         quadRegions = {regionsWidth, regionsHeight, 0, 0};
    //         for (auto i=0; i < 4; i++) {
    //             auto cornX = x + ((i & 1) ^ (i >> 1));
    //             auto cornY = y + (i >> 1);
    //             auto transPix = (float*)transImg->getPixelAddressNearest(
    //                 cornX
    //                 ,cornY
    //             );
    //             quad.edges[0].p = {
    //                 cornX + transPix[0] * args.renderScale.x
    //                 ,cornY + transPix[1] * args.renderScale.y
    //             };
    //         }
    //         quad.bounds(&quadBounds);
    //         if (!Coords::rectIntersection(quadBounds, args.renderWindow, &quadRegions)) {
    //             continue;
    //         }
    //         // std::cout << quadRegions.x1 << "," << quadRegions.y1 << " " << quadRegions.x2 << "," << quadRegions.y2 << std::endl;
    //         quad.initialise();
    //         quadRegions.x1 >>= 7;
    //         quadRegions.y1 >>= 7;
    //         quadRegions.x2 = (quadRegions.x2 >> 7) + ((quadRegions.x2 % 128) ? 1 : 0);
    //         quadRegions.y2 = (quadRegions.y2 >> 7) + ((quadRegions.y2 % 128) ? 1 : 0);
    //         // std::cout << quadRegions.x1 << "," << quadRegions.y1 << " " << quadRegions.x2 << "," << quadRegions.y2 << std::endl;
    //         // std::cout.flush();
    //         // for (auto rY=quadRegions.y1; rY < quadRegions.y2; rY++) {
    //         //     auto regPtr = regions.get() + rY * regionsWidth + quadRegions.x1;
    //         //     for (auto rX=quadRegions.x1; rX < quadRegions.x2; rX++, regPtr++) {
    //         //         regPtr->push_back(quad);
    //         //     }
    //         // }
    //     }
    // }

    // // start entire output at 0
    // // initialise all the sums of ratios for each output pixel
    // auto windowWidth = args.renderWindow.x2 - args.renderWindow.x1;
    // auto windowHeight = args.renderWindow.y2 - args.renderWindow.y1;
    // auto_ptr<double> ratioSums(new double[windowHeight * windowWidth]);
    // auto ratioSumsPtr = ratioSums.get();
    // for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
    //     for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++, ratioSumsPtr++) {
    //         *ratioSumsPtr = 0;
    //         outPIX = (float*)dstImg->getPixelAddress(x, y);
    //         for (int c=0; c < dstComponentCount; c++, outPIX++) {
    //             *outPIX = 0;
    //         }
    //     }
    // }

    // OfxPointD p;
    // OfxPointD transVect;
    // OfxPointD prevTransVect;
    // OfxPointD cornerPoint;
    // bool allSame;
    // Quadrangle quad;
    // OfxRectI quadBounds;
    // OfxRectI intersectBounds;
    // OfxPointD transPoint;
    // float* transPIX;
    // OfxPointD srcPoint;
    // double intersection;
    // auto_ptr<float> values(new float[componentCount]);
    // for (p.y=srcROD.y1; p.y < srcROD.y2; p.y++) {
    //     for (p.x=srcROD.x1; p.x < srcROD.x2; p.x++) {
    //         if (abort()) {return;}
    //         // establish quadrangle points
    //         allSame = true;
    //         auto edgePtr = quad.edges;
    //         for (int i=0; i < 4; i++, edgePtr++) {
    //             cornerPoint.x = p.x + (i % 3); // 1 and 2
    //             cornerPoint.y = p.y + (i >> 1); // 2 and 3
    //             transPIX = (float*)transImg->getPixelAddressNearest(
    //                 cornerPoint.x, cornerPoint.y
    //             );
    //             transVect.x = transPIX[0] * args.renderScale.x / srcPar;
    //             transVect.y = transPIX[1] * args.renderScale.y;
    //             if (i > 0) {
    //                 // so far all same and same as last one
    //                 allSame = (
    //                     allSame
    //                     && transVect.x == prevTransVect.x
    //                     && transVect.y == prevTransVect.y
    //                 );
    //             }
    //             prevTransVect = transVect;
    //             vectorAdd(cornerPoint, transVect, &edgePtr->p);
    //         }

    //         // all the same? There's no distortion,
    //         // we just know whence to draw this pixel
    //         // Also if the quad is invalid, do the same.
    //         // I have good reason to believe initialise won't be
    //         // call if allSame is true
    //         if (allSame || !quad.initialise()) {
    //             auto srcPIX = (float*)srcImg->getPixelAddressNearest(p.x, p.y);
    //             addPixelValue(
    //                 quad.edges[0].p, srcPIX, componentCount, 1,
    //                 dstImg.get(), ratioSums.get(), args.renderWindow
    //             );
    //             continue;
    //         }

    //         // it's distorted, let's bblaaay
    //         // go through every pixel inside the smallest rect
    //         // containing this quadrangle, intersected with render window
    //         quad.bounds(&quadBounds);
    //         rectIntersect(&quadBounds, &args.renderWindow, &intersectBounds);
    //         for (transPoint.y=intersectBounds.y1; transPoint.y < intersectBounds.y2; transPoint.y++) {
    //             for (transPoint.x=intersectBounds.x1; transPoint.x < intersectBounds.x2; transPoint.x++) {
    //                 if (abort()) {return;}
    //                 QuadranglePixel quadPix(&quad, transPoint);
    //                 if (transPoint.x == 1422 && transPoint.y == 429) {
    //                     std::cout << quadPix.intersection << std::endl;
    //                 }
    //                 if (quadPix.intersection <= 0) {continue;}
    //                 quadPix.calculateIdentityPoint(&srcPoint);
    //                 if (transPoint.x == 1422 && transPoint.y == 429) {
    //                     std::cout << srcPoint.x << "," << srcPoint.y << std::endl;
    //                 }
    //                 if (
    //                     IsNaN(srcPoint.x)
    //                     || IsNaN(srcPoint.y)
    //                 ) {
    //                     srcPoint.x = 0;
    //                     srcPoint.y = 0;
    //                 }
    //                 if (srcPoint.x < 0) {srcPoint.x = 0;}
    //                 if (srcPoint.x >= 1) {srcPoint.x = 1 - QUADRANGLEDISTORT_DELTA;}
    //                 if (srcPoint.y < 0) {srcPoint.y = 0;}
    //                 if (srcPoint.y >= 1) {srcPoint.y = 1 - QUADRANGLEDISTORT_DELTA;}
    //                 srcPoint.x += p.x;
    //                 srcPoint.y += p.y;
    //                 bilinear(
    //                     srcPoint.x, srcPoint.y, srcImg.get(), values.get(), componentCount
    //                 );
    //                 addPixelValue(
    //                     transPoint, values.get(), componentCount, quadPix.intersection,
    //                     dstImg.get(), ratioSums.get(), args.renderWindow
    //                 );
    //             }
    //         }
    //     }
    // }

    // ratioSumsPtr = ratioSums.get();
    // for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
    //     for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++, ratioSumsPtr++) {
    //         if (abort()) {return;}
    //         if (*ratioSumsPtr == 0) {continue;}
    //         outPIX = (float*)dstImg->getPixelAddress(x, y);
    //         for (int c=0; c < componentCount; c++, outPIX++) {
    //             *outPIX /= *ratioSumsPtr;
    //         }
    //     }
    // }
}
