#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include "EstimateOffsetMapPluginMacros.h"
#include <mutex>

using namespace OFX;


typedef struct {
    OfxPointD from;
    OfxPointD bladeTip;
    OfxPointD rHiltTip;
    OfxPointD lHiltTip;
    double radius;
} sword_t;


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
    BooleanParam* _blackOutside;
    DoubleParam* _minRotate;
    DoubleParam* _maxRotate;
    DoubleParam* _minScale;
    DoubleParam* _maxScale;
    DoubleParam* _maxTranslate;
    IntParam* _iterations;
    IntParam* _seed;
};
