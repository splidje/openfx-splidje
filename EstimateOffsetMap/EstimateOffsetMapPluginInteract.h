#ifndef ESTIMATEOFFSETMAPPLUGININTERACT_H
#define ESTIMATEOFFSETMAPPLUGININTERACT_H

#include "ofxsImageEffect.h"

using namespace OFX;


class EstimateOffsetMapPluginInteract
    : public OverlayInteract
{
public:
    EstimateOffsetMapPluginInteract(OfxInteractHandle handle, OFX::ImageEffect* effect);

    virtual bool draw(const DrawArgs &args);
};

class EstimateOffsetMapPluginOverlayDescriptor
    : public DefaultEffectOverlayDescriptor<EstimateOffsetMapPluginOverlayDescriptor, EstimateOffsetMapPluginInteract>
{
};

#endif
