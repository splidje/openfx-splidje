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

void OffsetMapPlugin::getClipPreferences(ClipPreferencesSetter &clipPreferences) {
    clipPreferences.setClipComponents(*_dstClip, _srcClip->getPixelComponents());
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
    auto offImgBounds = offImg->getRegionOfDefinition();
    if (rectIsEmpty(offImgBounds)) {
        return;
    }
    auto offComps = offImg->getPixelComponentCount();
    auto offPar = _offClip->getPixelAspectRatio();
    OfxRectI renderWindow;
    toPixelEnclosing(offROI, args.renderScale, offPar, &renderWindow);
    OfxRectD srcROD = _srcClip->getRegionOfDefinition(args.time);
    OfxRectD srcROI;
    srcROI.x1 = srcROD.x2;
    srcROI.x2 = srcROD.x1;
    srcROI.y1 = srcROD.y2;
    srcROI.y2 = srcROD.y1;
    OfxPointI curP;
    for (curP.y=renderWindow.y1; curP.y < renderWindow.y2; curP.y++) {
        for (curP.x=renderWindow.x1; curP.x < renderWindow.x2; curP.x++) {
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

inline void readQuad(Image* img, int x, int y, int comps, OfxPointD renderScale, double par, Quadrangle* quad) {
    for (auto r=0; r < 2; r++) {
        for (auto c=0; c < 2; c++) {
            auto pix = (float*)img->getPixelAddressNearest(x + c, y + r);
            OfxPointD offset;
            offset.x = pix[0] * renderScale.x / par;
            if (comps > 1) {
                offset.y = pix[1] * renderScale.y;
            } else {
                offset.y = 0;
            }
            auto idx = r * 2 + (r ? (1-c) : c);
            quad->edges[idx].p.x = x + offset.x;
            quad->edges[idx].p.y = y + offset.y;
        }
    }
    quad->initialise();
}

// the overridden render function
void OffsetMapPlugin::render(const RenderArguments &args)
{
    auto_ptr<Image> srcImg(_srcClip->fetchImage(args.time));
    auto srcComps = srcImg->getPixelComponentCount();
    auto srcImgBounds = srcImg->getRegionOfDefinition();
    auto_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    auto dstComps = dstImg->getPixelComponentCount();
    // OfxRectD srcRegion = _srcClip->getRegionOfDefinition(args.time);
    // OfxRectD offRegion;
    // toCanonical(args.renderWindow, args.renderScale, _offClip->getPixelAspectRatio(), &offRegion);
    // offRegion.x2 += 1;
    // offRegion.y2 += 1;
    auto_ptr<Image> offImg(_offClip->fetchImage(args.time));
    auto offComps = offImg->getPixelComponentCount();
    auto offPar = offImg->getPixelAspectRatio();
    for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            auto dstPix = (float*)dstImg->getPixelAddressNearest(x, y);
            Quadrangle quad;
            readQuad(offImg.get(), x, y, offComps, args.renderScale, offPar, &quad);
            OfxRectI bounds;
            quad.bounds(&bounds);
            if (quad.zeroEdgeCount <= 1)
            {
                for (auto srcY=bounds.y1; srcY < bounds.y2; srcY++) {
                    for (auto srcX=bounds.x1; srcX < bounds.x2; srcX++) {
                        // quad.
                    }
                }
            }
            else {
                auto srcPix = (float*)srcImg->getPixelAddressNearest(round(quad.edges[0].p.x), round(quad.edges[0].p.y));
                for (auto c=0; c < dstComps; c++) {
                    if (c < srcComps) {
                        dstPix[c] = srcPix[0];
                    } else {
                        dstPix[c] = 0;
                    }
                }
            }
        }
    }
}
