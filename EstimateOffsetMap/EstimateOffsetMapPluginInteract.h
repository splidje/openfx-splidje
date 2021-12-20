#include "ofxsImageEffect.h"

using namespace OFX;


class EstimateOffsetMapPluginInteract
    : public OverlayInteract
{
public:
    EstimateOffsetMapPluginInteract(OfxInteractHandle handle, ImageEffect* effect);

    virtual bool draw(const DrawArgs &args);
};

class EstimateOffsetMapPluginOverlayDescriptor
    : public DefaultEffectOverlayDescriptor<EstimateOffsetMapPluginOverlayDescriptor, EstimateOffsetMapPluginInteract>
{
};
