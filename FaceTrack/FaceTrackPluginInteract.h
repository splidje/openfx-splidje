#ifndef FACETRACKPLUGININTERACT_H
#define FACETRACKPLUGININTERACT_H

#include "ofxsImageEffect.h"
#include "FaceTrackMacros.h"

using namespace OFX;


class FaceTrackPluginInteract
    : public OverlayInteract
{
public:
    FaceTrackPluginInteract(OfxInteractHandle handle, OFX::ImageEffect* effect);

    virtual bool draw(const DrawArgs &args);
};

class FaceTrackOverlayDescriptor
    : public DefaultEffectOverlayDescriptor<FaceTrackOverlayDescriptor, FaceTrackPluginInteract>
{
};

#endif
