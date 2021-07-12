#include "BoneWarpInteract.h"
#include <cmath>

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#define SEGMENT_ANGLE (2 * M_PI / 36)


BoneWarpInteract::BoneWarpInteract(OfxInteractHandle handle, OFX::ImageEffect* effect)
    : OFX::OverlayInteract(handle)
{
    for (int i=0; i < 2; i++) {
        _jointFromCentres[i] = effect->fetchDouble2DParam(kParamJointFromCentre(i));
        addParamToSlaveTo(_jointFromCentres[i]);
        _jointToCentres[i] = effect->fetchDouble2DParam(kParamJointToCentre(i));
        addParamToSlaveTo(_jointToCentres[i]);
        _jointRadii[i] = effect->fetchDoubleParam(kParamJointRadius(i));
        addParamToSlaveTo(_jointRadii[i]);
    }
}

bool
BoneWarpInteract::draw(const DrawArgs &args)
{
    // shadow (uses GL_PROJECTION)
    glMatrixMode(GL_PROJECTION);
    glMatrixMode(GL_MODELVIEW); // Modelview should be used on Nuke

    glEnable(GL_LINE_STIPPLE);

    OfxPointD fromCentres[2];
    OfxPointD toCentres[2];
    double radii[2];

    for (int i=0; i < 2; i++) {
        fromCentres[i] = _jointFromCentres[i]->getValueAtTime(args.time);
        toCentres[i] = _jointToCentres[i]->getValueAtTime(args.time);
        radii[i] = _jointRadii[i]->getValueAtTime(args.time);
    }

    glLineStipple(1, 0xFFFF);

    glColor3f(1, 1, 1);
    glBegin(GL_LINES);
    for (int i=0; i < 2; i++) {
        auto fromCentre = fromCentres[i];
        glVertex2f(fromCentre.x, fromCentre.y);
    }
    glEnd();

    for (int i=0; i < 2; i++) {
        auto radius = radii[i];
        auto fromCentre = fromCentres[i];
        if (i) {glColor3f(1, 0, 0);}
        else {glColor3f(0, 1, 0);}
        glBegin(GL_LINE_LOOP);
        for (double theta=0; theta < 2 * M_PI; theta += SEGMENT_ANGLE) {
            glVertex2f(fromCentre.x + radius * sin(theta), fromCentre.y + radius * cos(theta));
        }
        glEnd();
    }

    glLineStipple(1, 0x3333);

    glColor3f(1, 1, 1);
    glBegin(GL_LINES);
    for (int i=0; i < 2; i++) {
        auto toCentre = toCentres[i];
        glVertex2f(toCentre.x, toCentre.y);
    }
    glEnd();

    for (int i=0; i < 2; i++) {
        auto radius = radii[i];
        auto toCentre = toCentres[i];
        if (i) {glColor3f(1, 0, 0);}
        else {glColor3f(0, 1, 0);}
        glBegin(GL_LINE_LOOP);
        for (double theta=0; theta < 2 * M_PI; theta += SEGMENT_ANGLE) {
            glVertex2f(toCentre.x + radius * sin(theta), toCentre.y + radius * cos(theta));
        }
        glEnd();
    }

    glColor3f(0, 1, 0);
    auto fromToX = toCentres[0].x - fromCentres[0].x;
    auto fromToY = toCentres[0].y - fromCentres[0].y;
    auto d = sqrt(fromToX * fromToX + fromToY * fromToY);
    if (d) {
        auto normTangX = -fromToY / d;
        auto normTangY = fromToX / d;

        glBegin(GL_LINES);
        glLineStipple(1, 0xFFFF);
        glVertex2f(fromCentres[0].x + normTangX * radii[0], fromCentres[0].y + normTangY * radii[0]);
        glLineStipple(1, 0x3333);
        glVertex2f(toCentres[0].x + normTangX * radii[0], toCentres[0].y + normTangY * radii[0]);
        glEnd();

        glBegin(GL_LINES);
        glLineStipple(1, 0xFFFF);
        glVertex2f(fromCentres[0].x - normTangX * radii[0], fromCentres[0].y - normTangY * radii[0]);
        glLineStipple(1, 0x3333);
        glVertex2f(toCentres[0].x - normTangX * radii[0], toCentres[0].y - normTangY * radii[0]);
        glEnd();
    }    

    return true;
} // draw
