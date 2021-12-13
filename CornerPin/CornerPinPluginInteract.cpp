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

    glColor3f(1, 0.5, 0.5);

    for (auto iter = intersections.begin(); iter < intersections.end(); iter++) {
        glBegin(GL_LINE_LOOP);
        for (auto iter2 = iter->begin(); iter2 < iter->end(); iter2++) {
            glVertex2d(iter2->x, iter2->y);
        }
        glEnd();
    }

    return true;
}

inline bool _inVicinity(const OfxPointD* p, const OfxPointD* h, const OfxPointD* radius) {
    return (
        p->x >= h->x - radius->x && p->x <= h->x + radius->x
        && p->y >= h->y - radius->y && p->y <= h->y + radius->y
    );
}

bool CornerPinPluginInteract::penDown(const PenArgs &args) {
    OfxPointD radius;
    radius.x = 10 * args.pixelScale.x;
    radius.y = 10 * args.pixelScale.y;
    auto bottomLeft = _effect->fetchDouble2DParam(kParamBottomLeft)->getValueAtTime(args.time);
    if (_inVicinity(&args.penPosition, &bottomLeft, &radius)) {
        movingHandle = 1;
        return true;
    }
    auto bottomRight = _effect->fetchDouble2DParam(kParamBottomRight)->getValueAtTime(args.time);
    if (_inVicinity(&args.penPosition, &bottomRight, &radius)) {
        movingHandle = 2;
        return true;
    }
    auto topRight = _effect->fetchDouble2DParam(kParamTopRight)->getValueAtTime(args.time);
    if (_inVicinity(&args.penPosition, &topRight, &radius)) {
        movingHandle = 3;
        return true;
    }
    auto topLeft = _effect->fetchDouble2DParam(kParamTopLeft)->getValueAtTime(args.time);
    if (_inVicinity(&args.penPosition, &topLeft, &radius)) {
        movingHandle = 4;
        return true;
    }
    movingHandle = 0;
    return false;
}

bool CornerPinPluginInteract::penUp(const PenArgs &args) {
    if (movingHandle) {
        movingHandle = 0;
        return true;
    }
    return false;
}

bool CornerPinPluginInteract::penMotion(const PenArgs &args) {
    const char* paramName;
    switch (movingHandle) {
        case 1: paramName = kParamBottomLeft; break;
        case 2: paramName = kParamBottomRight; break;
        case 3: paramName = kParamTopRight; break;
        case 4: paramName = kParamTopLeft; break;
        default: return false;
    }
    _effect->fetchDouble2DParam(paramName)->setValueAtTime(args.time, args.penPosition);
    return true;
}
