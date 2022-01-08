#include "QuadrangleDistortPluginInteract.h"
#include "QuadrangleDistortPluginMacros.h"
#include "QuadrangleDistortPlugin.h"
#include "QuadrangleDistort.h"

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

using namespace QuadrangleDistort;


QuadrangleDistortPluginInteract::QuadrangleDistortPluginInteract(OfxInteractHandle handle, ImageEffect* effect)
    : OverlayInteract(handle) {
        addParamToSlaveTo(effect->fetchDouble2DParam(kParamBottomLeft));
        addParamToSlaveTo(effect->fetchDouble2DParam(kParamBottomRight));
        addParamToSlaveTo(effect->fetchDouble2DParam(kParamTopRight));
        addParamToSlaveTo(effect->fetchDouble2DParam(kParamTopLeft));
}

bool QuadrangleDistortPluginInteract::draw(const DrawArgs &args) {
    glColor3f(1, 1, 1);

    OfxPointD p;
    glBegin(GL_LINE_LOOP);
    auto bottomLeft = _effect->fetchDouble2DParam(kParamBottomLeft)->getValueAtTime(args.time);
    glVertex2d(bottomLeft.x, bottomLeft.y);
    auto bottomRight = _effect->fetchDouble2DParam(kParamBottomRight)->getValueAtTime(args.time);
    glVertex2d(bottomRight.x, bottomRight.y);
    auto topRight = _effect->fetchDouble2DParam(kParamTopRight)->getValueAtTime(args.time);
    glVertex2d(topRight.x, topRight.y);
    auto topLeft = _effect->fetchDouble2DParam(kParamTopLeft)->getValueAtTime(args.time);
    glVertex2d(topLeft.x, topLeft.y);
    glEnd();

    Quadrangle quad;
    quad.edges[0].p = bottomLeft;
    quad.edges[1].p = bottomRight;
    quad.edges[2].p = topRight;
    quad.edges[3].p = topLeft;
    quad.initialise();

    auto srcClip = _effect->fetchClip(kSourceClip);
    auto srcROD = srcClip->getRegionOfDefinition(args.time);

    Quadrangle window;
    window.edges[0].p.x = srcROD.x1;
    window.edges[0].p.y = srcROD.y1;
    window.edges[1].p.x = srcROD.x2;
    window.edges[1].p.y = srcROD.y1;
    window.edges[2].p.x = srcROD.x2;
    window.edges[2].p.y = srcROD.y2;
    window.edges[3].p.x = srcROD.x1;
    window.edges[3].p.y = srcROD.y2;
    window.initialise();

    glColor3f(0.5, 0.5, 1);
    glLineStipple(1, 0xAAAA);
    glEnable(GL_LINE_STIPPLE);
    for (int i=0; i < 4; i++) {
        if (!quad.edges[i].isInitialised) {
            continue;
        }
        OfxPointD ends[2];
        int count = 0;
        for (int j=0; j < 4; j++) {
            auto crosses = window.edges[j].crosses(&quad.edges[i]);
            if (crosses < 0 || crosses > 1) {
                continue;
            }
            ends[count].x = window.edges[j].p.x + window.edges[j].vect.x * crosses;
            ends[count].y = window.edges[j].p.y + window.edges[j].vect.y * crosses;
            count++;
            if (count == 2) {
                break;
            }
        }
        if (count == 2) {
            glBegin(GL_LINES);
            glVertex2d(ends[0].x, ends[0].y);
            glVertex2d(ends[1].x, ends[1].y);
            glEnd();
        }
    }
    glDisable(GL_LINE_STIPPLE);

    auto intersections = ((QuadrangleDistortPlugin*)_effect)->getIntersections();

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

bool QuadrangleDistortPluginInteract::penDown(const PenArgs &args) {
    OfxPointD radius;
    radius.x =  10 * args.pixelScale.x;
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

bool QuadrangleDistortPluginInteract::penUp(const PenArgs &args) {
    if (movingHandle) {
        movingHandle = 0;
        return true;
    }
    return false;
}

bool QuadrangleDistortPluginInteract::penMotion(const PenArgs &args) {
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
