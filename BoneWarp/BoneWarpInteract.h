#include "ofxsImageEffect.h"
#include "BoneWarpMacros.h"

using namespace OFX;


class BoneWarpInteract
    : public OverlayInteract
{
public:
    BoneWarpInteract(OfxInteractHandle handle, OFX::ImageEffect* effect);

    virtual bool draw(const DrawArgs &args);

private:
    Double2DParam* _jointFromCentres[2];
    Double2DParam* _jointToCentres[2];
    DoubleParam* _jointRadii[2];
};

class BoneWarpOverlayDescriptor
    : public DefaultEffectOverlayDescriptor<BoneWarpOverlayDescriptor, BoneWarpInteract>
{
};
