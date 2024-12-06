#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include <iostream>
#include <unsupported/Eigen/Polynomials>

using namespace OFX;

#define kPluginName "PlotAlgebraic"
#define kPluginGrouping "Color"
#define kPluginDescription \
"PlotAlgebraic"

#define kPluginIdentifier "com.ajptechnical.splidje.openfx.PlotAlgebraic"
#define kPluginVersionMajor 0
#define kPluginVersionMinor 1

#define kSourceClip "Source"

#define kParamRandomSeed "seed"
#define kParamRandomSeedLabel "Random Seed"

#define kParamMaxCoeff "maxCoeff"
#define kParamMaxCoeffLabel "Max Coefficient"

#define kParamMaxRoot "maxRoot"
#define kParamMaxRootLabel "Max Complex Root"

#define kParamNumIters "numIters"
#define kParamNumItersLabel "Number of Iterations"

#define NUM_COEFFS 5

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
    IntParam* _randomSeed;
    IntParam* _maxCoeff;
    Double2DParam* _maxRoot;
    IntParam* _numIters;
    Eigen::Matrix<double, NUM_COEFFS, 1> _coeffs;
    Eigen::PolynomialSolver<double, NUM_COEFFS - 1> _solver;
};
