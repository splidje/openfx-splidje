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


class OffsetMapPlugin : public ImageEffect
{
public:
    OffsetMapPlugin(OfxImageEffectHandle handle);

private:
    virtual bool getRegionOfDefinition(const RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;

    virtual void getClipPreferences(ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;

    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
    ) OVERRIDE FINAL;

    virtual void getRegionsOfInterest(const RegionsOfInterestArguments &args, RegionOfInterestSetter &rois) OVERRIDE FINAL;

    /* Override the render */
    virtual void render(const RenderArguments &args) OVERRIDE FINAL;

private:
    Clip* _srcClip;
    Clip* _offClip;
    Clip* _dstClip;
};
