#include "FaceTrackPluginInteract.h"
#include "FaceTrackPlugin.h"

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif


FaceTrackPluginInteract::FaceTrackPluginInteract(OfxInteractHandle handle, OFX::ImageEffect* effect)
    : OFX::OverlayInteract(handle)
{
    auto faceTrackPlugin = (FaceTrackPlugin*)effect;
    faceTrackPlugin->addFaceParamsToSlaveTo(this, &faceTrackPlugin->_face);
}

bool FaceTrackPluginInteract::draw(const DrawArgs &args)
{
    auto faceTrackPlugin = (FaceTrackPlugin*)_effect;

    // shadow (uses GL_PROJECTION)
    glMatrixMode(GL_PROJECTION);
    glMatrixMode(GL_MODELVIEW); // Modelview should be used on Nuke

    faceTrackPlugin->drawFace(&faceTrackPlugin->_face, args.time);

    return true;
}
