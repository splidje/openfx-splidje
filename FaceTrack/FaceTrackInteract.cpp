#include "FaceTrackInteract.h"
#include <iostream>

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
    _faceBottomLeft = effect->fetchDouble2DParam(kParamFaceBottomLeft);
    addParamToSlaveTo(_faceBottomLeft);
    _faceTopRight = effect->fetchDouble2DParam(kParamFaceTopRight);
    addParamToSlaveTo(_faceTopRight);
    // Jaw
    for (int i=0; i < kLandmarkCountJaw; i++) {
        _jaw[i] = effect->fetchDouble2DParam(kParamJaw(i));
        addParamToSlaveTo(_jaw[i]);
    }
    // Eyebrows
    for (int i=0; i < kLandmarkCountEyebrowRight; i++) {
        _eyebrowRight[i] = effect->fetchDouble2DParam(kParamEyebrowRight(i));
        addParamToSlaveTo(_eyebrowRight[i]);
    }
    for (int i=0; i < kLandmarkCountEyebrowLeft; i++) {
        _eyebrowLeft[i] = effect->fetchDouble2DParam(kParamEyebrowLeft(i));
        addParamToSlaveTo(_eyebrowLeft[i]);
    }
    // Nose
    for (int i=0; i < kLandmarkCountNoseBridge; i++) {
        _noseBridge[i] = effect->fetchDouble2DParam(kParamNoseBridge(i));
        addParamToSlaveTo(_noseBridge[i]);
    }
    for (int i=0; i < kLandmarkCountNoseBottom; i++) {
        _noseBottom[i] = effect->fetchDouble2DParam(kParamNoseBottom(i));
        addParamToSlaveTo(_noseBottom[i]);
    }
    // Eyes
    for (int i=0; i < kLandmarkCountEyeRight; i++) {
        _eyeRight[i] = effect->fetchDouble2DParam(kParamEyeRight(i));
        addParamToSlaveTo(_eyeRight[i]);
    }
    for (int i=0; i < kLandmarkCountEyeLeft; i++) {
        _eyeLeft[i] = effect->fetchDouble2DParam(kParamEyeLeft(i));
        addParamToSlaveTo(_eyeLeft[i]);
    }
    // Mouth
    for (int i=0; i < kLandmarkCountMouthOutside; i++) {
        _mouthOutside[i] = effect->fetchDouble2DParam(kParamMouthOutside(i));
        addParamToSlaveTo(_mouthOutside[i]);
    }
    for (int i=0; i < kLandmarkCountMouthInside; i++) {
        _mouthInside[i] = effect->fetchDouble2DParam(kParamMouthInside(i));
        addParamToSlaveTo(_mouthInside[i]);
    }
}

void addVertex(Double2DParam* p1, double time) {
    auto p1Val = p1->getValueAtTime(time);
    glVertex2f(p1Val.x, p1Val.y);
}

bool
FaceTrackInteract::draw(const DrawArgs &args)
{
    // shadow (uses GL_PROJECTION)
    glMatrixMode(GL_PROJECTION);
    glMatrixMode(GL_MODELVIEW); // Modelview should be used on Nuke

    auto bottomLeft = _faceBottomLeft->getValueAtTime(args.time);
    auto topRight = _faceTopRight->getValueAtTime(args.time);

    glColor3f(1, 0, 0);
    glBegin(GL_LINE_LOOP);
    glVertex2f(bottomLeft.x, bottomLeft.y);
    glVertex2f(topRight.x, bottomLeft.y);
    glVertex2f(topRight.x, topRight.y);
    glVertex2f(bottomLeft.x, topRight.y);
    glEnd();

    // Jaw
    glColor3f(0, 0, 1);

    glBegin(GL_LINES);
    for (int i=0; i < kLandmarkCountJaw; i++) {
        addVertex(_jaw[i], args.time);
        if (i > 0 && i < kLandmarkCountJaw - 1) {
            addVertex(_jaw[i], args.time);
        }
    }
    glEnd();

    // Eyebrows
    glColor3f(0, 1, 0);

    // Right Eyebrow
    glBegin(GL_LINES);
    for (int i=0; i < kLandmarkCountEyebrowRight; i++) {
        addVertex(_eyebrowRight[i], args.time);
        if (i > 0 && i < kLandmarkCountEyebrowRight - 1) {
            addVertex(_eyebrowRight[i], args.time);
        }
    }
    glEnd();

    // Left Eyebrow
    glBegin(GL_LINES);
    for (int i=0; i < kLandmarkCountEyebrowLeft; i++) {
        addVertex(_eyebrowLeft[i], args.time);
        if (i > 0 && i < kLandmarkCountEyebrowLeft - 1) {
            addVertex(_eyebrowLeft[i], args.time);
        }
    }
    glEnd();

    // Nose
    glColor3f(1, 1, 0);

    // Nose Bridge
    glBegin(GL_LINES);
    for (int i=0; i < kLandmarkCountNoseBridge; i++) {
        addVertex(_noseBridge[i], args.time);
        if (i > 0 && i < kLandmarkCountNoseBridge - 1) {
            addVertex(_noseBridge[i], args.time);
        }
    }
    glEnd();

    // Nose Tip
    glBegin(GL_LINES);
    for (int i=0; i < kLandmarkCountNoseBottom; i++) {
        addVertex(_noseBottom[i], args.time);
        if (i > 0 && i < kLandmarkCountNoseBottom - 1) {
            addVertex(_noseBottom[i], args.time);
        }
    }
    glEnd();

    // Eyes
    glColor3f(1, 0, 1);

    // Right Eye
    glBegin(GL_LINE_LOOP);
    for (int i=0; i < kLandmarkCountEyeRight; i++) {
        addVertex(_eyeRight[i], args.time);
    }
    glEnd();

    // Left Eye
    glBegin(GL_LINE_LOOP);
    for (int i=0; i < kLandmarkCountEyeLeft; i++) {
        addVertex(_eyeLeft[i], args.time);
    }
    glEnd();

    // Mouth
    glColor3f(0, 1, 1);

    // Mouth Outisde
    glBegin(GL_LINE_LOOP);
    for (int i=0; i < kLandmarkCountMouthOutside; i++) {
        addVertex(_mouthOutside[i], args.time);
    }
    glEnd();

    // Mouth Inside
    glBegin(GL_LINE_LOOP);
    for (int i=0; i < kLandmarkCountMouthInside; i++) {
        addVertex(_mouthInside[i], args.time);
    }
    glEnd();

    return true;
} // draw
