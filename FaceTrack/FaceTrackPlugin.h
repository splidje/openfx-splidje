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

    void trackTime(double t);

    void refreshShapePredictorFaceLandmarksFile();

private:
    dlib::frontal_face_detector _detector;
    dlib::shape_predictor _predictor;
    Clip* _srcClip;
    PushButtonParam* _track;
    PushButtonParam* _trackForward;
    Double2DParam* _faceBottomLeft;
    Double2DParam* _faceTopRight;

    Double2DParam* _jaw[kLandmarkCountJaw];
    Double2DParam* _eyebrowRight[kLandmarkCountEyebrowRight];
    Double2DParam* _eyebrowLeft[kLandmarkCountEyebrowLeft];
    Double2DParam* _noseBridge[kLandmarkCountNoseBridge];
    Double2DParam* _noseBottom[kLandmarkCountNoseBottom];
    Double2DParam* _eyeRight[kLandmarkCountEyeRight];
    Double2DParam* _eyeLeft[kLandmarkCountEyeLeft];
    Double2DParam* _mouthOutside[kLandmarkCountMouthOutside];
    Double2DParam* _mouthInside[kLandmarkCountMouthInside];
};
