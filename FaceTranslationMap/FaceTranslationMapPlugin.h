#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include "FaceTranslationMapPluginMacros.h"
#include "../FaceTrackPluginBase/FaceTrackPluginBase.h"
#include "FaceTranslationMapPluginInteract.h"
#include "../TriangleMaths/TriangleMaths.h"
#include <mutex>

using namespace OFX;


class FaceTranslationMapPlugin : public FaceTrackPluginBase
{
    friend class FaceTranslationMapPluginInteract;

public:
    FaceTranslationMapPlugin(OfxImageEffectHandle handle);

    void getFaceMesh(std::vector<TriangleMaths::Triangle> *vect) {
        _faceMeshLock.lock();
        if (!_faceMeshInitialised) {
            _faceMeshLock.unlock();
            return;
        }
        for (auto tri : _faceMesh) {
            vect->push_back(tri);
        }
        _faceMeshLock.unlock();
    }

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

    void stabiliseSourceAtTime(double t);

    virtual void changedParam(const InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

    void _generateFaceMesh(std::vector<OfxPointD>* vertices);

private:
    Clip* _srcClip;
    Clip* _trgClip;
    Clip* _dstClip;
    PushButtonParam* _srcTrack;
    PushButtonParam* _srcTrackAll;
    PushButtonParam* _srcClearKeyframeAll;
    IntParam* _srcHighFreqRemovalCount;
    PushButtonParam* _srcRemoveHighFreqs;
    PushButtonParam* _trgTrack;
    PushButtonParam* _trgTrackAll;
    IntParam* _referenceFrame;
    PushButtonParam* _stabSrc;
    Double2DParam* _stabCentre;
    Double2DParam* _stabTrans;
    DoubleParam* _stabScale;
    DoubleParam* _stabRot;
    PushButtonParam* _calcRel;
    PushButtonParam* _calcRelAll;
    ChoiceParam* _output;
    DoubleParam* _feather;
    FaceParams _srcFaceParams;
    FaceParams _trgFaceParams;
    FaceParams _relFaceParams;
    TriangleMaths::Triangle _faceMesh[113];
    bool _faceMeshInitialised = false;
    std::mutex _faceMeshLock;
};
