#include "OffsetMapPlugin.h"
#include "ofxsCoords.h"
#include "../QuadrangleDistort/QuadrangleDistort.h"
#include <iostream>
#include <thread>

using namespace Coords;
using namespace QuadrangleDistort;

OffsetMapPlugin::OffsetMapPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
{
    _offClip = fetchClip(kOffsetsClip);
    assert(_offClip && (_offClip->getPixelComponents() == ePixelComponentRGB ||
    	    _offClip->getPixelComponents() == ePixelComponentRGBA));
    _srcClip = fetchClip(kSourceClip);
    assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentRGB ||
    	    _srcClip->getPixelComponents() == ePixelComponentRGBA));
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentRGB ||
    	    _dstClip->getPixelComponents() == ePixelComponentRGBA));
}

bool OffsetMapPlugin::getRegionOfDefinition(const RegionOfDefinitionArguments &args, OfxRectD &rod) {
    rod = _offClip->getRegionOfDefinition(args.time);
    return true;
}

void OffsetMapPlugin::getClipPreferences(ClipPreferencesSetter &clipPreferences) {
#ifdef OFX_EXTENSIONS_NATRON
    OfxRectI format;
    _offClip->getFormat(format);
    clipPreferences.setOutputFormat(format);
#endif
    clipPreferences.setPixelAspectRatio(*_dstClip, _offClip->getPixelAspectRatio());
    clipPreferences.setClipComponents(*_dstClip, _srcClip->getPixelComponents());
}

bool OffsetMapPlugin::isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
) {
    if (!_offClip->isConnected()) {
        identityClip = _srcClip;
        identityTime = args.time;
        return true;
    }
    return false;
}

inline void toCanonicalFixed(
    const OfxPointI & p_pixel,
    const OfxPointD & renderScale,
    double par,
    OfxPointD *p_canonical
) {
    assert(par);
    p_canonical->x = p_pixel.x * par / renderScale.x;
    p_canonical->y = p_pixel.y / renderScale.y;
}

void OffsetMapPlugin::getRegionsOfInterest(const RegionsOfInterestArguments &args, RegionOfInterestSetter &rois) {
    auto offROI = args.regionOfInterest;
    offROI.x2 += 1;
    offROI.y2 += 1;
    rois.setRegionOfInterest(*_offClip, offROI);
    auto_ptr<Image> offImg(_offClip->fetchImage(args.time, offROI));
    if (!offImg.get()) {
        return;
    }
    if (abort()) {return;}
    auto offImgBounds = offImg->getRegionOfDefinition();
    if (rectIsEmpty(offImgBounds)) {
        return;
    }
    auto offComps = offImg->getPixelComponentCount();
    auto offPar = _offClip->getPixelAspectRatio();
    OfxRectI renderWindow;
    toPixelEnclosing(offROI, args.renderScale, offPar, &renderWindow);
    auto srcROD = _srcClip->getRegionOfDefinition(args.time);
    OfxRectD srcROI;
    srcROI.x1 = srcROD.x2;
    srcROI.x2 = srcROD.x1;
    srcROI.y1 = srcROD.y2;
    srcROI.y2 = srcROD.y1;
    OfxPointI curP;
    for (curP.y=renderWindow.y1; curP.y < renderWindow.y2; curP.y++) {
        for (curP.x=renderWindow.x1; curP.x < renderWindow.x2; curP.x++) {
            if (abort()) {return;}
            OfxPointD srcP;
            toCanonicalFixed(curP, args.renderScale, offPar, &srcP);
            auto pix = (float*)offImg->getPixelAddressNearest(curP.x, curP.y);
            srcP.x += pix[0];
            if (offComps > 1) {
                srcP.y += pix[1];
            }
            srcROI.x1 = std::min(srcROI.x1, srcP.x);
            srcROI.x2 = std::max(srcROI.x2, srcP.x);
            srcROI.y1 = std::min(srcROI.y1, srcP.y);
            srcROI.y2 = std::max(srcROI.y2, srcP.y);
        }
    }
    rois.setRegionOfInterest(*_srcClip, srcROI);
}

