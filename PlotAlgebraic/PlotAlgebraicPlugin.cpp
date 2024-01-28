#include "PlotAlgebraicPlugin.h"
#include "ofxsCoords.h"
#include <cmath>


PlotAlgebraicPlugin::PlotAlgebraicPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
{
    _srcClip = fetchClip(kSourceClip);
    assert(
        _srcClip && (
            _srcClip->getPixelComponents() == ePixelComponentRGB
            || _srcClip->getPixelComponents() == ePixelComponentRGBA
            || _srcClip->getPixelComponents() == ePixelComponentAlpha
        )
    );
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    assert(
        _dstClip && (
            _dstClip->getPixelComponents() == ePixelComponentRGB
            || _dstClip->getPixelComponents() == ePixelComponentRGBA
            || _dstClip->getPixelComponents() == ePixelComponentAlpha
        )
    );
}

bool _incrementIntPair(float &a, float &b, int n) {
    if (a == -n && b == 1) {return false;}
    if (a == n && b < n) {
        b++;
    } else if (b == n && a > -n) {
        a--;
    } else {
        b--;
    }
    return true;
}

void PlotAlgebraicPlugin::render(const RenderArguments &args)
{
    std::unique_ptr<Image> srcImg(_srcClip->fetchImage(args.time));
    std::unique_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    auto nComponents = srcImg->getPixelComponentCount();

    auto bounds = srcImg->getBounds();

    auto width = bounds.x2 - bounds.x1;
    auto height = bounds.y2 - bounds.y1;

    auto n = (height > width ? height : width) >> 1;

    float a = n, b = 1;

    auto PIX = (float*)dstImg->getPixelData();
    for (auto y=bounds.y1; y < bounds.y2; y++) {
        for (auto x=bounds.x1; x < bounds.x2; x++) {
            for (int c=0; c < nComponents; c++, PIX++) {
                *PIX = c > 2;
            }
        }
    }

    do {
        float c = n, d = 1;
        do {
            PIX = (float*)dstImg->getPixelAddressNearest(round(a / b) + (width >> 1), round(c / d) + (height >> 1));
            if (!PIX) {continue;}
            for (int c = 0; c < 3; c++) {
                PIX[c] += 1;
            }
        } while(_incrementIntPair(c, d, n));
    } while(_incrementIntPair(a,b,n));
    

    // auto mapping = _mapping->getValue();

    // switch (mapping) {
    //     case 0:
    //     case 1:
    //     case 2:
    //         renderCurve(args, mapping, srcImg.get(), dstImg.get(), components);
    //         break;
    //     case 3:
    //         renderMatrix(args, srcImg.get(), dstImg.get(), components);
    //         break;
    // }
}
