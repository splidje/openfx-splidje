#include "ofxsImageEffect.h"
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/image_io.h>
#include "FaceTrackPluginBaseMacros.h"

using namespace OFX;


class FaceTrackPluginBase : public ImageEffect
{
public:
    typedef struct {
        Double2DParam* bottomLeft;
        Double2DParam* topRight;
        Double2DParam* landmarks[kLandmarkCount];
    } FaceParams;

    FaceTrackPluginBase(OfxImageEffectHandle handle);

    static void defineFaceParams(ImageEffectDescriptor* desc, PageParamDescriptor* page, std::string prefix);

    static std::string landmarkIndexToParamName(int index);

    static void drawFace(FaceParams* faceParams, double t);

protected:
    void fetchFaceParams(FaceParams* faceParams, std::string prefix);

    void trackClipAtTime(Clip* clip, FaceParams* faceParams, double t);

    void addFaceParamsToSlaveTo(OverlayInteract* interact, FaceParams* faceParams);

private:
    dlib::frontal_face_detector _detector;
    dlib::shape_predictor _predictor;
};
