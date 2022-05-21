#include "FaceTrackPluginBase.h"
#include "ofxsCoords.h"

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif


FaceTrackPluginBase::FaceTrackPluginBase(OfxImageEffectHandle handle)
    : ImageEffect(handle)
{
    _detector = dlib::get_frontal_face_detector();
    auto landmarksPath =
        getPluginFilePath()
        + "/Contents/Resources/shape_predictor_68_face_landmarks.dat";
    dlib::deserialize(landmarksPath) >> _predictor;
}

std::string FaceTrackPluginBase::landmarkIndexToParamName(int index) {
    if (index >= kLandmarkIndexJawStart && index <= kLandmarkIndexJawEnd) {
        return "jaw" + std::to_string(index - kLandmarkIndexJawStart);
    } else if (index >= kLandmarkIndexEyebrowRightStart && index <= kLandmarkIndexEyebrowRightEnd) {
        return "eyebrowRight" + std::to_string(index - kLandmarkIndexEyebrowRightStart);
    } else if (index >= kLandmarkIndexEyebrowLeftStart && index <= kLandmarkIndexEyebrowLeftEnd) {
        return "eyebrowLeft" + std::to_string(index - kLandmarkIndexEyebrowLeftStart);
    } else if (index >= kLandmarkIndexNoseBridgeStart && index <= kLandmarkIndexNoseBridgeEnd) {
        return "noseBridge" + std::to_string(index - kLandmarkIndexNoseBridgeStart);
    } else if (index >= kLandmarkIndexNoseBottomStart && index <= kLandmarkIndexNoseBottomEnd) {
        return "noseBottom" + std::to_string(index - kLandmarkIndexNoseBottomStart);
    } else if (index >= kLandmarkIndexEyeRightStart && index <= kLandmarkIndexEyeRightEnd) {
        return "eyeRight" + std::to_string(index - kLandmarkIndexEyeRightStart);
    } else if (index >= kLandmarkIndexEyeLeftStart && index <= kLandmarkIndexEyeLeftEnd) {
        return "eyeLeft" + std::to_string(index - kLandmarkIndexEyeLeftStart);
    } else if (index >= kLandmarkIndexMouthOutsideStart && index <= kLandmarkIndexMouthOutsideEnd) {
        return "mouthOutside" + std::to_string(index - kLandmarkIndexMouthOutsideStart);
    } else if (index >= kLandmarkIndexMouthInsideStart && index <= kLandmarkIndexMouthInsideEnd) {
        return "mouthInside" + std::to_string(index - kLandmarkIndexMouthInsideStart);
    }
    return "";
}

