#include "EstimateOffsetMapPlugin.h"


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
    _bottomLeft = fetchDouble2DParam(kParamBottomLeft);
    _bottomRight = fetchDouble2DParam(kParamBottomRight);
    _topRight = fetchDouble2DParam(kParamTopRight);
    _topLeft = fetchDouble2DParam(kParamTopLeft);
    _fix = fetchPushButtonParam(kParamFix);
}

bool EstimateOffsetMapPlugin::isIdentity(const IsIdentityArguments &args, 
                                  Clip * &identityClip, double &/*identityTime*/
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
)
{
    return true;
}

void EstimateOffsetMapPlugin::render(const OFX::RenderArguments &args) {
    return;
}
