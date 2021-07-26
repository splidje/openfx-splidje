#include "FaceTrackInteract.h"

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif


FaceTrackInteract::FaceTrackInteract(OfxInteractHandle handle, OFX::ImageEffect* effect)
    : OFX::OverlayInteract(handle)
{
    _faceTopLeft = effect->fetchDouble2DParam(kParamFaceTopLeft);
    addParamToSlaveTo(_faceTopLeft);
    _faceBottomRight = effect->fetchDouble2DParam(kParamFaceBottomRight);
    addParamToSlaveTo(_faceBottomRight);
    _eyebrowLeftLeft = effect->fetchDouble2DParam(kParamEyebrowLeftLeft);
    addParamToSlaveTo(_eyebrowLeftLeft);
    _eyebrowLeftRight = effect->fetchDouble2DParam(kParamEyebrowLeftRight);
    addParamToSlaveTo(_eyebrowLeftRight);
    _eyebrowRightLeft = effect->fetchDouble2DParam(kParamEyebrowRightLeft);
    addParamToSlaveTo(_eyebrowRightLeft);
    _eyebrowRightRight = effect->fetchDouble2DParam(kParamEyebrowRightRight);
    addParamToSlaveTo(_eyebrowRightRight);
}

void drawLine(Double2DParam* p1, Double2DParam* p2, double time) {
    auto p1Val = p1->getValueAtTime(time);
    auto p2Val = p2->getValueAtTime(time);
    glBegin(GL_LINE_LOOP);
    glVertex2f(p1Val.x, p1Val.y);
    glVertex2f(p2Val.x, p2Val.y);
    glEnd();
}

bool
FaceTrackInteract::draw(const DrawArgs &args)
{
    // shadow (uses GL_PROJECTION)
    glMatrixMode(GL_PROJECTION);
    glMatrixMode(GL_MODELVIEW); // Modelview should be used on Nuke

    auto topLeft = _faceTopLeft->getValueAtTime(args.time);
    auto bottomRight = _faceBottomRight->getValueAtTime(args.time);

    glColor3f(1, 0, 0);
    glBegin(GL_LINE_LOOP);
    glVertex2f(topLeft.x, topLeft.y);
    glVertex2f(bottomRight.x, topLeft.y);
    glVertex2f(bottomRight.x, bottomRight.y);
    glVertex2f(topLeft.x, bottomRight.y);
    glEnd();

    glColor3f(0, 1, 0);
    // left eyebrow
    drawLine(_eyebrowLeftLeft, _eyebrowLeftRight, args.time);
    // right eyebrow
    drawLine(_eyebrowRightLeft, _eyebrowRightRight, args.time);

    return true;
} // draw
