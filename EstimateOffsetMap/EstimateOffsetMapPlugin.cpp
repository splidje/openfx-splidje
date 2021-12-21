#include "EstimateOffsetMapPlugin.h"
#include "../QuadrangleDistort/QuadrangleDistort.h"
#include "ofxsCoords.h"
#include <iostream>
#include <random>

using namespace QuadrangleDistort;
using namespace Coords;


EstimateOffsetMapPlugin::EstimateOffsetMapPlugin(OfxImageEffectHandle handle)
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
    _blackOutside = fetchBooleanParam(kParamBlackOutside);
    _minScale = fetchDoubleParam(kParamMinScale);
    _maxScale = fetchDoubleParam(kParamMaxScale);
    _minRotate = fetchDoubleParam(kParamMinRotate);
    _maxRotate = fetchDoubleParam(kParamMaxRotate);
    _maxTranslate = fetchDoubleParam(kParamMaxTranslate);
    _iterations = fetchIntParam(kParamIterations);
    _seed = fetchIntParam(kParamSeed);
}

bool EstimateOffsetMapPlugin::getRegionOfDefinition(const RegionOfDefinitionArguments &args, OfxRectD &rod) {
    rod = _trgClip->getRegionOfDefinition(args.time);
    return true;
}

void EstimateOffsetMapPlugin::getClipPreferences(ClipPreferencesSetter &clipPreferences) {
#ifdef OFX_EXTENSIONS_NATRON
    OfxRectI format;
    _trgClip->getFormat(format);
    clipPreferences.setOutputFormat(format);
#endif
    clipPreferences.setPixelAspectRatio(*_dstClip, _trgClip->getPixelAspectRatio());
    // TODO: Should be chosen as minimum num components of src or trg
    // but no more than trg components is fine (some will be 0 if not in src)
    clipPreferences.setClipComponents(*_dstClip, _trgClip->getPixelComponents());
}

void EstimateOffsetMapPlugin::getRegionsOfInterest(const RegionsOfInterestArguments &args, RegionOfInterestSetter &rois)
{
    rois.setRegionOfInterest(*_srcClip, _srcClip->getRegionOfDefinition(args.time));
}

inline double toRadians(double euc) {
    return M_PI * euc / 180.0;
}

