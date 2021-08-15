#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include "FaceTranslationMapPluginMacros.h"
#include "../FaceTrackPluginBase/FaceTrackPluginBase.h"
#include "FaceTranslationMapPluginInteract.h"
#include <mutex>

using namespace OFX;


class FaceTranslationMapPlugin : public FaceTrackPluginBase
{
    friend class FaceTranslationMapPluginInteract;

public:
    FaceTranslationMapPlugin(OfxImageEffectHandle handle);

private:
    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
    ) OVERRIDE FINAL;

    std::string getSelectedOuputOptionLabel(double t);

    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

    void calculateRelative(double t);

    virtual void changedParam(const InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

private:
    Clip* _srcClip;
    Clip* _trgClip;
    Clip* _dstClip;
    PushButtonParam* _srcTrack;
    PushButtonParam* _srcTrackAll;
    IntParam* _srcHighFreqRemovalCount;
    PushButtonParam* _srcRemoveHighFreqs;
    PushButtonParam* _trgTrack;
    PushButtonParam* _trgTrackAll;
    IntParam* _referenceFrame;
    PushButtonParam* _calcRel;
    PushButtonParam* _calcRelAll;
    ChoiceParam* _output;
    DoubleParam* _feather;
    FaceParams _srcFaceParams;
    FaceParams _trgFaceParams;
    FaceParams _relFaceParams;
};
