#include "FaceTrackPlugin.h"
#include "ofxsCoords.h"


FaceTrackPlugin::FaceTrackPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
{
    _detector = dlib::get_frontal_face_detector();
    auto landmarksPath =
        getPluginFilePath()
        + "/Contents/Resources/shape_predictor_68_face_landmarks.dat";
    dlib::deserialize(landmarksPath) >> _predictor;

    _srcClip = fetchClip(kSourceClip);
    assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentRGB ||
    	    _srcClip->getPixelComponents() == ePixelComponentRGBA));
    _track = fetchPushButtonParam(kParamTrack);
    _trackForward = fetchPushButtonParam(kParamTrackForward);
    _faceBottomLeft = fetchDouble2DParam(kParamFaceBottomLeft);
    _faceTopRight = fetchDouble2DParam(kParamFaceTopRight);
    // Jaw
    for (int i=0; i < kLandmarkCountJaw; i++) {
        _jaw[i] = fetchDouble2DParam(kParamJaw(i));
    }
    // Eyebrows
    for (int i=0; i < kLandmarkCountEyebrowRight; i++) {
        _eyebrowRight[i] = fetchDouble2DParam(kParamEyebrowRight(i));
    }
    for (int i=0; i < kLandmarkCountEyebrowLeft; i++) {
        _eyebrowLeft[i] = fetchDouble2DParam(kParamEyebrowLeft(i));
    }
    // Nose
    for (int i=0; i < kLandmarkCountNoseBridge; i++) {
        _noseBridge[i] = fetchDouble2DParam(kParamNoseBridge(i));
    }
    for (int i=0; i < kLandmarkCountNoseBottom; i++) {
        _noseBottom[i] = fetchDouble2DParam(kParamNoseBottom(i));
    }
    // Eyes
    for (int i=0; i < kLandmarkCountEyeRight; i++) {
        _eyeRight[i] = fetchDouble2DParam(kParamEyeRight(i));
    }
    for (int i=0; i < kLandmarkCountEyeLeft; i++) {
        _eyeLeft[i] = fetchDouble2DParam(kParamEyeLeft(i));
    }
    // Mouth
    for (int i=0; i < kLandmarkCountMouthOutside; i++) {
        _mouthOutside[i] = fetchDouble2DParam(kParamMouthOutside(i));
    }
    for (int i=0; i < kLandmarkCountMouthInside; i++) {
        _mouthInside[i] = fetchDouble2DParam(kParamMouthInside(i));
    }
}

bool FaceTrackPlugin::isIdentity(const IsIdentityArguments &args, 
                                  Clip * &identityClip, double &/*identityTime*/
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
)
{
    identityClip = _srcClip;
    return true;
}

void FaceTrackPlugin::render(const OFX::RenderArguments &args) {
}

u_char float_to_u_char_clamped(float v) {
    return std::max(0.0f, std::min(1.0f, v)) * UCHAR_MAX;
}

void setPointParam(Double2DParam* param, dlib::point val, OfxRectI bounds, double time) {
    param->setValueAtTime(time, bounds.x1 + val.x(), bounds.y2 - val.y());
}

void FaceTrackPlugin::changedParam(const InstanceChangedArgs &args, const std::string &paramName) {
    if (paramName == kParamTrack) {
        trackTime(args.time);
    } else if (paramName == kParamTrackForward) {
        OfxRangeD timeline;
        this->timeLineGetBounds(timeline.min, timeline.max);
        for (auto t=args.time; t < timeline.max; t++) {
            trackTime(t);
        }
    }
}

