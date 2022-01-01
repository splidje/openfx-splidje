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
    _minRadius = fetchDoubleParam(kParamMinRadius);
    _maxRadius = fetchDoubleParam(kParamMaxRadius);
    _minRotate = fetchDoubleParam(kParamMinRotate);
    _maxRotate = fetchDoubleParam(kParamMaxRotate);
    _minScale = fetchDoubleParam(kParamMinScale);
    _maxScale = fetchDoubleParam(kParamMaxScale);
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

OfxPointD pItoD(OfxPointI& p) {
    OfxPointD pD;
    pD.x = p.x;
    pD.y = p.y;
    return pD;
}

template<class T>
inline T* pixelAddress(T* start, const OfxRectI* rod, int comps, int x, int y) {
    x = std::max(std::min(x, rod->x2 - 1), rod->x1);
    y = std::max(std::min(y, rod->y2 - 1), rod->y1);
    auto offset = (y - rod->y1) * (rod->x2 - rod->x1) + (x - rod->x1);
    return start + offset * comps;
}

inline bool operator<(const OfxPointI& lhs, const OfxPointI& rhs) {
    return lhs.x < rhs.x || lhs.x == rhs.x && lhs.y < rhs.y;
}

inline OfxPointD operator/(const OfxPointD& p, double d) {
    OfxPointD quot;
    quot.x = p.x / d;
    quot.y = p.y / d;
    return quot;
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

    auto blackOutside = _blackOutside->getValueAtTime(args.time);
    auto minRadiusCan = _minRadius->getValueAtTime(args.time);
    auto maxRadiusCan = _maxRadius->getValueAtTime(args.time);
    auto minRotateEuc = _minRotate->getValueAtTime(args.time);
    auto maxRotateEuc = _maxRotate->getValueAtTime(args.time);
    auto minScale = _minScale->getValueAtTime(args.time);
    auto maxScale = _maxScale->getValueAtTime(args.time);
    auto maxTranslateCan = _maxTranslate->getValueAtTime(args.time);
    auto iterations = _iterations->getValueAtTime(args.time);
    auto seed = _seed->getValueAtTime(args.time);

    auto rndWidth = args.renderWindow.x2 - args.renderWindow.x1;
    auto rndHeight = args.renderWindow.y2 - args.renderWindow.y1;
    auto rndNumPixels = rndWidth * rndHeight;
       
    // initialise offsets & differences
    auto_ptr<float> draws(new float[rndNumPixels * 2]);
    auto_ptr<float> diffs(new float[rndNumPixels]);
    auto drawsPtr = draws.get();
    auto diffPtr = diffs.get();
    float* trgPix;
    for (auto y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (auto x=args.renderWindow.x1; x < args.renderWindow.x2; x++
            ,drawsPtr += 2, diffPtr++)
        {
            if (abort()) {return;}
            drawsPtr[0] = x;
            drawsPtr[1] = y;
            float* srcPix;
            if (blackOutside) {
                srcPix = (float*)srcImg->getPixelAddress(x, y);
            } else {
                srcPix = (float*)srcImg->getPixelAddressNearest(x, y);
            }
            trgPix = (float*)trgImg->getPixelAddress(x, y);
            *diffPtr = 0;
            for (auto c=0; c < comps; c++) {
                auto srcVal = srcPix ? srcPix[c] : 0;
                auto trgVal = trgPix ? trgPix[c] : 0;
                *diffPtr += std::abs(trgVal - srcVal);
            }
        }
    }

    std::random_device rd;
    std::default_random_engine eng(rd());
    eng.seed(seed);
    std::uniform_real_distribution<double> distrRadius(minRadiusCan * args.renderScale.x, maxRadiusCan * args.renderScale.x);
    std::uniform_real_distribution<double> distrRot(toRadians(minRotateEuc), toRadians(maxRotateEuc));
    std::uniform_real_distribution<double> distrScale(minScale, maxScale);
    std::uniform_real_distribution<double> distrTranslate(0, maxTranslateCan * args.renderScale.x);
    std::uniform_real_distribution<double> distrTransDir(0, M_PI * 2);
    std::uniform_real_distribution<double> distrSrcX(srcROD.x1, srcROD.x2);
    std::uniform_real_distribution<double> distrSrcY(srcROD.y1, srcROD.y2);

    std::vector<OfxPointI> touched;
    std::set<OfxPointI> touchedSet;
    std::set<int> locked;
    Quadrangle quad;

    auto_ptr<float> scratchDraws(new float[rndNumPixels * 2]);
    auto_ptr<float> scratchDiffs(new float[rndNumPixels]);
    float* scratchDrawPtr;
    float* scratchDiffPtr;
    for (auto i=0; i < iterations; i++) {
        auto centreX = distrSrcX(eng);
        auto centreY = distrSrcY(eng);
        auto radius = distrRadius(eng);
        auto radiusSq = radius * radius;
        auto scaleX = distrScale(eng);
        auto scaleY = distrScale(eng);
        auto rot = distrRot(eng);
        auto translate = distrTranslate(eng);
        auto transDir = distrTransDir(eng);
        auto transX = translate * cos(transDir);
        auto transY = translate * sin(transDir);

        auto trgCentreX = centreX + transX;
        auto trgCentreY = centreY + transY;
        auto trgRadiusX = scaleX * radius;
        auto trgRadiusY = scaleY * radius;

        OfxRectI bounds;
        bounds.x1 = std::max(
            (int)std::floor(trgCentreX - trgRadiusX)
            ,args.renderWindow.x1
        );
        bounds.y1 = std::max(
            (int)std::floor(trgCentreY - trgRadiusY)
            ,args.renderWindow.y1
        );
        bounds.x2 = std::min(
            (int)std::ceil(trgCentreX + trgRadiusX)
            ,args.renderWindow.x2
        );
        bounds.y2 = std::min(
            (int)std::ceil(trgCentreY + trgRadiusY)
            ,args.renderWindow.y2
        );

        touched.clear();
        touchedSet.clear();

        double oldScore = 0;
        double newScore = 0;
        auto_ptr<float> srcPix(new float[comps]);
        OfxPointI p;
        for (p.y=bounds.y1; p.y < bounds.y2; p.y++) {
            if (abort()) {return;}
            for (p.x=bounds.x1; p.x < bounds.x2; p.x++) {
                // relative to translated centre
                auto dx = p.x - trgCentreX;
                auto dy = p.y - trgCentreY;
                // undo scale
                dx /= scaleX;
                dy /= scaleY;
                // undo the rotate
                auto rotX = dx * cos(-rot) - dy * sin(-rot);
                auto rotY = dx * sin(-rot) + dy * cos(-rot);
                // within the radius?
                auto distSq = rotX*rotX + rotY*rotY;
                if (distSq > radiusSq) {
                    continue;
                }
                // set to draw from this src point
                auto srcX = centreX + rotX;
                auto srcY = centreY + rotY;
                touched.push_back(p);
                touchedSet.insert(p);
                scratchDrawPtr = pixelAddress(scratchDraws.get(), &args.renderWindow, 2, p.x, p.y);
                scratchDrawPtr[0] = srcX;
                scratchDrawPtr[1] = srcY;
                diffPtr = pixelAddress(diffs.get(), &args.renderWindow, 1, p.x, p.y);
                oldScore += *diffPtr;
                scratchDiffPtr = pixelAddress(scratchDiffs.get(), &args.renderWindow, 1, p.x, p.y);
                *scratchDiffPtr = 0;
                trgPix = (float*)trgImg->getPixelAddress(p.x, p.y);
                bilinear(
                    srcX
                    ,srcY
                    ,srcImg.get()
                    ,srcPix.get()
                    ,comps
                    ,blackOutside
                );
                for (auto c=0; c < comps; c++) {
                    *scratchDiffPtr += std::abs(trgPix[c] - srcPix.get()[c]);
                }
                newScore += *scratchDiffPtr;
            }
        }

        // std::cout << newScore << " " << oldScore << std::endl;
        if (newScore >= oldScore) {
            continue;
        }

        // propagate effects
        for (auto j=0; j < touched.size(); j++) {
            if (abort()) {return;}
            auto p = touched[j];
            scratchDrawPtr = pixelAddress(scratchDraws.get(), &args.renderWindow, 2, p.x, p.y);
            quad.edges[0].p.x = scratchDrawPtr[0];
            quad.edges[0].p.y = scratchDrawPtr[1];
            auto sumP = quad.edges[0].p;
            for (auto k=0; k < 4; k++) {
                locked.clear();
                auto curP = p;
                auto skip = false;
                for (auto l=0; l < 3; l++) {
                    auto rot = (k + l) % 4;
                    curP.x += rot < 3 ? 1 - rot : 0;
                    curP.y += rot > 0 ? 2 - rot : 0;
                    if (
                        curP.x < args.renderWindow.x1
                        || curP.x >= args.renderWindow.x2
                        || curP.y < args.renderWindow.y1
                        || curP.y >= args.renderWindow.y2
                    ) {
                        skip = true;
                        break;
                    }
                    if (touchedSet.find(curP) != touchedSet.end()) {
                        locked.insert(l + 1);
                        if (locked.size() > 2) {
                            skip = true;
                            break;
                        }
                        scratchDrawPtr = pixelAddress(scratchDraws.get(), &args.renderWindow, 2, curP.x, curP.y);
                        sumP.x += scratchDrawPtr[0];
                        sumP.y += scratchDrawPtr[1];
                    } else {
                        scratchDrawPtr = pixelAddress(draws.get(), &args.renderWindow, 2, curP.x, curP.y);
                    }
                    quad.edges[l + 1].p.x = scratchDrawPtr[0];
                    quad.edges[l + 1].p.y = scratchDrawPtr[1];
                }
                if (skip) {
                    continue;
                }
                quad.initialise();
                if (quad.isValid()) {
                    continue;
                }
                auto avgP = sumP / locked.size();
                curP = p;
                for (auto l=0; l < 3; l++) {
                    auto rot = (k + l) % 4;
                    curP.x += rot < 3 ? 1 - rot : 0;
                    curP.y += rot > 0 ? 2 - rot : 0;
                    if (locked.find(l + 1) != locked.end()) {
                        continue;
                    }
                    quad.edges[l + 1].p = avgP;
                    touched.push_back(curP);
                    touchedSet.insert(curP);
                    scratchDrawPtr = pixelAddress(scratchDraws.get(), &args.renderWindow, 2, curP.x, curP.y);
                    scratchDrawPtr[0] = quad.edges[l + 1].p.x;
                    scratchDrawPtr[1] = quad.edges[l + 1].p.y;
                    scratchDiffPtr = pixelAddress(scratchDiffs.get(), &args.renderWindow, 1, curP.x, curP.y);
                    *scratchDiffPtr = 0;
                    trgPix = (float*)trgImg->getPixelAddress(curP.x, curP.y);
                    bilinear(
                        scratchDrawPtr[0]
                        ,scratchDrawPtr[1]
                        ,srcImg.get()
                        ,srcPix.get()
                        ,comps
                        ,blackOutside
                    );
                    for (auto c=0; c < comps; c++) {
                        *scratchDiffPtr += std::abs(trgPix[c] - srcPix.get()[c]);
                    }
                    newScore += *scratchDiffPtr;
                    diffPtr = pixelAddress(diffs.get(), &args.renderWindow, 1, curP.x, curP.y);
                    oldScore += *diffPtr;
                }
            }
        }

        if (newScore >= oldScore) {
            continue;
        }

        for (auto p : touched) {
            scratchDrawPtr = pixelAddress(scratchDraws.get(), &args.renderWindow, 2, p.x, p.y);
            drawsPtr = pixelAddress(draws.get(), &args.renderWindow, 2, p.x, p.y);
            drawsPtr[0] = scratchDrawPtr[0];
            drawsPtr[1] = scratchDrawPtr[1];
            scratchDiffPtr = pixelAddress(scratchDiffs.get(), &args.renderWindow, 1, p.x, p.y);
            diffPtr = pixelAddress(diffs.get(), &args.renderWindow, 1, p.x, p.y);
            *diffPtr = *scratchDiffPtr;
        }
    }

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
