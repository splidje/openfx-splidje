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
    _smudgeRadius = fetchDoubleParam(kParamSmudgeRadius);
    _maxSmudgeLength = fetchDoubleParam(kParamMaxSmudgeLength);
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
    auto limitsWidth = limits.x2 - limits.x1;
    auto limitsHeight = limits.y2 - limits.y1;

    auto blackOutside = _blackOutside->getValueAtTime(args.time);
    auto smudgeRadius = _smudgeRadius->getValueAtTime(args.time);
    auto maxSmudgeLength = _maxSmudgeLength->getValueAtTime(args.time);
    auto iterations = _iterations->getValueAtTime(args.time);

    auto rndWidth = args.renderWindow.x2 - args.renderWindow.x1;
    auto rndHeight = args.renderWindow.y2 - args.renderWindow.y1;
    auto rndNumPixels = rndWidth * rndHeight;

    auto_ptr<float> tempPix(new float[comps]);
    auto_ptr<float> tempPix2(new float[comps]);

    // initialise offsets & differences
    auto_ptr<float> draws(new float[rndNumPixels * 2]);
    auto_ptr<float> diffs(new float[rndNumPixels * comps]);
    auto_ptr<bool> changed(new bool[rndNumPixels]);
    auto drawsPtr = draws.get();
    auto diffPtr = diffs.get();
    auto changedPtr = changed.get();
    for (auto y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (auto x=args.renderWindow.x1; x < args.renderWindow.x2; x++
            ,drawsPtr += 2, diffPtr += comps, changedPtr++)
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
            for (auto c=0; c < comps; c++) {
                auto trgVal = trgPix ? trgPix[c] : 0;
                diffPtr[c] = std::abs(trgVal - tempPix.get()[c]);
            }
        }
    }

    double radius = smudgeRadius * args.renderScale.x;
    double maxBladeLen = maxSmudgeLength * args.renderScale.x;
    std::random_device rd;
    std::default_random_engine eng(rd());
    eng.seed(_seed->getValueAtTime(args.time));
    std::uniform_real_distribution<double> distrFromX(trgROD.x1, trgROD.x2);
    std::uniform_real_distribution<double> distrFromY(trgROD.y1, trgROD.y2);
    auto not_zero = std::min(args.renderScale.x, args.renderScale.y);

    std::uniform_real_distribution<double> distrBladeAng(0, M_PI * 2);
    std::uniform_real_distribution<double> distrBladeLen(radius + not_zero, std::max(radius + not_zero, maxBladeLen));

    auto_ptr<float> scratchDraws(new float[rndNumPixels * 2]);
    auto_ptr<float> scratchDiffs(new float[rndNumPixels * comps]);

    std::vector<sword_t> swords;

    for (auto i=0; i < iterations; i++) {
        if (abort()) {return;}
        auto fromX = distrFromX(eng);
        auto fromY = distrFromY(eng);
        auto bladeAng = distrBladeAng(eng);
        auto blade = distrBladeLen(eng);
        auto bladeX = blade * cos(bladeAng);
        auto bladeY = blade * sin(bladeAng);
        auto bladeNormX = bladeX / blade;
        auto bladeNormY = bladeY / blade;
        auto tipX = fromX + bladeX;
        auto tipY = fromY + bladeY;

        auto hiltAng = acos(radius / blade);
        auto lHiltNormX = cos(hiltAng) * bladeNormX - sin(hiltAng) * bladeNormY;
        auto lHiltNormY = sin(hiltAng) * bladeNormX + cos(hiltAng) * bladeNormY;
        auto rHiltNormX = cos(-hiltAng) * bladeNormX - sin(-hiltAng) * bladeNormY;
        auto rHiltNormY = sin(-hiltAng) * bladeNormX + cos(-hiltAng) * bladeNormY;
        auto lHiltVectX = radius * lHiltNormX;
        auto lHiltVectY = radius * lHiltNormY;
        auto rHiltVectX = radius * rHiltNormX;
        auto rHiltVectY = radius * rHiltNormY;
        auto lHiltTipX = fromX + lHiltVectX;
        auto lHiltTipY = fromY + lHiltVectY;
        auto rHiltTipX = fromX + rHiltVectX;
        auto rHiltTipY = fromY + rHiltVectY;
        auto lHiltTipVectX = tipX - lHiltTipX;
        auto lHiltTipVectY = tipY - lHiltTipY;
        auto rHiltTipVectX = tipX - rHiltTipX;
        auto rHiltTipVectY = tipY - rHiltTipY;
        auto lHiltTipLen = lHiltTipVectX*lHiltTipVectX + lHiltTipVectY*lHiltTipVectY;
        auto rHiltTipLen = rHiltTipVectX*rHiltTipVectX + rHiltTipVectY*rHiltTipVectY;
        auto lHiltTipNormX = lHiltTipVectX / lHiltTipLen;
        auto lHiltTipNormY = lHiltTipVectY / lHiltTipLen;
        auto rHiltTipNormX = rHiltTipVectX / rHiltTipLen;
        auto rHiltTipNormY = rHiltTipVectY / rHiltTipLen;

        sword_t sword;
        toCanonicalSub({fromX, fromY}, args.renderScale, trgPar, &sword.from);
        toCanonicalSub({tipX, tipY}, args.renderScale, trgPar, &sword.bladeTip);
        toCanonicalSub({fromX + rHiltVectX, fromY + rHiltVectY}, args.renderScale, trgPar, &sword.rHiltTip);
        toCanonicalSub({fromX + lHiltVectX, fromY + lHiltVectY}, args.renderScale, trgPar, &sword.lHiltTip);
        sword.radius = radius / args.renderScale.x;

        swords.push_back(sword);

        OfxRectI region;
        region.x1 = std::max(args.renderWindow.x1, (int)std::floor(std::min(fromX - radius, tipX)));
        region.y1 = std::max(args.renderWindow.y1, (int)std::floor(std::min(fromY - radius, tipY)));
        region.x2 = std::min(args.renderWindow.x2, (int)std::ceil(std::max(fromX + radius, tipX)));
        region.y2 = std::min(args.renderWindow.y2, (int)std::ceil(std::max(fromY + radius, tipY)));

        double oldScore = 0;
        double newScore = 0;

        float* scratchDrawsPtr;
        float* scratchDiffPtr;

        for (auto y=region.y1; y < region.y2; y++) {
            if (abort()) {return;}
            for (auto x=region.x1; x < region.x2; x++) {
                auto dx = x - fromX;
                auto dy = y - fromY;
                auto bladeWays = dx * bladeNormX + dy * bladeNormY;
                if (bladeWays > blade || bladeWays < - radius) {
                    continue;
                }
                double power;
                auto maxHilt = radius * (1 - bladeWays / blade);
                auto lHiltTipWays = dx * lHiltTipVectX + dy * lHiltTipVectY;
                auto rHiltTipWays = dx * rHiltTipVectX + dy * rHiltTipVectY;
                if (lHiltTipWays >= 0 && rHiltTipWays >= 0) {
                    if (lHiltTipWays <= rHiltTipWays) {
                        auto ddx = lHiltTipX - x;
                        auto ddy = lHiltTipY - y;
                        auto lHiltWays = ddx * lHiltNormX + ddy * lHiltNormY;
                        if (lHiltWays < 0 || lHiltWays > maxHilt) {
                            continue;
                        }
                        power = lHiltWays / maxHilt;
                    } else {
                        auto ddx = rHiltTipX - x;
                        auto ddy = rHiltTipY - y;
                        auto rHiltWays = ddx * rHiltNormX + ddy * rHiltNormY;
                        if (rHiltWays < 0 || rHiltWays > maxHilt) {
                            continue;
                        }
                        power = rHiltWays / maxHilt;
                    }
                } else {
                    auto dist = sqrt(dx*dx + dy*dy);
                    if (dist > radius) {
                        continue;
                    }
                    power = 1 - dist / radius;
                }
                auto toTipX = tipX - x;
                auto toTipY = tipY - y;
                auto xOff = x - args.renderWindow.x1;
                auto yOff = y - args.renderWindow.y1;
                auto offset = rndWidth * yOff + xOff;
                drawsPtr = draws.get() + offset * 2;
                scratchDrawsPtr = scratchDraws.get() + offset * 2;
                changedPtr = changed.get() + offset;
                scratchDrawsPtr[0] = drawsPtr[0] + power * toTipX;
                scratchDrawsPtr[1] = drawsPtr[1] + power * toTipY;

                *changedPtr = true;

                diffPtr = diffs.get() + offset * comps;
                scratchDiffPtr = scratchDiffs.get() + offset * comps;
                bilinear(
                    x, y, trgImg.get(), tempPix.get(), comps, blackOutside
                );
                bilinear(
                    scratchDrawsPtr[0], scratchDrawsPtr[1]
                    ,srcImg.get(), tempPix2.get()
                    ,comps, blackOutside
                );
                auto tempPixPtr = tempPix.get();
                auto tempPix2Ptr = tempPix2.get();
                for (auto c=0; c < comps; c++
                    ,diffPtr++, scratchDiffPtr++, tempPixPtr++, tempPix2Ptr++)
                {
                    oldScore += *diffPtr;
                    *scratchDiffPtr = std::abs(*tempPix2Ptr - *tempPixPtr);
                    newScore += *scratchDiffPtr;
                }
            }
        }

        if (oldScore >= newScore) {
            for (auto y = region.y1; y < region.y2; y++) {
                if (abort()) {return;}
                auto yOff = y - args.renderWindow.y1;
                auto xOff = region.x1 - args.renderWindow.x1;
                auto offset = rndWidth * yOff + xOff;
                drawsPtr = draws.get() + offset * 2;
                scratchDrawsPtr = scratchDraws.get() + offset * 2;
                diffPtr = diffs.get() + offset * comps;
                scratchDiffPtr = scratchDiffs.get() + offset * comps;
                changedPtr = changed.get() + offset;
                for (auto x = region.x1; x < region.x2; x++
                    ,drawsPtr += 2, scratchDrawsPtr += 2, diffPtr += comps, scratchDiffPtr += comps, changedPtr++)
                {
                    if (!*changedPtr) {
                        continue;
                    }
                    *changedPtr = false; // wipe changed
                    for (auto c=0; c < 2; c++) {
                        drawsPtr[c] = scratchDrawsPtr[c];
                    }
                    for (auto c=0; c < comps; c++) {
                        diffPtr[c] = scratchDiffPtr[c];
                    }
                }
            }
        } else {
            // wipe changed
            for (auto y = region.y1; y < region.y2; y++) {
                if (abort()) {return;}
                changedPtr = (
                    changed.get()
                    + (rndWidth * (y - args.renderWindow.y1))
                    + (region.x1 - args.renderWindow.x1)
                );
                for (auto x = region.x1; x < region.x2; x++, changedPtr++) {
                    *changedPtr = false;
                }
            }
        }
    }

    // setSwords(&swords);
    // redrawOverlays();

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

void EstimateOffsetMapPlugin::setSwords(std::vector<sword_t>* swords) {
    _swordsLock.lock();
    _swords = *swords;
    _swordsLock.unlock();
}

void EstimateOffsetMapPlugin::getSwords(std::vector<sword_t>* swords) {
    _swordsLock.lock();
    *swords = _swords;
    _swordsLock.unlock();
}
