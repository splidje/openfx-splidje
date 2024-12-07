#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include <iostream>
#include <vector>
#include <eigen3/Eigen/Eigen>

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

#define kParamMapping "mapping"
#define kParamMappingLabel "Mapping"
#define kParamMappingHint "Mapping"

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

#define kParamMatrixRed "matrixRed"
#define kParamMatrixRedLabel "Matrix Red"
#define kParamMatrixRedHint "Matrix Red"

#define kParamMatrixGreen "matrixGreen"
#define kParamMatrixGreenLabel "Matrix Green"
#define kParamMatrixGreenHint "Matrix Green"

#define kParamMatrixBlue "matrixBlue"
#define kParamMatrixBlueLabel "Matrix Blue"
#define kParamMatrixBlueHint "Matrix Blue"

#define kParamMatrixAlpha "matrixAlpha"
#define kParamMatrixAlphaLabel "Matrix Alpha"
#define kParamMatrixAlphaHint "Matrix Alpha"

#define kParamX1 "x1"

#define kParamY1 "y1"

#define kParamSlope1 "slope1"

#define kParamX2 "x2"

#define kParamY2 "y2"

#define kParamX3 "x3"

#define kParamY3 "y3"

#define kParamSlope3 "slope3"


typedef std::tuple<double, double, double> point3d_t;


class EstimateGradePlugin : public ImageEffect
{
public:
    EstimateGradePlugin(OfxImageEffectHandle handle);

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

    void renderCurve(const RenderArguments &args, int mapping, Image* srcImg, Image* dstImg, int components);
    void renderMatrix(const RenderArguments &args, Image* srcImg, Image* dstImg, int components);
    void renderCube(const RenderArguments &args, Image* srcImg, Image* dstImg, int components);
    
    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
    ) OVERRIDE FINAL;

    virtual void changedParam(const InstanceChangedArgs &args, const std::string &paramName);

    virtual void estimate(double time);

    void estimateCurve(double time, int mapping, int samples, int iterations, Image* srcImg, Image* trgImg, int components, OfxRectI isect, double horizScale);
    void estimateMatrix(double time, int samples, Image* srcImg, Image* trgImg, int components, OfxRectI isect, double horizScale);
    void estimateCube(double time, int samples, Image* srcImg, Image* trgImg, int components, OfxRectI isect, double horizScale);


private:
    Clip* _srcClip;
    Clip* _trgClip;
    Clip* _dstClip;
    ChoiceParam* _mapping;
    IntParam* _samples;
    IntParam* _iterations;
    PushButtonParam* _estimate;
    RGBAParam* _whitePoint;
    RGBAParam* _blackPoint;
    RGBAParam* _centrePoint;
    RGBAParam* _slope;
    RGBAParam* _gamma;
    RGBAParam* _matrixRed;
    RGBAParam* _matrixGreen;
    RGBAParam* _matrixBlue;
    RGBAParam* _matrixAlpha;
    RGBAParam* _x1;
    RGBAParam* _y1;
    RGBAParam* _slope1;
    RGBAParam* _x2;
    RGBAParam* _y2;
    RGBAParam* _x3;
    RGBAParam* _y3;
    RGBAParam* _slope3;

    std::vector<double> _cube_src_points;
    std::vector<double> _cube_trg_points;
    std::vector<std::vector<int>> _cube_src_tetrahedra;
    std::vector<Eigen::Matrix4d> _cube_src_tetrahedron_inverse_barycentric_matrices;
    std::vector<std::array<double, 3>> _cube_src_tetrahedron_bounds_minimum;
    std::vector<std::array<double, 3>> _cube_src_tetrahedron_bounds_maximum;
};
