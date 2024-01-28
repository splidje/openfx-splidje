#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include <iostream>

using namespace OFX;

#define kPluginName "PlotAlgebraic"
#define kPluginGrouping "Color"
#define kPluginDescription \
"PlotAlgebraic"

#define kPluginIdentifier "com.ajptechnical.splidje.openfx.PlotAlgebraic"
#define kPluginVersionMajor 0
#define kPluginVersionMinor 1

#define kSourceClip "Source"


class PlotAlgebraicPlugin : public ImageEffect
{
public:
    PlotAlgebraicPlugin(OfxImageEffectHandle handle);

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

private:
    Clip* _srcClip;
    Clip* _dstClip;
};
