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
    // auto plugin = (EstimateOffsetMapPlugin*)_effect;

    // std::vector<sword_t> swords;
    // plugin->getSwords(&swords);

    // auto ang = 2 * M_PI / 32;

    // for (auto sword : swords) {
    //     glColor3f(1, 1, 1);

    //     glBegin(GL_LINE_LOOP);
    //     glVertex2d(sword.from.x, sword.from.y);
    //     glVertex2d(sword.bladeTip.x, sword.bladeTip.y);
    //     glEnd();

    //     glColor3f(1, 0.5, 0.5);

    //     glBegin(GL_LINE_LOOP);
    //     glVertex2d(sword.from.x, sword.from.y);
    //     glVertex2d(sword.rHiltTip.x, sword.rHiltTip.y);
    //     glVertex2d(sword.bladeTip.x, sword.bladeTip.y);
    //     glVertex2d(sword.lHiltTip.x, sword.lHiltTip.y);
    //     glEnd();

    //     glBegin(GL_LINE_LOOP);
    //     for (double a = 0; a < 2 * M_PI; a += ang) {
    //         auto rx = sword.radius * cos(a);
    //         auto ry = sword.radius * sin(a);
    //         glVertex2d(sword.from.x + rx, sword.from.y + ry);
    //     }
    //     glEnd();
    // }

    return true;
}
