#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include <iostream>

using namespace OFX;

#define kPluginName "OffsetMap"
#define kPluginGrouping "Transform"
#define kPluginDescription \
"OffsetMap"

#define kPluginIdentifier "com.ajptechnical.openfx.OffsetMap"
#define kPluginVersionMajor 0
#define kPluginVersionMinor 1

#define kSourceClip "Source"
#define kOffsetsClip "Offsets"

#define kParamIterateTemporally "iterateTemporally"
#define kParamIterateTemporallyLabel "Iterate Temporally"
#define kParamIterateTemporallyHint "Iterate Temporally"

#define kParamReferenceFrame "referenceFrame"
#define kParamReferenceFrameLabel "Reference Frame"
#define kParamReferenceFrameHint "Reference Frame"

#define MAX_CACHE_OUTPUTS 10


class CachedOutput {
public:
    bool inCache = false;
    double time;
    OfxRectI rod;
    int components;
    auto_ptr<ImageMemory> imgMem;
    void refresh(double t, OfxRectI newROD, int comps) {
        time = t;
        components = comps;
        rod = newROD;
        auto width = newROD.x2 - newROD.x1;
        auto height = newROD.y2 - newROD.y1;
        imgMem.reset(
            new ImageMemory(width * height * comps * sizeof(float))
        );
    }
};


class OffsetMapPlugin : public ImageEffect
{
public:
    OffsetMapPlugin(OfxImageEffectHandle handle);

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    
    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
    ) OVERRIDE FINAL;

    virtual void getClipPreferences(ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;

    virtual void getFramesNeeded(const FramesNeededArguments &args, FramesNeededSetter &frames) OVERRIDE FINAL;

    CachedOutput* getOutput(double t, OfxPointD renderScale);

private:
    Clip* _srcClip;
    Clip* _offClip;
    Clip* _dstClip;
    BooleanParam* _iterateTemporally;
    IntParam* _referenceFrame;

    bool _haveLastRenderArgs = false;
    RenderArguments _lastRenderArgs;
    int _nextCacheIndex = 0;
    CachedOutput _cachedOutputs[MAX_CACHE_OUTPUTS];
    std::map<double, CachedOutput*> _cachedOutputByTime;
};