inline void readQuad(Image* img, int x, int y, int comps, OfxPointD renderScale, double offPar, double srcPar, Quadrangle* quad) {
    for (auto r=0; r < 2; r++) {
        for (auto c=0; c < 2; c++) {
            auto pix = (float*)img->getPixelAddressNearest(x + c, y + r);
            OfxPointD offset;
            offset.x = pix[0] * renderScale.x / srcPar;
            if (comps > 1) {
                offset.y = pix[1] * renderScale.y;
            } else {
                offset.y = 0;
            }
            auto idx = r * 2 + (r ? (1-c) : c);
            quad->edges[idx].p.x = x * offPar / srcPar + offset.x;
            quad->edges[idx].p.y = y + offset.y;
        }
    }
    quad->initialise();
}

// the overridden render function
void OffsetMapPlugin::render(const RenderArguments &args)
{
    auto_ptr<Image> srcImg(_srcClip->fetchImage(args.time));
    if (!srcImg.get()) {
        return;
    }
    if (abort()) {return;}
    auto srcComps = srcImg->getPixelComponentCount();
    auto srcPar = srcImg->getPixelAspectRatio();
    auto srcImgBounds = srcImg->getRegionOfDefinition();
    auto srcWidth = srcImgBounds.x2 - srcImgBounds.x1;
    auto srcHeight = srcImgBounds.y2 - srcImgBounds.y1;
    auto_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    assert(dstImg.get());
    if (abort()) {return;}
    auto dstComps = dstImg->getPixelComponentCount();
    auto_ptr<Image> offImg(_offClip->fetchImage(args.time));
    if (!offImg.get()) {
        return;
    }
    if (abort()) {return;}
    auto offComps = offImg->getPixelComponentCount();
    auto offPar = offImg->getPixelAspectRatio();
    auto_ptr<double> sumValues(new double[srcComps]);
    double sumWeights;
    auto_ptr<float> srcPix(new float[srcComps]);
    for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            auto dstPix = (float*)dstImg->getPixelAddressNearest(x, y);
            Quadrangle quad;
            readQuad(offImg.get(), x, y, offComps, args.renderScale, offPar, srcPar, &quad);
            if (abort()) {return;}
            sumWeights = 0;
            if (quad.isValid())
            {
                OfxRectI bounds;
                quad.bounds(&bounds);
                for (auto c=0; c < srcComps; c++) {
                    sumValues.get()[c] = 0;
                }
                OfxPointD srcP;
                for (srcP.y=bounds.y1; srcP.y <= bounds.y2; srcP.y++) {
                    for (srcP.x=bounds.x1; srcP.x <= bounds.x2; srcP.x++) {
                        if (abort()) {return;}
                        quad.setCurrentPixel(srcP);
                        auto intersection = quad.calculatePixelIntersection(NULL);
                        if (!intersection) {
                            continue;
                        }
                        sumWeights += intersection;
                        OfxPointD idP;
                        quad.calculatePixelIdentity(&idP);
                        bilinear(
                            srcImgBounds.x1 + srcWidth * idP.x,
                            srcImgBounds.y1 + srcHeight * idP.y,
                            srcImg.get(),
                            srcPix.get(),
                            srcComps
                        );
                        for (auto c=0; c < srcComps; c++) {
                            sumValues.get()[c] += intersection * srcPix.get()[c];
                        }
                    }
                }
            }
            if (sumWeights > QUADRANGLEDISTORT_DELTA) {
                for (auto c=0; c < srcComps; c++) {
                    dstPix[c] = sumValues.get()[c] / sumWeights;
                }
            } else {
                bilinear(
                    quad.edges[0].p.x,
                    quad.edges[0].p.y,
                    srcImg.get(),
                    srcPix.get(),
                    srcComps
                );
                for (auto c=0; c < srcComps; c++) {
                    dstPix[c] = srcPix.get()[c];
                }
            }
        }
    }
}
