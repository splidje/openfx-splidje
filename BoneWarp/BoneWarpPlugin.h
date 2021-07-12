#include "ofxsImageEffect.h"
#include "ofxsMacros.h"

using namespace OFX;


class BoneWarpPlugin : public ImageEffect
{
public:
    BoneWarpPlugin(OfxImageEffectHandle handle);

private:
    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
    ) OVERRIDE FINAL;

    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

private:
    Clip* _srcClip;
    Clip* _dstClip;
    Double2DParam* _jointFromCentres[2];
    Double2DParam* _jointToCentres[2];
    DoubleParam* _jointRadii[2];
};
