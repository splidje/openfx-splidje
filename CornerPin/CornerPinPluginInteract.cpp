#include "CornerPinPluginInteract.h"
#include "CornerPinPluginMacros.h"
#include "CornerPinPlugin.h"

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif


CornerPinPluginInteract::CornerPinPluginInteract(OfxInteractHandle handle, ImageEffect* effect)
    : OverlayInteract(handle) {
        addParamToSlaveTo(effect->fetchDouble2DParam(kParamBottomLeft));
        addParamToSlaveTo(effect->fetchDouble2DParam(kParamBottomRight));
        addParamToSlaveTo(effect->fetchDouble2DParam(kParamTopRight));
        addParamToSlaveTo(effect->fetchDouble2DParam(kParamTopLeft));
}

bool CornerPinPluginInteract::draw(const DrawArgs &args) {
    glColor3f(1, 1, 1);

    OfxPointD p;
    glBegin(GL_LINE_LOOP);
    p = _effect->fetchDouble2DParam(kParamBottomLeft)->getValueAtTime(args.time);
    glVertex2d(p.x, p.y);
    p = _effect->fetchDouble2DParam(kParamBottomRight)->getValueAtTime(args.time);
    glVertex2d(p.x, p.y);
    p = _effect->fetchDouble2DParam(kParamTopRight)->getValueAtTime(args.time);
    glVertex2d(p.x, p.y);
    p = _effect->fetchDouble2DParam(kParamTopLeft)->getValueAtTime(args.time);
    glVertex2d(p.x, p.y);
    glEnd();

    auto intersections = ((CornerPinPlugin*)_effect)->getIntersections();

    // std::cout << args.renderScale.x << " " << args.renderScale.y << std::endl;
    // std::cout << intersections->size() << std::endl;
    // std::cout.flush();

    glColor3f(1, 0.5, 0.5);

    for (auto iter = intersections.begin(); iter < intersections.end(); iter++) {
        glBegin(GL_LINE_LOOP);
        for (auto iter2 = iter->begin(); iter2 < iter->end(); iter2++) {
            // std::cout << "drawing " << iter2->x << "," << iter2->y << std::endl;
            // std::cout.flush();
            glVertex2d(iter2->x / args.renderScale.x, iter2->y / args.renderScale.y);
        }
        glEnd();
    }

    return true;
}
