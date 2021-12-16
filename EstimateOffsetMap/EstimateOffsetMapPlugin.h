#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include "EstimateOffsetMapPluginMacros.h"

using namespace OFX;


class EstimateOffsetMapPlugin : public ImageEffect
{
public:
    EstimateOffsetMapPlugin(OfxImageEffectHandle handle);

private:
    virtual bool getRegionOfDefinition(const RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;

    virtual void getClipPreferences(ClipPreferencesSetter &clipPreferences) OVERRIDE FINAL;

    virtual void getRegionsOfInterest(const RegionsOfInterestArguments &args, RegionOfInterestSetter &rois) OVERRIDE FINAL;

    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

private:
    Clip* _srcClip;
    Clip* _trgClip;
    Clip* _dstClip;
    IntParam* _iterations;
    IntParam* _seed;
};
