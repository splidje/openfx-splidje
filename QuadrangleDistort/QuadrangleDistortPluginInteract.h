#include "ofxsImageEffect.h"

using namespace OFX;


class QuadrangleDistortPluginInteract
    : public OverlayInteract
{
public:
    QuadrangleDistortPluginInteract(OfxInteractHandle handle, ImageEffect* effect);

    virtual bool draw(const DrawArgs &args);

private:

    virtual bool penDown(const PenArgs &args);
    virtual bool penUp(const PenArgs &args);
    virtual bool penMotion(const PenArgs &args);

    int movingHandle;
};

class CornerPinPluginOverlayDescriptor
    : public DefaultEffectOverlayDescriptor<CornerPinPluginOverlayDescriptor, QuadrangleDistortPluginInteract>
{
};
