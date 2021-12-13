#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include "EstimateOffsetMapPluginMacros.h"
#include "EstimateOffsetMapPluginInteract.h"

using namespace OFX;


class EstimateOffsetMapPlugin : public ImageEffect
{
    friend class EstimateOffsetMapPluginInteract;

public:
    EstimateOffsetMapPlugin(OfxImageEffectHandle handle);

private:
    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
    ) OVERRIDE FINAL;

    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

    // virtual void changedParam(const InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

private:
    Clip* _srcClip;
    Clip* _trgClip;
    Clip* _dstClip;
    Double2DParam* _bottomLeft;
    Double2DParam* _bottomRight;
    Double2DParam* _topRight;
    Double2DParam* _topLeft;
    PushButtonParam* _fix;
};
