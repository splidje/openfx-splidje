#include "PatchMatchPlugin.h"
#include "PatchMatcher.h"
#include "ofxsCoords.h"
#include <thread>

PatchMatchPlugin::PatchMatchPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
{
    srcClip = fetchClip(kSourceClip);
    assert(srcClip && (srcClip->getPixelComponents() == ePixelComponentRGB ||
    	    srcClip->getPixelComponents() == ePixelComponentRGBA));
    trgClip = fetchClip(kTargetClip);
    assert(trgClip && (trgClip->getPixelComponents() == ePixelComponentRGB ||
    	    trgClip->getPixelComponents() == ePixelComponentRGBA));
    dstClip = fetchClip(kOfxImageEffectOutputClipName);
    assert(dstClip && (dstClip->getPixelComponents() == ePixelComponentRGB ||
    	    dstClip->getPixelComponents() == ePixelComponentRGBA));
    initClip = fetchClip(kInitialClip);
    assert(initClip && (initClip->getPixelComponents() == ePixelComponentRGB ||
    	    initClip->getPixelComponents() == ePixelComponentRGBA));
    patchSize = fetchIntParam(kParamPatchSize);
    startLevel = fetchIntParam(kParamStartLevel);
    endLevel = fetchIntParam(kParamEndLevel);
    iterations = fetchDoubleParam(kParamIterations);
    acceptableScore = fetchDoubleParam(kParamAcceptableScore);
    randomSeed = fetchIntParam(kParamRandomSeed);
    logCoords = fetchInt2DParam(kParamLogCoords);
}

// the overridden render function
void PatchMatchPlugin::render(const RenderArguments &args)
{
    PatchMatcher(this, args).render();
}

bool PatchMatchPlugin::isIdentity(const IsIdentityArguments &args, 
                                  Clip * &identityClip, double &/*identityTime*/
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
)
{
    return false;
}

bool PatchMatchPlugin::getRegionOfDefinition(const RegionOfDefinitionArguments &args, OfxRectD &rod)
{
    auto numLevels = calculateNumLevelsAtTime(args.time);
    auto endL = std::max(
        1, std::min(numLevels, endLevel->getValueAtTime(args.time))
    );
    rod = trgClip->getRegionOfDefinition(args.time);
    double scale = 1;
    for (int l=numLevels; l > endL; l--) {scale *= 0.5;}
    rod.x1 *= scale;
    rod.x2 *= scale;
    rod.y1 *= scale;
    rod.y2 *= scale;
    return true;
}

int PatchMatchPlugin::calculateNumLevelsAtTime(double time)
{
    auto boundsA = srcClip->getRegionOfDefinition(time);
    auto boundsB = trgClip->getRegionOfDefinition(time);
    auto minDim = std::min(
        std::min(boundsWidth(boundsA), boundsWidth(boundsB))
        ,std::min(boundsHeight(boundsA), boundsHeight(boundsB))
    );
    auto pSize = patchSize->getValueAtTime(time);
    if (minDim <= pSize) {
        return 1;
    }
    int numLevels = int(std::log2(double(minDim) / pSize)) + 1;
    startLevel->setDisplayRange(1, numLevels);
    endLevel->setDisplayRange(1, numLevels);
    return numLevels;
}
