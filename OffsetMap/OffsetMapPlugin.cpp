#include "OffsetMapPlugin.h"
#include "ofxsCoords.h"
#include "../QuadrangleDistort/QuadrangleDistort.h"
#include <iostream>

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
    _blackOutside = fetchBooleanParam(kParamBlackOutside);
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
    srcROI.y1 = srcROD.y2;
    srcROI.x2 = srcROD.x1;
    srcROI.y2 = srcROD.y1;
    OfxPointI curP;
    for (curP.y=renderWindow.y1; curP.y < renderWindow.y2; curP.y++) {
        for (curP.x=renderWindow.x1; curP.x < renderWindow.x2; curP.x++) {
            if (abort()) {return;}
            OfxPointD srcP;
            toCanonical(curP, args.renderScale, offPar, &srcP);
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
    auto blackOutside = _blackOutside->getValueAtTime(args.time);
    OfxPointI posPix;
    for (posPix.y=args.renderWindow.y1; posPix.y < args.renderWindow.y2; posPix.y++) {
        for (posPix.x=args.renderWindow.x1; posPix.x < args.renderWindow.x2; posPix.x++) {
            if (abort()) {
                return;
            }
            auto dstPix = (float*)dstImg->getPixelAddress(posPix.x, posPix.y);
            if (!dstPix) {
                return;
            }
            auto offPix = (float*)offImg->getPixelAddressNearest(posPix.x, posPix.y);
            double srcXPix = (offPar * posPix.x + args.renderScale.x * offPix[0]) / srcPar;
            double srcYPix = posPix.y;
            if (offComps > 1) {
                srcYPix += args.renderScale.y * offPix[1];
            }
            bilinear(
                srcXPix,
                srcYPix,
                srcImg.get(),
                dstPix,
                srcComps,
                blackOutside
            );
        }
    }
}
