#include "EstimateOffsetMapPlugin.h"
#include "../QuadrangleDistort/QuadrangleDistort.h"
#include <iostream>

using namespace QuadrangleDistort;


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
    auto srcROD = srcImg->getRegionOfDefinition();
    auto trgROD = trgImg->getRegionOfDefinition();

    OfxRectI relevantArea;
    relevantArea.x1 = std::min(srcROD.x1, trgROD.x1);
    relevantArea.y1 = std::min(srcROD.y1, trgROD.y1);
    relevantArea.x2 = std::max(srcROD.x2, trgROD.x2);
    relevantArea.y2 = std::max(srcROD.y2, trgROD.y2);
    auto areaWidth = relevantArea.x2 - relevantArea.x1;
    auto areaHeight = relevantArea.y2 - relevantArea.y1;

    auto srcPar = srcImg->getPixelAspectRatio();    
    auto comps = std::min(srcImg->getPixelComponentCount(), trgImg->getPixelComponentCount());
    auto_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    assert(dstImg.get());
    auto dstComps = dstImg->getPixelComponentCount();
    assert(dstComps >= 2);

    auto iterations = _iterations->getValueAtTime(args.time);
    std::srand(_seed->getValueAtTime(args.time));

    auto width = args.renderWindow.x2 - args.renderWindow.x1;
    auto height = args.renderWindow.y2 - args.renderWindow.y1;
    auto numPixels = width * height;

    // initialise offsets & differences
    auto_ptr<float> offsets(new float[numPixels * 2]);
    auto_ptr<float> diffs(new float[numPixels * comps]);
    auto_ptr<bool> changed(new bool[numPixels]);
    auto offsetPtr = offsets.get();
    auto diffPtr = diffs.get();
    auto changedPtr = changed.get();
    for (auto y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (auto x=args.renderWindow.x1; x < args.renderWindow.x2; x++, offsetPtr += 2, diffPtr += comps, changedPtr++) {
            if (abort()) {
                return;
            }
            auto srcPix = (float*)srcImg->getPixelAddress(x, y);
            auto trgPix = (float*)trgImg->getPixelAddress(x, y);
            auto dstPix = (float*)dstImg->getPixelAddress(x, y);
            *changedPtr = false;
            for (auto c=0; c < dstComps; c++) {
                if (dstPix) {
                    dstPix[c] = 0;
                }
                if (c < comps) {
                    auto srcVal = srcPix ? srcPix[c] : 0;
                    auto trgVal = trgPix ? trgPix[c] : 0;
                    diffPtr[c] = std::abs(trgVal - srcVal);
                }
                if (c < 2) {
                    offsetPtr[c] = 0;
                }
            }
        }
    }

    auto max_radius = std::max(areaWidth, areaHeight) >> 3;

    // iterate
    auto_ptr<float> newDiffs(new float[numPixels * comps]);
    for (auto i=0; i < iterations; i++) {
        if (abort()) {
            return;
        }
        auto centX = relevantArea.x1 + std::rand() % areaWidth;
        auto centY = relevantArea.y1 + std::rand() % areaHeight;
        auto radius = std::rand() % (max_radius << 1) - max_radius;
        auto centralForce = std::rand() % 8 - 4;
        double radiusSq = radius * radius;
        offsetPtr = offsets.get();
        diffPtr = diffs.get();
        changedPtr = changed.get();
        auto newDiffPtr = newDiffs.get();
        double oldScore = 0;
        double newScore = 0;
        for (auto y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
            for (auto x=args.renderWindow.x1; x < args.renderWindow.x2; x++, offsetPtr += 2, diffPtr += comps, newDiffPtr += comps, changedPtr++) {
                if (abort()) {
                    return;
                }
                auto dstPix = (float*)dstImg->getPixelAddress(x, y);
                auto offsetX = dstPix ? dstPix[0] : 0;
                auto offsetY = dstPix ? dstPix[1] : 0;
                auto srcX = x + offsetX * args.renderScale.x;
                auto srcY = y + offsetY * args.renderScale.y;
                auto distX = srcX - centX;
                auto distY = srcY - centY;
                auto distSq = distX*distX + distY*distY;
                if (distSq <= QUADRANGLEDISTORT_DELTA || distSq > radiusSq) {
                    continue;
                }
                auto force = centralForce * (radiusSq - distSq) / radiusSq;
                auto dist = std::sqrt(distSq);
                srcX += distX * force / dist;
                srcY += distY * force / dist;
                offsetPtr[0] = srcX - x;
                offsetPtr[1] = srcY - y;
                auto srcPix = (float*)srcImg->getPixelAddress(srcX, srcY);
                auto trgPix = (float*)trgImg->getPixelAddress(x, y);
                for (auto c=0; c < comps; c++) {
                    auto srcVal = srcPix ? srcPix[c] : 0;
                    auto trgVal = trgPix ? trgPix[c] : 0;
                    oldScore += diffPtr[c];
                    newDiffPtr[c] = std::abs(trgVal - srcVal);
                    newScore += newDiffPtr[c];
                }
                *changedPtr = true;
            }
        }

        if (oldScore == newScore) {
            // wipe changed
            changedPtr = changed.get();
            for (auto y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
                for (auto x=args.renderWindow.x1; x < args.renderWindow.x2; x++, changedPtr++) {
                    *changedPtr = false;
                }
            }
        } else {
            // update
            diffPtr = diffs.get();
            offsetPtr = offsets.get();
            newDiffPtr = newDiffs.get();
            changedPtr = changed.get();
            for (auto y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
                for (auto x=args.renderWindow.x1; x < args.renderWindow.x2; x++, offsetPtr += 2, diffPtr += comps, newDiffPtr += comps, changedPtr++) {
                    if (!*changedPtr) {
                        continue;
                    }
                    *changedPtr = false; // clean up
                    auto dstPix = (float*)dstImg->getPixelAddress(x, y);
                    if (dstPix) {
                        dstPix[0] = offsetPtr[0] / args.renderScale.x;
                        dstPix[1] = offsetPtr[1] / args.renderScale.y;
                    }
                    for (auto c=0; c < comps; c++) {
                        diffPtr[c] = newDiffPtr[c];
                    }
                }
            }
        }
    }
}
