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


typedef struct {
    double time;
    OfxPointD renderScale;
    int components;
    OfxRectI rod;
    float* imgData;
} _cached_output_t;


class OffsetMapPlugin : public ImageEffect
{
public:
    OffsetMapPlugin(OfxImageEffectHandle handle);
    ~OffsetMapPlugin() {
        for (int i=0; i < MAX_CACHE_OUTPUTS; i++) {
            if (_cached_outputs[i].imgData) {
                delete[] _cached_outputs[i].imgData;
            }
        }
    }

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

    virtual void changedClip(const InstanceChangedArgs &args, const std::string &clipName) OVERRIDE FINAL;

    virtual void endChanged(InstanceChangeReason reason) OVERRIDE FINAL;

    _cached_output_t* getCachedOutput(double t, OfxPointD renderScale);

private:
    Clip* _srcClip;
    Clip* _offClip;
    Clip* _dstClip;
    BooleanParam* _iterateTemporally;
    IntParam* _referenceFrame;

    _cached_output_t _cached_outputs[MAX_CACHE_OUTPUTS];
    int _next_cached_output_index;
    std::map<double, _cached_output_t*> _cached_output_by_time;
};