void FaceTrackPlugin::trackTime(double t) {
    auto_ptr<Image> srcImg(_srcClip->fetchImage(t));
    auto srcBounds = srcImg->getBounds();
    auto srcComponentCount = srcImg->getPixelComponentCount();
    dlib::array2d<dlib::rgb_pixel> img;
    img.set_size(srcBounds.y2 - srcBounds.y1, srcBounds.x2 - srcBounds.x1);
    auto imgPix = img.begin();
    for (auto y = srcBounds.y2 - 1; y >= srcBounds.y1; y--) {
        auto srcPix = (float*)srcImg->getPixelAddress(srcBounds.x1, y);
        for (auto x = srcBounds.x1; x < srcBounds.x2; x++, imgPix++, srcPix += srcComponentCount) {
            imgPix->red = float_to_u_char_clamped(srcPix[0]);
            imgPix->green = float_to_u_char_clamped(srcPix[1]);
            imgPix->blue = float_to_u_char_clamped(srcPix[2]);
        }
    }

    // detect faces
    // dlib::pyramid_up(img);        
    auto faces = _detector(img);
    if (faces.size() == 0) {
        std::cout << "No faces detected " << t << std::endl;
        return;
    }
    auto face = faces[0];
    _faceBottomLeft->setValueAtTime(
        t, face.left() + srcBounds.x1, srcBounds.y2 - face.bottom()
    );
    _faceTopRight->setValueAtTime(
        t, face.right() + srcBounds.x1, srcBounds.y2 - face.top()
    );

    // predict shape
    auto shape = _predictor(img, face);

    if (shape.num_parts() != 68) {
        std::cout << "should be 68 landmarks, instead it's: " << shape.num_parts() << std::endl;
        return;
    }

    // Jaw
    for (int i=kLandmarkIndexJawStart; i <= kLandmarkIndexJawEnd; i++) {
        setPointParam(
            _jaw[i - kLandmarkIndexJawStart], shape.part(i), srcBounds, t
        );
    }
    // Eyebrows
    for (int i=kLandmarkIndexEyebrowRightStart; i <= kLandmarkIndexEyebrowRightEnd; i++) {
        setPointParam(
            _eyebrowRight[i - kLandmarkIndexEyebrowRightStart], shape.part(i), srcBounds, t
        );
    }
    for (int i=kLandmarkIndexEyebrowLeftStart; i <= kLandmarkIndexEyebrowLeftEnd; i++) {
        setPointParam(
            _eyebrowLeft[i - kLandmarkIndexEyebrowLeftStart], shape.part(i), srcBounds, t
        );
    }
    // Nose
    for (int i=kLandmarkIndexNoseBridgeStart; i <= kLandmarkIndexNoseBridgeEnd; i++) {
        setPointParam(
            _noseBridge[i - kLandmarkIndexNoseBridgeStart], shape.part(i), srcBounds, t
        );
    }
    for (int i=kLandmarkIndexNoseBottomStart; i <= kLandmarkIndexNoseBottomEnd; i++) {
        setPointParam(
            _noseBottom[i - kLandmarkIndexNoseBottomStart], shape.part(i), srcBounds, t
        );
    }
    // Eyes
    for (int i=kLandmarkIndexEyeRightStart; i <= kLandmarkIndexEyeRightEnd; i++) {
        setPointParam(
            _eyeRight[i - kLandmarkIndexEyeRightStart], shape.part(i), srcBounds, t
        );
    }
    for (int i=kLandmarkIndexEyeLeftStart; i <= kLandmarkIndexEyeLeftEnd; i++) {
        setPointParam(
            _eyeLeft[i - kLandmarkIndexEyeLeftStart], shape.part(i), srcBounds, t
        );
    }
    // Mouth
    for (int i=kLandmarkIndexMouthOutsideStart; i <= kLandmarkIndexMouthOutsideEnd; i++) {
        setPointParam(
            _mouthOutside[i - kLandmarkIndexMouthOutsideStart], shape.part(i), srcBounds, t
        );
    }
    for (int i=kLandmarkIndexMouthInsideStart; i <= kLandmarkIndexMouthInsideEnd; i++) {
        setPointParam(
            _mouthInside[i - kLandmarkIndexMouthInsideStart], shape.part(i), srcBounds, t
        );
    }
}
