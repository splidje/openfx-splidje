#include "ofxsImageEffect.h"
#include "FaceTrackMacros.h"

using namespace OFX;


class FaceTrackInteract
    : public OverlayInteract
{
public:
    FaceTrackInteract(OfxInteractHandle handle, OFX::ImageEffect* effect);

    virtual bool draw(const DrawArgs &args);

private:
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

class FaceTrackOverlayDescriptor
    : public DefaultEffectOverlayDescriptor<FaceTrackOverlayDescriptor, FaceTrackInteract>
{
};
