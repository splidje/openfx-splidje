#include "ofxsImageEffect.h"

using namespace OFX;


class CornerPinPluginInteract
    : public OverlayInteract
{
public:
    CornerPinPluginInteract(OfxInteractHandle handle, ImageEffect* effect);

    virtual bool draw(const DrawArgs &args);

private:
};

class CornerPinPluginOverlayDescriptor
    : public DefaultEffectOverlayDescriptor<CornerPinPluginOverlayDescriptor, CornerPinPluginInteract>
{
};
