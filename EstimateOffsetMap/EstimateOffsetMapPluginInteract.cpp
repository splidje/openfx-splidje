#include "EstimateOffsetMapPluginInteract.h"
#include "EstimateOffsetMapPlugin.h"

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif


EstimateOffsetMapPluginInteract::EstimateOffsetMapPluginInteract(OfxInteractHandle handle, OFX::ImageEffect* effect)
    : OFX::OverlayInteract(handle)
{
    auto plugin = (EstimateOffsetMapPlugin*)effect;
    addParamToSlaveTo(plugin->_bottomLeft);
    addParamToSlaveTo(plugin->_bottomRight);
    addParamToSlaveTo(plugin->_topRight);
    addParamToSlaveTo(plugin->_topLeft);
}

bool EstimateOffsetMapPluginInteract::draw(const DrawArgs &args)
{
    auto plugin = (EstimateOffsetMapPlugin*)_effect;

    glColor3f(1, 1, 1);

    OfxPointD p;
    glBegin(GL_LINE_LOOP);
    p = plugin->_bottomLeft->getValueAtTime(args.time);
    glVertex2d(p.x, p.y);
    p = plugin->_bottomRight->getValueAtTime(args.time);
    glVertex2d(p.x, p.y);
    p = plugin->_topRight->getValueAtTime(args.time);
    glVertex2d(p.x, p.y);
    p = plugin->_topLeft->getValueAtTime(args.time);
    glVertex2d(p.x, p.y);
    glEnd();

    return true;
}
