#include "OffsetMapPlugin.h"
#include "ofxsCoords.h"


OffsetMapPlugin::OffsetMapPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
    , _next_cached_output_index(0)
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

void OffsetMapPlugin::changedClip(const InstanceChangedArgs &args, const std::string &clipName) {
    std::cout << "changed clip!" << std::endl;
}

void OffsetMapPlugin::endChanged(InstanceChangeReason reason) {
    std::cout << "changed!" << std::endl;
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
    auto cachedOutput = getCachedOutput(args.time, args.renderScale);
    if (!cachedOutput) {return;}
    auto cachedWidth = cachedOutput->rod.x2 - cachedOutput->rod.x1;
    auto_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    auto dstComponents = dstImg->getPixelComponentCount();
    for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        auto dstPix = (float*)dstImg->getPixelAddress(args.renderWindow.x1, y);
        for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            int cachedX = round(cachedOutput->renderScale.x * x / args.renderScale.x);
            int cachedY = round(cachedOutput->renderScale.y * y / args.renderScale.y);
            auto cachedPix = cachedOutput->imgData + (cachedY * cachedWidth + cachedX) * cachedOutput->components;
            for (int c=0; c < dstComponents; c++, dstPix++) {
                if (c < cachedOutput->components) {
                    *dstPix = cachedPix[c];
                } else {
                    *dstPix = 0;
                }
            }
        }
    }
}

_cached_output_t* OffsetMapPlugin::getCachedOutput(double t, OfxPointD renderScale) {
    _cached_output_t* ret;
    auto iter = _cached_output_by_time.find(t);
    if (iter != _cached_output_by_time.end()
        && iter->second->renderScale.x >= renderScale.x
        && iter->second->renderScale.y >= renderScale.y)
    {
        std::cout << t << " " << renderScale.x << "," << renderScale.y << " already cached. returning." << std::endl;
        ret = iter->second;
    } else {
        std::cout << t << " " << renderScale.x << "," << renderScale.y << " not cached. generating and caching." << std::endl;
        auto blah = _offClip->getRegionOfDefinition(t);
        auto_ptr<Image> offImg(_offClip->fetchImage(t, _offClip->getRegionOfDefinition(t)));
        if (!offImg.get()) {return NULL;}
        auto offROD = offImg->getRegionOfDefinition();
        auto offWidth = offROD.x2 - offROD.x1;
        auto offHeight = offROD.y2 - offROD.y1;
        auto offComponents = offImg->getPixelComponentCount();
        float* srcImgData;
        OfxRectI srcROD;
        int srcComponents;
        OfxPointD srcRenderScale = renderScale;
        auto_ptr<Image> srcImgCleaner;
        auto iterTemp = _iterateTemporally->getValueAtTime(t);
        auto refFrame = _referenceFrame->getValueAtTime(t);        
        if (iterTemp && refFrame != t) {
            auto srcFrame = refFrame - t;
            auto cached_output = getCachedOutput(srcFrame, renderScale);
            srcComponents = cached_output->components;
            srcImgData = cached_output->imgData;
            srcROD = cached_output->rod;
            srcRenderScale = cached_output->renderScale;
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
        ret = _cached_outputs + _next_cached_output_index;
        ret->time = t;
        ret->renderScale = renderScale;
        ret->components = srcComponents;
        ret->rod = offROD;
        if (ret->imgData) {delete[] ret->imgData;}
        ret->imgData = new float[offWidth * offHeight * srcComponents];
        auto dstPix = ret->imgData;
        for (int y=offROD.y1; y < offROD.y2; y++) {
            auto offPix = (float*)offImg->getPixelAddress(offROD.x1, y);
            for (int x=offROD.x1; x < offROD.x2; x++) {
                int xSrc = round(((x / renderScale.x) + offPix[0]) * srcRenderScale.x);
                int ySrc = round(((y / renderScale.y) + offPix[1]) * srcRenderScale.y);
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
        _cached_output_by_time[t] = ret;
        _next_cached_output_index++;
        if (_next_cached_output_index == MAX_CACHE_OUTPUTS) {
            _next_cached_output_index = 0;
        }
    }
    return ret;
}
