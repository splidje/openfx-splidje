#ifndef PATCHMATCHPLUGIN_H
#define PATCHMATCHPLUGIN_H

#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include <iostream>
#include <random>

using namespace OFX;

#define kPluginName "PatchMatch"
#define kPluginGrouping "Transform"
#define kPluginDescription \
"PatchMatch"

#define kPluginIdentifier "com.ajptechnical.openfx.PatchMatch"
#define kPluginVersionMajor 0
#define kPluginVersionMinor 2

#define kSourceClip "Source"
#define kTargetClip "Target"
#define kInitialClip "Initial"

#define kParamPatchSize "patchSize"

#define kParamStartLevel "startLevel"

#define kParamEndLevel "endLevel"

#define kParamNumIterations "numIterations"

#define kParamRandomSeed "seed"


class PatchMatchPlugin : public ImageEffect
{
public:
    PatchMatchPlugin(OfxImageEffectHandle handle);

    int calculateNumLevelsAtTime(double time);

    auto_ptr<std::default_random_engine> randEng;

private:
    virtual void getRegionsOfInterest(const RegionsOfInterestArguments& args, RegionOfInterestSetter &rois) OVERRIDE FINAL;

    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
    ) OVERRIDE FINAL;

    bool getRegionOfDefinition(const RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;

    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    
    Clip* _srcClip;
    Clip* _trgClip;
    Clip* _dstClip;
    Clip* _initClip;
    IntParam* _patchSize;
    IntParam* _endLevel;
    IntParam* _startLevel;
    IntParam* _numIterations;
    IntParam* _randomSeed;
};

#endif // def PATCHMATCHPLUGIN_H
