#include "OffsetMapPlugin.h"
#include "ofxsCoords.h"


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
    _iterateTemporally = fetchBooleanParam(kParamIterateTemporally);
    _referenceFrame = fetchIntParam(kParamReferenceFrame);
}

void OffsetMapPlugin::getClipPreferences(ClipPreferencesSetter &clipPreferences) {
    clipPreferences.setClipComponents(*_dstClip, _srcClip->getPixelComponents());
}

void OffsetMapPlugin::getFramesNeeded(const FramesNeededArguments &args, FramesNeededSetter &frames) {
    std::cout << "getFramesNeeded " << args.time << std::endl;
    auto iterTemp = _iterateTemporally->getValueAtTime(args.time);
    auto refFrame = _referenceFrame->getValueAtTime(args.time);
    if (!iterTemp || refFrame == args.time) {
        ImageEffect::getFramesNeeded(args, frames);
        return;
    }
    OfxRangeD range;
    range.min = std::min(double(refFrame), args.time);
    range.max = std::max(double(refFrame), args.time);
    frames.setFramesNeeded(*_srcClip, range);
    frames.setFramesNeeded(*_offClip, range);
}

bool OffsetMapPlugin::isIdentity(const IsIdentityArguments &args, 
                                  Clip * &identityClip, double &/*identityTime*/
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
)
{
    return false;
}

// the overridden render function
void OffsetMapPlugin::render(const RenderArguments &args)
{
    auto_ptr<OffsetMapImage> output(getOutput(args.time, args.renderScale));
    if (!output.get()) {return;}
    auto width = output->rod.x2 - output->rod.x1;
    auto_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    auto dstComponents = dstImg->getPixelComponentCount();
    for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {        
        auto dstPix = (float*)dstImg->getPixelAddress(args.renderWindow.x1, y);
        for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            auto cachedPix = output->imgData + (y * width + x) * output->components;
            for (int c=0; c < dstComponents; c++, dstPix++) {
                if (c < output->components) {
                    *dstPix = cachedPix[c];
                } else {
                    *dstPix = 0;
                }
            }
        }
    }
}

OffsetMapImage* OffsetMapPlugin::getOutput(double t, OfxPointD renderScale) {
    auto_ptr<Image> offImg(_offClip->fetchImage(t, _offClip->getRegionOfDefinition(t)));
    if (!offImg.get()) {return NULL;}
    auto offROD = offImg->getRegionOfDefinition();
    auto offComponents = offImg->getPixelComponentCount();
    float* srcImgData;
    OfxRectI srcROD;
    int srcComponents;
    auto_ptr<Image> srcImgCleaner;
    auto_ptr<OffsetMapImage> outImgCleaner;
    auto iterTemp = _iterateTemporally->getValueAtTime(t);
    auto refFrame = _referenceFrame->getValueAtTime(t);        
    if (iterTemp && refFrame != t) {
        auto srcFrame = t + (refFrame < t ? -1 : 1);
        outImgCleaner.reset(getOutput(srcFrame, renderScale));
        srcComponents = outImgCleaner->components;
        srcImgData = outImgCleaner->imgData;
        srcROD = outImgCleaner->rod;
    } else {
        srcImgCleaner.reset(
            _srcClip->fetchImage(
                t, _srcClip->getRegionOfDefinition(t)
            )
        );
        srcComponents = srcImgCleaner->getPixelComponentCount();
        srcImgData = (float*)srcImgCleaner->getPixelData();
        srcROD = srcImgCleaner->getRegionOfDefinition();
    }
    auto srcWidth = srcROD.x2 - srcROD.x1;
    auto ret = new OffsetMapImage(offROD, srcComponents);
    auto dstPix = ret->imgData;
    for (int y=offROD.y1; y < offROD.y2; y++) {
        auto offPix = (float*)offImg->getPixelAddress(offROD.x1, y);
        for (int x=offROD.x1; x < offROD.x2; x++) {
            int xSrc = round(x + offPix[0] * renderScale.x);
            int ySrc = round(y + offPix[1] * renderScale.y);
            float* srcPix = NULL;
            if (xSrc >= srcROD.x1 && xSrc < srcROD.x2
                && ySrc >= srcROD.y1 && ySrc < srcROD.y2)
            {
                srcPix = srcImgData + (ySrc * srcWidth + xSrc) * srcComponents;
            }
            for (int c=0; c < srcComponents; c++, dstPix++) {
                if (srcPix) {
                    *dstPix = srcPix[c];
                } else {
                    *dstPix = 0;
                }
            }
            offPix += offComponents;
        }
    }
    return ret;
}
