#include "FaceTranslationMapPluginInteract.h"
#include "FaceTranslationMapPlugin.h"
#include <iostream>

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif


FaceTranslationMapPluginInteract::FaceTranslationMapPluginInteract(OfxInteractHandle handle, OFX::ImageEffect* effect)
    : OFX::OverlayInteract(handle)
{
    auto faceTranslationMapPlugin = (FaceTranslationMapPlugin*)effect;
    addParamToSlaveTo(faceTranslationMapPlugin->_output);
    faceTranslationMapPlugin->addFaceParamsToSlaveTo(this, &faceTranslationMapPlugin->_srcFaceParams);
    faceTranslationMapPlugin->addFaceParamsToSlaveTo(this, &faceTranslationMapPlugin->_trgFaceParams);
}

bool FaceTranslationMapPluginInteract::draw(const DrawArgs &args)
{
    auto faceTranslationMapPlugin = (FaceTranslationMapPlugin*)_effect;

    // shadow (uses GL_PROJECTION)
    glMatrixMode(GL_PROJECTION);
    glMatrixMode(GL_MODELVIEW); // Modelview should be used on Nuke

    auto outputLabel = faceTranslationMapPlugin->getSelectedOuputOptionLabel(args.time);

    if (outputLabel == kParamOutputChoiceSourceLabel) {
        faceTranslationMapPlugin->drawFace(&faceTranslationMapPlugin->_srcFaceParams, args.time);
    } else if (outputLabel == kParamOutputChoiceTargetLabel) {
        faceTranslationMapPlugin->drawFace(&faceTranslationMapPlugin->_trgFaceParams, args.time);
    }

    return true;
}