void EstimateOffsetMapPlugin::render(const OFX::RenderArguments &args)
{
    auto_ptr<Image> srcImg(_srcClip->fetchImage(args.time));
    auto_ptr<Image> trgImg(_trgClip->fetchImage(args.time));
    if (!srcImg.get() || !trgImg.get()) {
        return;
    }
    auto_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    assert(dstImg.get());
    auto dstComps = dstImg->getPixelComponentCount();
    assert(dstComps >= 2);
    auto srcROD = srcImg->getRegionOfDefinition();
    auto srcWidth = srcROD.x2 - srcROD.x1;
    auto srcHeight = srcROD.y2 - srcROD.y1;
    auto srcPar = srcImg->getPixelAspectRatio();
    auto trgROD = trgImg->getRegionOfDefinition();
    auto trgWidth = trgROD.x2 - trgROD.x1;
    auto trgHeight = trgROD.y2 - trgROD.y1;
    auto trgToSrcScaleX = (double)srcWidth / (double)trgWidth;
    auto trgToSrcScaleY = (double)srcHeight / (double)trgHeight;
    auto trgPar = trgImg->getPixelAspectRatio();
    auto comps = std::min(srcImg->getPixelComponentCount(), trgImg->getPixelComponentCount());

    OfxRectI limits;
    limits.x1 = srcROD.x1 - 1;
    limits.y1 = srcROD.y1 - 1;
    limits.x2 = srcROD.x2 + 1;
    limits.y2 = srcROD.y2 + 1;

    auto blackOutside = _blackOutside->getValueAtTime(args.time);
    auto minScale = _minScale->getValueAtTime(args.time);
    auto maxScale = _maxScale->getValueAtTime(args.time);
    auto minRotateEuc = _minRotate->getValueAtTime(args.time);
    auto maxRotateEuc = _maxRotate->getValueAtTime(args.time);
    auto maxTranslate = _maxTranslate->getValueAtTime(args.time);
    auto iterations = _iterations->getValueAtTime(args.time);
    auto seed = _seed->getValueAtTime(args.time);

    auto rndWidth = args.renderWindow.x2 - args.renderWindow.x1;
    auto rndHeight = args.renderWindow.y2 - args.renderWindow.y1;
    auto rndNumPixels = rndWidth * rndHeight;
       
    auto_ptr<float> tempPix(new float[comps]);

    // initialise offsets & differences
    auto_ptr<float> draws(new float[rndNumPixels * 2]);
    auto_ptr<float> diffs(new float[rndNumPixels]);
    auto_ptr<bool> changed(new bool[rndNumPixels]);
    auto drawsPtr = draws.get();
    auto diffPtr = diffs.get();
    auto changedPtr = changed.get();
    auto diffSum = 0;
    for (auto y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (auto x=args.renderWindow.x1; x < args.renderWindow.x2; x++
            ,drawsPtr += 2, diffPtr++, changedPtr++)
        {
            if (abort()) {return;}
            *changedPtr = false;
            auto srcX = (x - trgROD.x1) * trgToSrcScaleX + srcROD.x1;
            auto srcY = (y - trgROD.y1) * trgToSrcScaleY + srcROD.y1;
            for (auto c=0; c < 2; c++) {
                drawsPtr[0] = srcX;
                drawsPtr[1] = srcY;
            }
            bilinear(
                srcX
                ,srcY
                ,srcImg.get()
                ,tempPix.get()
                ,comps
                ,blackOutside
            );
            auto trgPix = (float*)trgImg->getPixelAddress(x, y);
            *diffPtr = 0;
            for (auto c=0; c < comps; c++) {
                auto trgVal = trgPix ? trgPix[c] : 0;
                *diffPtr += std::abs(trgVal - tempPix.get()[c]);
            }
            diffSum += *diffPtr;
        }
    }
    auto diffMean = (double)diffSum / (double)rndNumPixels;

    std::random_device rd;
    std::default_random_engine eng(rd());
    eng.seed(seed);
    std::uniform_real_distribution<double> distrScale(minScale, maxScale);
    std::uniform_real_distribution<double> distrRot(toRadians(minRotateEuc), toRadians(maxRotateEuc));
    std::uniform_real_distribution<double> distrTranslate(0, maxTranslate);
    std::uniform_real_distribution<double> distrTransDir(0, M_PI * 2);
    std::uniform_real_distribution<double> distrSrcX(srcROD.x1, srcROD.x2);
    std::uniform_real_distribution<double> distrSrcY(srcROD.y1, srcROD.y2);

    for (auto i=0; i < iterations; i++) {
        auto centreX = distrSrcX(eng);
        auto centreY = distrSrcY(eng);
        auto scaleX = distrScale(eng);
        auto scaleY = distrScale(eng);
        auto rot = distrRot(eng);
        auto translate = distrTranslate(eng);
        auto transDir = distrTransDir(eng);
        auto transX = translate * cos(transDir);
        auto transY = translate * sin(transDir);

        diffSum = 0;
        diffPtr = diffs.get();
        drawsPtr = draws.get();
        for (auto y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
            if (abort()) {return;}
            for (auto x=args.renderWindow.x1; x < args.renderWindow.x2; x++, diffPtr++, drawsPtr += 2) {
                if (*diffPtr <= diffMean) {
                    diffSum += *diffPtr;
                    continue;
                }
                auto dx = drawsPtr[0] - centreX;
                auto dy = drawsPtr[1] - centreY;
                dx *= scaleX;
                dy *= scaleY;
                auto rotX = dx * cos(rot) - dy * sin(rot);
                auto rotY = dx * sin(rot) + dy * cos(rot);
                rotX += transX;
                rotY += transY;
                // rotX = std::max((float)limits.x1, std::min((float)limits.x2, (float)rotX));
                // rotY = std::max((float)limits.y1, std::min((float)limits.y2, (float)rotY));
                drawsPtr[0] = rotX;
                drawsPtr[1] = rotY;
                auto trgPix = (float*)trgImg->getPixelAddress(x, y);
                bilinear(
                    rotX
                    ,rotY
                    ,srcImg.get()
                    ,tempPix.get()
                    ,comps
                    ,blackOutside
                );
                for (auto c=0; c < comps; c++) {
                    diffSum += std::abs(trgPix[c] - tempPix.get()[c]);
                }
            }
        }
        diffMean = (double)diffSum / (double)rndNumPixels;

    // write out and readjust values
    drawsPtr = draws.get();
    for (auto y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (auto x=args.renderWindow.x1; x < args.renderWindow.x2; x++
            ,drawsPtr += 2)
        {
            auto dstPix = (float*)dstImg->getPixelAddress(x, y);
            if (!dstPix) {
                continue;
            }
            dstPix[0] = (drawsPtr[0] * srcPar - x * trgPar) / args.renderScale.x;
            if (dstComps > 1) {
                dstPix[1] = (drawsPtr[1] - y) / args.renderScale.y;
            }
            for (auto c=2; c < dstComps; c++) {
                dstPix[c] = 0;
            }
        }
    }
}
