#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include <iostream>
#include <complex>

using namespace OFX;

#define kPluginName "PlotAlgebraic"
#define kPluginGrouping "Color"
#define kPluginDescription "PlotAlgebraic"

#define kPluginIdentifier "com.ajptechnical.splidje.openfx.PlotAlgebraic"
#define kPluginVersionMajor 0
#define kPluginVersionMinor 1

#define kParamMaxCoefficient "maxLeadingCoefficient"
#define kParamMaxCoefficientLabel "Max Leading Coefficient"

#define kParamIterationCount "iterationCount"
#define kParamIteractionCountLabel "Number of Iterations"

#define kParamRandomSeed "randomSeed"
#define kParamRandomSeedLabel "Random Seed"

#define kParamGenerate "generate"
#define kParamGenerateLabel "Generate"

#define COEFFICIENT_COUNT 5
#define COEFFICIENT_FACTORS ((int[]){1, 4, 6, 4, 1})

class PlotAlgebraicPlugin : public ImageEffect
{
public:
    PlotAlgebraicPlugin(OfxImageEffectHandle handle);

private:
    virtual void changedParam(const InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

private:
    Clip* _destinationClip;
    IntParam* _maxLeadingCoefficient;
    IntParam* _iterationCount;
    IntParam* _randomSeed;
    PushButtonParam* _generate;
    std::vector<std::complex<double>> _roots;
};
