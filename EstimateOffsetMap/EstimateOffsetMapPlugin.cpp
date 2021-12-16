#include "EstimateOffsetMapPlugin.h"
#include "../QuadrangleDistort/QuadrangleDistort.h"

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

typedef struct {
    OfxPointI pos;
    Quadrangle* quadPtr;
    std::set<int> changed;
} dirtyQuad_t;

void EstimateOffsetMapPlugin::render(const OFX::RenderArguments &args)
{
    auto_ptr<Image> srcImg(_srcClip->fetchImage(args.time));
    auto_ptr<Image> trgImg(_trgClip->fetchImage(args.time));
    if (!srcImg.get() || !trgImg.get()) {
        return;
    }
    auto srcROD = srcImg->getRegionOfDefinition();
    auto srcWidth = srcROD.x2 - srcROD.x1;
    auto srcHeight = srcROD.y2 - srcROD.y1;
    auto srcPar = srcImg->getPixelAspectRatio();
    auto comps = std::min(srcImg->getPixelComponentCount(), trgImg->getPixelComponentCount());
    auto_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    assert(dstImg.get());
    auto dstComps = dstImg->getPixelComponentCount();
    assert(dstComps >= 2);

    auto iterations = _iterations->getValueAtTime(args.time);

    auto width = args.renderWindow.x2 - args.renderWindow.x1;
    auto height = args.renderWindow.y2 - args.renderWindow.y1;
    auto numPixels = width * height;

    // initialise quads
    auto_ptr<Quadrangle> quads(new Quadrangle[numPixels]);
    auto quadPtr = quads.get();
    for (auto y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (auto x=args.renderWindow.x1; x < args.renderWindow.x2; x++, quadPtr++) {
            for (auto e=0; e < 4; e++) {
                quadPtr->edges[e].p.x = x + (e == 1 || e == 2);
                quadPtr->edges[e].p.y = y + (e > 1);
            }
            quadPtr->initialise();
        }
    }

    // initialise differences
    auto_ptr<float> diffs(new float[numPixels * comps]);
    auto diffPtr = diffs.get();
    for (auto y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (auto x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            auto srcPix = (float*)srcImg->getPixelAddress(x, y);
            auto trgPix = (float*)trgImg->getPixelAddress(x, y);
            for (auto c=0; c < comps; c++, diffPtr++) {
                auto srcVal = srcPix ? srcPix[c] : 0;
                auto trgVal = trgPix ? trgPix[c] : 0;
                *diffPtr = std::abs(trgVal - srcVal);
            }
        }
    }

    // iterate
    OfxPointI pos;
    std::vector<dirtyQuad_t> dirtied;
    std::set<Quadrangle*> visited;
    std::set<int> changed;
    for (auto i=0; i < iterations; i++) {
        pos.x = args.renderWindow.x1 + rand() % width;
        pos.y = args.renderWindow.y1 + rand() % height;
        quadPtr = quads.get() + pos.y * width + pos.x;
        quadPtr->edges[0].p.x = srcROD.x1 + (rand() % srcWidth);
        quadPtr->edges[0].p.y = srcROD.y1 + (rand() % srcHeight);
        quadPtr->initialise();
        dirtied.clear();
        dirtied.push_back({pos, quadPtr, {0}});

        for (auto d : dirtied) {
            visited.insert(d.quadPtr);
            changed.clear();
            quadPtr->fix(&d.changed, &changed);
            OfxPointI adjPos;
            std::set<int> locked;
            for (auto yOff=-1; yOff <= 1; yOff += 2) {
                for (auto xOff=-1; xOff <= 1; xOff += 2) {
                    adjPos.x = pos.x + xOff;
                    adjPos.y = pos.y + yOff;
                    auto adjQuadPtr = quads.get() + adjPos.y * width + adjPos.x;
                    if (visited.find(adjQuadPtr) != visited.end()) {
                        continue;
                    }
                    locked.clear();
                    if (xOff == -1 && yOff == -1) {
                        if (changed.find(0) != changed.end()) {
                            locked.insert(2);
                        }
                    } else if (xOff == 0 && yOff == -1) {
                        if (changed.find(0) != changed.end()) {
                            locked.insert(3);
                        }
                        if (changed.find(1) != changed.end()) {
                            locked.insert(2);
                        }
                    } else if (xOff == 1 && yOff == -1) {
                        if (changed.find(1) != changed.end()) {
                            locked.insert(3);
                        }
                    } else if (xOff == 1 && yOff == 0) {
                        if (changed.find(1) != changed.end()) {
                            locked.insert(0);
                        }
                        if (changed.find(2) != changed.end()) {
                            locked.insert(3);
                        }
                    } else if (xOff == 1 && yOff == 1) {
                        if (changed.find(2) != changed.end()) {
                            locked.insert(0);
                        }
                    } else if (xOff == 0 && yOff == 1) {
                        if (changed.find(2) != changed.end()) {
                            locked.insert(1);
                        }
                        if (changed.find(3) != changed.end()) {
                            locked.insert(0);
                        }
                    } else if (xOff == -1 && yOff == 1) {
                        if (changed.find(3) != changed.end()) {
                            locked.insert(1);
                        }
                    } else if (xOff == -1 && yOff == 0) {
                        if (changed.find(0) != changed.end()) {
                            locked.insert(1);
                        }
                        if (changed.find(3) != changed.end()) {
                            locked.insert(1);
                        }
                    }
                    if (locked.size() > 0) {
                        dirtied.push_back({adjPos, adjQuadPtr, locked});
                    }
                }
            }
        }
    }

    // write quads to pixels
    quadPtr = quads.get();
    diffPtr = diffs.get();
    for (auto y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (auto x=args.renderWindow.x1; x < args.renderWindow.x2; x++, quadPtr++, diffPtr += comps) {
            auto dstPix = (float*)dstImg->getPixelAddress(x, y);
            if (!dstPix) {
                continue;
            }
            dstPix[0] = srcPar * (quadPtr->edges[0].p.x - x) / args.renderScale.x;
            dstPix[1] = (quadPtr->edges[0].p.y - y) / args.renderScale.y;
            for (auto c=2; c < dstComps; c++) {
                dstPix[c] = 0;
            }
        }
    }
}
