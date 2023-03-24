#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include <iostream>

using namespace OFX;

#define kPluginName "EstimateGrade"
#define kPluginGrouping "Color"
#define kPluginDescription \
"EstimateGrade"

#define kPluginIdentifier "com.ajptechnical.splidje.openfx.EstimateGrade"
#define kPluginVersionMajor 0
#define kPluginVersionMinor 1

#define kSourceClip "Source"
#define kTargetClip "Target"

#define kParamCurve "curve"
#define kParamCurveLabel "Curve"
#define kParamCurveHint "Curve"

#define kParamSamples "samples"
#define kParamSamplesLabel "Samples"
#define kParamSamplesHint "Samples"

#define kParamIterations "iterations"
#define kParamIterationsLabel "Iterations"
#define kParamIterationsHint "Iterations"

#define kParamEstimate "estimate"
#define kParamEstimateLabel "Estimate"
#define kParamEstimateHint "Estimate"

#define kParamBlackPoint "blackPoint"
#define kParamBlackPointLabel "Black Point"
#define kParamBlackPointHint "Black Point"

#define kParamWhitePoint "whitePoint"
#define kParamWhitePointLabel "White Point"
#define kParamWhitePointHint "White Point"

#define kParamCentrePoint "centrePoint"
#define kParamCentrePointLabel "Centre Point"
#define kParamCentrePointHint "Centre Point"

#define kParamSlope "slope"
#define kParamSlopeLabel "Slope"
#define kParamSlopeHint "Slope"

#define kParamGamma "gamma"
#define kParamGammaLabel "Gamma"
#define kParamGammaHint "Gamma"


class EstimateGradePlugin : public ImageEffect
{
public:
    EstimateGradePlugin(OfxImageEffectHandle handle);

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    
    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
    ) OVERRIDE FINAL;

    virtual void changedParam(const InstanceChangedArgs &args, const std::string &paramName);

    virtual void estimate(double time);


private:
    Clip* _srcClip;
    Clip* _trgClip;
    Clip* _dstClip;
    ChoiceParam* _curve;
    IntParam* _samples;
    IntParam* _iterations;
    PushButtonParam* _estimate;
    RGBAParam* _whitePoint;
    RGBAParam* _blackPoint;
    RGBAParam* _centrePoint;
    RGBAParam* _slope;
    RGBAParam* _gamma;
};