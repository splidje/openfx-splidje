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
    _faceTopLeft = fetchDouble2DParam(kParamFaceTopLeft);
    _faceBottomRight = fetchDouble2DParam(kParamFaceBottomRight);
    _eyebrowLeftLeft = fetchDouble2DParam(kParamEyebrowLeftLeft);
    _eyebrowLeftRight = fetchDouble2DParam(kParamEyebrowLeftRight);
    _eyebrowRightLeft = fetchDouble2DParam(kParamEyebrowRightLeft);
    _eyebrowRightRight = fetchDouble2DParam(kParamEyebrowRightRight);
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
        auto_ptr<Image> srcImg(_srcClip->fetchImage(args.time));
        auto srcBounds = srcImg->getBounds();
        auto srcComponentCount = srcImg->getPixelComponentCount();
        // std::cout << b.x1 << " " << b.x2 << " " << b.y1 << " " << b.y2 << std::endl;
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
            std::cout << "No faces detected " << args.time << std::endl;
            return;
        }
        auto face = faces[0];
        _faceTopLeft->setValueAtTime(
            args.time, face.left() + srcBounds.x1, srcBounds.y2 - face.top()
        );
        _faceBottomRight->setValueAtTime(
            args.time, face.right() + srcBounds.x1, srcBounds.y2 - face.bottom()
        );

        // predict shape
        auto shape = _predictor(img, face);

        if (shape.num_parts() != 68) {
            std::cout << "should be 68 landmarks, instead it's: " << shape.num_parts() << std::endl;
            return;
        }

        setPointParam(_eyebrowLeftLeft, shape.part(kLandmarkIndexEyebrowLeftLeft), srcBounds, args.time);
        setPointParam(_eyebrowLeftRight, shape.part(kLandmarkIndexEyebrowLeftRight), srcBounds, args.time);
        setPointParam(_eyebrowRightLeft, shape.part(kLandmarkIndexEyebrowRightLeft), srcBounds, args.time);
        setPointParam(_eyebrowRightRight, shape.part(kLandmarkIndexEyebrowRightRight), srcBounds, args.time);
    }
}