void FaceTrackPluginBase::defineFaceParams(ImageEffectDescriptor* desc, PageParamDescriptor* page, std::string prefix) {
    // create groups
    auto mainGroup = desc->defineGroupParam(prefix + "face");
    if (page) {
        page->addChild(*mainGroup);
    }
    auto jawGroup = desc->defineGroupParam(prefix + "jaw");
    jawGroup->setParent(*mainGroup);
    auto eyebrowsGroup = desc->defineGroupParam(prefix + "eyebrows");
    eyebrowsGroup->setParent(*mainGroup);
    auto eyebrowRightGroup = desc->defineGroupParam(prefix + "eyebrowRight");
    eyebrowRightGroup->setParent(*eyebrowsGroup);
    auto eyebrowLeftGroup = desc->defineGroupParam(prefix + "eyebrowLeft");
    eyebrowLeftGroup->setParent(*eyebrowsGroup);
    auto noseGroup = desc->defineGroupParam(prefix + "nose");
    noseGroup->setParent(*mainGroup);
    auto noseBridgeGroup = desc->defineGroupParam(prefix + "noseBridge");
    noseBridgeGroup->setParent(*noseGroup);
    auto noseBottomGroup = desc->defineGroupParam(prefix + "noseBottom");
    noseBottomGroup->setParent(*noseGroup);
    auto eyesGroup = desc->defineGroupParam(prefix + "eyes");
    eyesGroup->setParent(*mainGroup);
    auto eyeRightGroup = desc->defineGroupParam(prefix + "eyeRight");
    eyeRightGroup->setParent(*eyesGroup);
    auto eyeLeftGroup = desc->defineGroupParam(prefix + "eyeLeft");
    eyeLeftGroup->setParent(*eyesGroup);
    auto mouthGroup = desc->defineGroupParam(prefix + "mouth");
    mouthGroup->setParent(*mainGroup);
    auto mouthOutsideGroup = desc->defineGroupParam(prefix + "mouthOutside");
    mouthOutsideGroup->setParent(*mouthGroup);
    auto mouthInsideGroup = desc->defineGroupParam(prefix + "mouthInside");
    mouthInsideGroup->setParent(*mouthGroup);

    // create params
    {
        auto param = desc->defineDouble2DParam(prefix + "faceBottomLeft");
        param->setParent(*mainGroup);
    }
    {
        auto param = desc->defineDouble2DParam(prefix + "faceTopRight");
        param->setParent(*mainGroup);
    }
    for (int i=0; i < kLandmarkCount; i++) {
        auto name = landmarkIndexToParamName(i);
        auto param = desc->defineDouble2DParam(prefix + name);
        if (name.rfind("jaw") == 0) {
            param->setParent(*jawGroup);
        }
        else if (name.rfind("eyebrowRight") == 0) {
            param->setParent(*eyebrowRightGroup);
        }
        else if (name.rfind("eyebrowLeft") == 0) {
            param->setParent(*eyebrowLeftGroup);
        }
        else if (name.rfind("noseBridge") == 0) {
            param->setParent(*noseBridgeGroup);
        }
        else if (name.rfind("noseBottom") == 0) {
            param->setParent(*noseBottomGroup);
        }
        else if (name.rfind("eyeRight") == 0) {
            param->setParent(*eyeRightGroup);
        }
        else if (name.rfind("eyeLeft") == 0) {
            param->setParent(*eyeLeftGroup);
        }
        else if (name.rfind("mouthOutside") == 0) {
            param->setParent(*mouthOutsideGroup);
        }
        else if (name.rfind("mouthInside") == 0) {
            param->setParent(*mouthInsideGroup);
        }
    }
}

void FaceTrackPluginBase::fetchFaceParams(FaceParams* faceParams, std::string prefix) {
    faceParams->bottomLeft = fetchDouble2DParam(prefix + "faceBottomLeft");
    faceParams->topRight = fetchDouble2DParam(prefix + "faceTopRight");
    for (int i=0; i < kLandmarkCount; i++) {
        std::string name = landmarkIndexToParamName(i);
        faceParams->landmarks[i] = fetchDouble2DParam(prefix + name);
    }
}

void FaceTrackPluginBase::addFaceParamsToSlaveTo(OverlayInteract* interact, FaceParams* faceParams) {
    interact->addParamToSlaveTo(faceParams->bottomLeft);
    interact->addParamToSlaveTo(faceParams->topRight);
    for (int i=0; i < kLandmarkCount; i++) {
        interact->addParamToSlaveTo(faceParams->landmarks[i]);
    }
}

u_char float_to_u_char_clamped(float v) {
    return std::max(0.0f, std::min(1.0f, v)) * UCHAR_MAX;
}

void setPointParam(Double2DParam* param, dlib::point val, OfxRectI bounds, double time) {
    auto x = bounds.x1 + val.x();
    auto y = bounds.y2 - val.y();
    param->setValueAtTime(time, x, y);
}

void FaceTrackPluginBase::trackClipAtTime(Clip* clip, FaceParams* faceParams, double t) {
    auto_ptr<Image> img(clip->fetchImage(t));
    auto bounds = img->getBounds();
    auto componentCount = img->getPixelComponentCount();
    dlib::array2d<dlib::rgb_pixel> dlibImg;
    dlibImg.set_size(bounds.y2 - bounds.y1, bounds.x2 - bounds.x1);
    auto dlibImgPix = dlibImg.begin();
    for (auto y = bounds.y2 - 1; y >= bounds.y1; y--) {
        auto imgPix = (float*)img->getPixelAddress(bounds.x1, y);
        for (auto x = bounds.x1; x < bounds.x2; x++, dlibImgPix++, imgPix += componentCount) {
            dlibImgPix->red = float_to_u_char_clamped(imgPix[0]);
            dlibImgPix->green = float_to_u_char_clamped(imgPix[1]);
            dlibImgPix->blue = float_to_u_char_clamped(imgPix[2]);
        }
    }

    // detect faces
    // dlib::pyramid_up(img);        
    auto faces = _detector(dlibImg);
    if (faces.size() == 0) {
        return;
    }
    auto face = faces[0];
    faceParams->bottomLeft->setValueAtTime(
        t, face.left() + bounds.x1, bounds.y2 - face.bottom()
    );
    faceParams->topRight->setValueAtTime(
        t, face.right() + bounds.x1, bounds.y2 - face.top()
    );

    // predict shape
    auto shape = _predictor(dlibImg, face);

    if (shape.num_parts() != kLandmarkCount) {
        std::cerr << "should be " << kLandmarkCount << " landmarks, instead it's: " << shape.num_parts() << std::endl;
        return;
    }

    for (int i=0; i < kLandmarkCount; i++) {
        setPointParam(faceParams->landmarks[i], shape.part(i), bounds, t);
    }
}

