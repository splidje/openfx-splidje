#ifndef FACETRANSLATIONMAPPLUGININTERACT_H
#define FACETRANSLATIONMAPPLUGININTERACT_H

#include "ofxsImageEffect.h"

using namespace OFX;


class FaceTranslationMapPluginInteract
    : public OverlayInteract
{
public:
    FaceTranslationMapPluginInteract(OfxInteractHandle handle, OFX::ImageEffect* effect);

    virtual bool draw(const DrawArgs &args);
};

class FaceTranslationMapPluginOverlayDescriptor
    : public DefaultEffectOverlayDescriptor<FaceTranslationMapPluginOverlayDescriptor, FaceTranslationMapPluginInteract>
{
};

#endif
