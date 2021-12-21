#include "EstimateOffsetMapPluginInteract.h"
#include "EstimateOffsetMapPluginMacros.h"
#include "EstimateOffsetMapPlugin.h"
#include <iostream>

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif


EstimateOffsetMapPluginInteract::EstimateOffsetMapPluginInteract(OfxInteractHandle handle, ImageEffect* effect)
    : OverlayInteract(handle) {}

bool EstimateOffsetMapPluginInteract::draw(const DrawArgs &args) {
    return true;
}
