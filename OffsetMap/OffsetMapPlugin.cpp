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
    if (_haveLastRenderArgs) {
        if (
            args.renderScale.x != _lastRenderArgs.renderScale.x
            || args.renderScale.y != _lastRenderArgs.renderScale.y
            || (
                args.time == _lastRenderArgs.time
                && args.renderWindow.x1 == _lastRenderArgs.renderWindow.x1
                && args.renderWindow.y1 == _lastRenderArgs.renderWindow.y1
                && args.renderWindow.x2 == _lastRenderArgs.renderWindow.x2
                && args.renderWindow.y2 == _lastRenderArgs.renderWindow.y2
            )
        ) {
            std::cout << "clearing cache" << std::endl;
            for (auto i : _cachedOutputByTime) {
                i.second->inCache = false;
                i.second->imgMem.reset();
            }
            _cachedOutputByTime.clear();
        }
    }
    _haveLastRenderArgs = true;
    _lastRenderArgs = args;
    std::cout << "calling getOutput for " << args.time << std::endl;
    auto output = getOutput(args.time, args.renderScale);
    if (!output) {return;}
    auto width = output->rod.x2 - output->rod.x1;
    auto_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    auto dstComponents = dstImg->getPixelComponentCount();
    auto imgData = (float*)output->imgMem->lock();
    for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {        
        auto dstPix = (float*)dstImg->getPixelAddress(args.renderWindow.x1, y);
        for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            auto cachedPix = imgData + (y * width + x) * output->components;
            for (int c=0; c < dstComponents; c++, dstPix++) {
                if (c < output->components) {
                    *dstPix = cachedPix[c];
                } else {
                    *dstPix = 0;
                }
            }
        }
    }
    output->imgMem->unlock();
}

CachedOutput* OffsetMapPlugin::getOutput(double t, OfxPointD renderScale)
{
    for (auto i : _cachedOutputByTime) {
        std::cout << i.first << " ";
    }
    std::cout << std::endl;
    auto iter = _cachedOutputByTime.find(t);
    if (iter != _cachedOutputByTime.end()) {
        std::cout << "using cached " << t << std::endl;
        return iter->second;
    }

    std::cout << "caching " << t << std::endl;
    auto_ptr<Image> offImg(_offClip->fetchImage(t, _offClip->getRegionOfDefinition(t)));
    if (!offImg.get()) {return NULL;}
    auto offROD = offImg->getRegionOfDefinition();
    auto offComponents = offImg->getPixelComponentCount();
    float* srcImgData;
    OfxRectI srcROD;
    int srcComponents;
    CachedOutput* srcImgCached = NULL;
    auto_ptr<Image> srcImgCleaner;
    auto iterTemp = _iterateTemporally->getValueAtTime(t);
    auto refFrame = _referenceFrame->getValueAtTime(t);        
    if (iterTemp && refFrame != t) {
        auto srcFrame = t + (refFrame < t ? -1 : 1);
        srcImgCached = getOutput(srcFrame, renderScale);
        if (!srcImgCached) {
            return NULL;
        }
        srcComponents = srcImgCached->components;
        srcImgData = (float*)srcImgCached->imgMem->lock();
        srcROD = srcImgCached->rod;
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
    auto ret = _cachedOutputs + _nextCacheIndex;
    if (ret->inCache) {
        _cachedOutputByTime.erase(ret->time);
    }
    ret->refresh(t, offROD, srcComponents);
    auto imgData = (float*)ret->imgMem->lock();
    auto dstPix = imgData;
    for (int y=offROD.y1; y < offROD.y2; y++) {
        if (abort()) {
            std::cout << "abort" << std::endl;
            break;
        }
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
    ret->imgMem->unlock();
    if (srcImgCached) {
        srcImgCached->imgMem->unlock();
    }
    if (abort()) {
        return NULL;
    }
    _cachedOutputByTime[t] = ret;
    ret->inCache = true;
    _nextCacheIndex++;
    if (_nextCacheIndex >= MAX_CACHE_OUTPUTS) {
        _nextCacheIndex = 0;
    }
    return ret;
}