void FaceTrackPluginBase::improveFaceByReferenceAtTime(Clip* clip, FaceParams* faceParams, double t, double refT) {
    
}

inline void addVertex(Double2DParam* p1, double t) {
    auto p1Val = p1->getValueAtTime(t);
    glVertex2f(p1Val.x, p1Val.y);
}

inline void drawLines(FaceTrackPluginBase::FaceParams* faceParams, int indexFirst, int indexLast, double t) {
    glBegin(GL_LINES);
    for (int i=indexFirst; i <= indexLast; i++) {
        addVertex(faceParams->landmarks[i], t);
        if (i > indexFirst && i < indexLast) {
            addVertex(faceParams->landmarks[i], t);
        }
    }
    glEnd();
}

inline void drawLineLoop(FaceTrackPluginBase::FaceParams* faceParams, int indexFirst, int indexLast, double t) {
    glBegin(GL_LINE_LOOP);
    for (int i=indexFirst; i <= indexLast; i++) {
        addVertex(faceParams->landmarks[i], t);
    }
    glEnd();
}

void FaceTrackPluginBase::drawFace(FaceParams* faceParams, double t) {
    auto bottomLeft = faceParams->bottomLeft->getValueAtTime(t);
    auto topRight = faceParams->topRight->getValueAtTime(t);

    glColor3f(1, 0, 0);
    glBegin(GL_LINE_LOOP);
    glVertex2f(bottomLeft.x, bottomLeft.y);
    glVertex2f(topRight.x, bottomLeft.y);
    glVertex2f(topRight.x, topRight.y);
    glVertex2f(bottomLeft.x, topRight.y);
    glEnd();

    // Jaw
    glColor3f(0, 0, 1);
    drawLines(faceParams, kLandmarkIndexJawStart, kLandmarkIndexJawEnd, t);

    // Eyebrows
    glColor3f(0, 1, 0);
    // Right Eyebrow
    drawLines(faceParams, kLandmarkIndexEyebrowRightStart, kLandmarkIndexEyebrowRightEnd, t);
    // Left Eyebrow
    drawLines(faceParams, kLandmarkIndexEyebrowLeftStart, kLandmarkIndexEyebrowLeftEnd, t);

    // Nose
    glColor3f(1, 1, 0);
    // Nose Bridge
    drawLines(faceParams, kLandmarkIndexNoseBridgeStart, kLandmarkIndexNoseBridgeEnd, t);
    // Nose Tip
    drawLines(faceParams, kLandmarkIndexNoseBottomStart, kLandmarkIndexNoseBottomEnd, t);

    // Eyes
    glColor3f(1, 0, 1);
    // Right Eye
    drawLineLoop(faceParams, kLandmarkIndexEyeRightStart, kLandmarkIndexEyeRightEnd, t);
    // Left Eye
    drawLineLoop(faceParams, kLandmarkIndexEyeLeftStart, kLandmarkIndexEyeLeftEnd, t);

    // Mouth
    glColor3f(0, 1, 1);
    // Mouth Outisde
    drawLineLoop(faceParams, kLandmarkIndexMouthOutsideStart, kLandmarkIndexMouthOutsideEnd, t);
    // Mouth Inside
    drawLineLoop(faceParams, kLandmarkIndexMouthInsideStart, kLandmarkIndexMouthInsideEnd, t);
}
