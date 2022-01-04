#include "PatchMatchPlugin.h"
#include "PatchMatcher.h"
#include "ofxsCoords.h"
#include "PatchMatchUtils.h"


PatchMatchPlugin::PatchMatchPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
{
    _srcClip = fetchClip(kSourceClip);
    assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentRGB ||
    	    _srcClip->getPixelComponents() == ePixelComponentRGBA));
    _trgClip = fetchClip(kTargetClip);
    assert(_trgClip && (_trgClip->getPixelComponents() == ePixelComponentRGB ||
    	    _trgClip->getPixelComponents() == ePixelComponentRGBA));
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentRGB ||
    	    _dstClip->getPixelComponents() == ePixelComponentRGBA));
    _initClip = fetchClip(kInitialClip);
    assert(_initClip && (_initClip->getPixelComponents() == ePixelComponentRGB ||
    	    _initClip->getPixelComponents() == ePixelComponentRGBA));
    _patchSize = fetchIntParam(kParamPatchSize);
    _startLevel = fetchIntParam(kParamStartLevel);
    _endLevel = fetchIntParam(kParamEndLevel);
    _numIterations = fetchIntParam(kParamNumIterations);
    _randomSeed = fetchIntParam(kParamRandomSeed);
}

void PatchMatchPlugin::getRegionsOfInterest(const RegionsOfInterestArguments& args, RegionOfInterestSetter &rois) {
    rois.setRegionOfInterest(*_srcClip, _srcClip->getRegionOfDefinition(args.time) * args.renderScale);
    rois.setRegionOfInterest(*_trgClip, _trgClip->getRegionOfDefinition(args.time) * args.renderScale);
}

// the overridden render function
void PatchMatchPlugin::render(const RenderArguments &args)
{
    auto_ptr<VectorGrid> srcImg(VectorGrid::fromClip(_srcClip, args.time));
    if (!srcImg.get()) {return;}
    auto_ptr<VectorGrid> trgImg(VectorGrid::fromClip(_trgClip, args.time));
    if (!trgImg.get()) {return;}

    srand(_randomSeed->getValueAtTime(args.time));
    auto numIterations = _numIterations->getValueAtTime(args.time);
    auto patchSize = _patchSize->getValueAtTime(args.time);

    auto numLevels = calculateNumLevelsAtTime(args.time);
    auto endLevel = std::max(numLevels, _endLevel->getValueAtTime(args.time));
    auto startLevel = std::min(endLevel, _startLevel->getValueAtTime(args.time));
    auto_ptr<VectorGrid> srcImgSc;
    auto_ptr<VectorGrid> trgImgSc;
    auto_ptr<PatchMatcher> patchMatcher;
    auto_ptr<VectorGrid> translateMap;
    for (auto level=startLevel; level <= endLevel; level++) {
        if (patchMatcher.get()) {
            translateMap.reset(patchMatcher->releaseTranslateMap());
        }
        if (level != numLevels) {
            auto scale = std::pow(0.5, numLevels - level);
            srcImgSc.reset(srcImg->scale(scale));
            trgImgSc.reset(trgImg->scale(scale));
            patchMatcher.reset(new PatchMatcher(srcImgSc.get(), trgImgSc.get(), patchSize));
        } else {
            patchMatcher.reset(new PatchMatcher(srcImg.get(), trgImg.get(), patchSize));
        }
        patchMatcher->randomInitialise();
        if (translateMap.get()) {
            patchMatcher->iterate(std::min(2, numIterations));
            patchMatcher->merge(translateMap.get(), 2);
        }
    }
    patchMatcher->iterate(numIterations);
    translateMap.reset(patchMatcher->releaseTranslateMap());
    *translateMap /= args.renderScale;

    translateMap->toClip(_dstClip, args.time, args.renderWindow);
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
        1, std::min(numLevels, _endLevel->getValueAtTime(args.time))
    );
    rod = _srcClip->getRegionOfDefinition(args.time);
    rod *= std::pow(0.5, numLevels - endL);
    return true;
}

int PatchMatchPlugin::calculateNumLevelsAtTime(double time)
{
    auto boundsA = _srcClip->getRegionOfDefinition(time);
    auto boundsB = _trgClip->getRegionOfDefinition(time);
    auto minDim = std::min(
        std::min(rectWidth(boundsA), rectWidth(boundsB))
        ,std::min(rectHeight(boundsA), rectHeight(boundsB))
    );
    auto pSize = _patchSize->getValueAtTime(time);
    if (minDim <= pSize) {
        return 1;
    }
    int numLevels = int(std::log2(double(minDim) / pSize)) + 1;
    _startLevel->setDisplayRange(1, numLevels);
    _endLevel->setDisplayRange(1, numLevels);
    return numLevels;
}
