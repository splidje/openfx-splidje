#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include <iostream>

using namespace OFX;

#define kPluginName "TranslateMap"
#define kPluginGrouping "Transform"
#define kPluginDescription \
"TranslateMap"

#define kPluginIdentifier "com.ajptechnical.openfx.TranslateMap"
#define kPluginVersionMajor 0
#define kPluginVersionMinor 1

#define kSourceClip "Source"
#define kTranslationsClip "Translations"

#define kParamApproximate "approximate"

#define MAX_CACHE_OUTPUTS 10


class TranslateMapPlugin : public ImageEffect
{
public:
    TranslateMapPlugin(OfxImageEffectHandle handle);

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

    virtual void getRegionsOfInterest(const RegionsOfInterestArguments &args, RegionOfInterestSetter &rois);

private:
    Clip* _srcClip;
    Clip* _transClip;
    Clip* _dstClip;
    BooleanParam* _approximate;
};
