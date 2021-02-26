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

#define MAX_CACHE_OUTPUTS 2


class OffsetMapImage {
public:
    OfxRectI rod;
    int components;
    float* imgData;
    OffsetMapImage(OfxRectI newROD, int comps) {
        components = comps;
        rod = newROD;
        auto width = newROD.x2 - newROD.x1;
        auto height = newROD.y2 - newROD.y1;
        imgData = new float[width * height * comps];
    }
    ~OffsetMapImage() {
        if (imgData) {
            delete[] imgData;
        }
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

    OffsetMapImage* getOutput(double t, OfxPointD renderScale);

private:
    Clip* _srcClip;
    Clip* _offClip;
    Clip* _dstClip;
    BooleanParam* _iterateTemporally;
    IntParam* _referenceFrame;
};
