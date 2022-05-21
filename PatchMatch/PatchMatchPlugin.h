#ifndef PATCHMATCHPLUGIN_H
#define PATCHMATCHPLUGIN_H

#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include <iostream>

using namespace OFX;

#define kPluginName "PatchMatch"
#define kPluginGrouping "Transform"
#define kPluginDescription \
"PatchMatch"

#define kPluginIdentifier "com.ajptechnical.splidje.openfx.PatchMatch"
#define kPluginVersionMajor 0
#define kPluginVersionMinor 1

#define kSourceClip "Source"
#define kTargetClip "Target"
#define kInitialClip "Initial"

#define kParamPatchSize "patchSize"
#define kParamPatchSizeLabel "Patch Size"
#define kParamPatchSizeHint "Patch Size"

#define kParamStartLevel "startLevel"
#define kParamStartLevelLabel "Start Level"
#define kParamStartLevelHint "Start Level"

#define kParamEndLevel "endLevel"
#define kParamEndLevelLabel "End Level"
#define kParamEndLevelHint "End Level"

#define kParamIterations "iterations"
#define kParamIterationsLabel "Iterations"
#define kParamIterationsHint "Iterations"

#define kParamAcceptableScore "acceptableScore"
#define kParamAcceptableScoreLabel "Acceptable Score"
#define kParamAcceptableScoreHint "Acceptable Score"
#define kParamAcceptableScoreDefault -1

#define kParamSpatialImpairmentFactor "spatialImpairmentFactor"
#define kParamSpatialImpairmentFactorLabel "Spatial Impairment Factor"
#define kParamSpatialImpairmentFactorHint "Spatial Impairment Factor"
#define kParamSpatialImpairmentFactorDefault 0

#define kParamRandomSeed "seed"
#define kParamRandomSeedLabel "Random Seed"
#define kParamRandomSeedHint "Random Seed"

#define kParamLogCoords "logCoords"
#define kParamLogCoordsLabel "Log Coords"
#define kParamLogCoordsHint "Log Coords"


class PatchMatchPlugin : public ImageEffect
{
public:
    PatchMatchPlugin(OfxImageEffectHandle handle);

    int calculateNumLevelsAtTime(double time);

    Clip* srcClip;
    Clip* trgClip;
    Clip* dstClip;
    Clip* initClip;
    IntParam* patchSize;
    IntParam* endLevel;
    IntParam* startLevel;
    DoubleParam* iterations;
    DoubleParam* acceptableScore;
    DoubleParam* spatialImpairmentFactor;
    IntParam* randomSeed;
    Int2DParam* logCoords;

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    
    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
    ) OVERRIDE FINAL;

    bool getRegionOfDefinition(const RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;
};

#endif // def PATCHMATCHPLUGIN_H
