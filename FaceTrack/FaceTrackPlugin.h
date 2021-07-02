#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/image_io.h>
#include <iostream>
#include "FaceTrackMacros.h"

using namespace OFX;


class FaceTrackPlugin : public ImageEffect
{
public:
    FaceTrackPlugin(OfxImageEffectHandle handle);

private:
    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
    ) OVERRIDE FINAL;

    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

    virtual void changedParam(const InstanceChangedArgs &args, const std::string &paramName) OVERRIDE FINAL;

    void refreshShapePredictorFaceLandmarksFile();

private:
    dlib::frontal_face_detector _detector;
    dlib::shape_predictor _predictor;
    Clip* _srcClip;
    PushButtonParam* _track;
    Double2DParam* _faceTopLeft;
    Double2DParam* _faceBottomRight;

    Double2DParam* _eyebrowLeftLeft;
    Double2DParam* _eyebrowLeftRight;
    Double2DParam* _eyebrowRightLeft;
    Double2DParam* _eyebrowRightRight;
};
