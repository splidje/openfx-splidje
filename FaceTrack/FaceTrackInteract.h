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
    Double2DParam* _faceTopLeft;
    Double2DParam* _faceBottomRight;

    Double2DParam* _eyebrowLeftLeft;
    Double2DParam* _eyebrowLeftRight;
    Double2DParam* _eyebrowRightLeft;
    Double2DParam* _eyebrowRightRight;
};

class FaceTrackOverlayDescriptor
    : public DefaultEffectOverlayDescriptor<FaceTrackOverlayDescriptor, FaceTrackInteract>
{
};
