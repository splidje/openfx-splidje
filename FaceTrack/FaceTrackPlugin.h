#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include "FaceTrackMacros.h"
#include "../FaceTrackPluginBase/FaceTrackPluginBase.h"
#include "FaceTrackPluginInteract.h"

using namespace OFX;


class FaceTrackPlugin : public FaceTrackPluginBase
{
    friend class FaceTrackPluginInteract;

public:
    FaceTrackPlugin(OfxImageEffectHandle handle);

private:
    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
    ) OVERRIDE FINAL;

    virtual void changedParam(const InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

private:
    Clip* _srcClip;
    PushButtonParam* _track;
    PushButtonParam* _trackForward;
    FaceParams _face;
};
