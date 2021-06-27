#include "TranslateMapPlugin.h"
#include "ofxsCoords.h"


TranslateMapPlugin::TranslateMapPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
{
    _transClip = fetchClip(kTranslationsClip);
    assert(_transClip && (_transClip->getPixelComponents() == ePixelComponentRGB ||
    	    _transClip->getPixelComponents() == ePixelComponentRGBA));
    _srcClip = fetchClip(kSourceClip);
    assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentRGB ||
    	    _srcClip->getPixelComponents() == ePixelComponentRGBA));
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentRGB ||
    	    _dstClip->getPixelComponents() == ePixelComponentRGBA));
}

void TranslateMapPlugin::getClipPreferences(ClipPreferencesSetter &clipPreferences) {
    clipPreferences.setClipComponents(*_dstClip, _srcClip->getPixelComponents());
}

bool TranslateMapPlugin::isIdentity(const IsIdentityArguments &args, 
                                  Clip * &identityClip, double &/*identityTime*/
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
)
{
    return false;
}

// the overridden render function
void TranslateMapPlugin::render(const RenderArguments &args)
{
    auto_ptr<Image> transImg(
        _transClip->fetchImage(args.time, _transClip->getRegionOfDefinition(args.time))
    );
    if (!transImg.get()) {return;}
    auto transROD = transImg->getRegionOfDefinition();
    auto trans_component_count = transImg->getPixelComponentCount();
    if (trans_component_count < 2) {return;}
    auto_ptr<Image> srcImg(_srcClip->fetchImage(args.time));
    auto srcROD = srcImg->getRegionOfDefinition();
    auto src_component_count = srcImg->getPixelComponentCount();
    auto_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    auto dst_component_count = dstImg->getPixelComponentCount();
    auto component_count = std::min(
        src_component_count, dst_component_count
    );

    // default to black
    for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            auto outPix = (float*)dstImg->getPixelAddress(x, y);
            for (int c=0; c < dst_component_count; c++, outPix++) {
                *outPix = 0;
            }
        }
    }

    auto srcPix = (float*)srcImg->getPixelData();
    auto transPix = (float*)transImg->getPixelData();
    for (int y=transROD.y1; y < transROD.y2; y++) {
        for (int x=transROD.x1;
            x < transROD.x2;
            x++, srcPix += src_component_count, transPix += trans_component_count
        ) {
            auto new_x = x + transPix[0] * args.renderScale.x;
            auto new_y = y + transPix[1] * args.renderScale.y;
            if (
                new_x >= args.renderWindow.x1 && new_x < args.renderWindow.x2
                && new_y >= args.renderWindow.y1 && new_y < args.renderWindow.y2
            ) {
                // std::cout << new_x << ", " << new_y << std::endl;
                auto outPix = (float*)dstImg->getPixelAddress(new_x, new_y);
                for (int c=0; c < component_count; c++, outPix++) {
                    // std::cout << c << " ";
                    *outPix = srcPix[c];
                }
                // std::cout << std::endl;
            }
        }
    }
    std::cout << "returning" << std::endl;
}
