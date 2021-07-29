#define kPluginName "FaceTrack"
#define kPluginGrouping "Transform"
#define kPluginDescription \
"Face tracking"

#define kPluginIdentifier "com.ajptechnical.openfx.FaceTrack"
#define kPluginVersionMajor 0
#define kPluginVersionMinor 1

#define kSourceClip "Source"

#define kParamTrack "track"
#define kParamTrackLabel "Track"
#define kParamTrackHint "Track"

#define kParamTrackForward "trackForward"
#define kParamTrackForwardLabel "Track Forward"
#define kParamTrackForwardHint "Track Forward"

#define kParamFaceBottomLeft "faceBottomLeft"
#define kParamFaceBottomLeftLabel "Face Bottom Left"
#define kParamFaceBottomLeftHint "Face Bottom Left"

#define kParamFaceTopRight "faceTopRight"
#define kParamFaceTopRightLabel "Face Top Right"
#define kParamFaceTopRightHint "Face Top Right"

#define kLandmarkIndexJawStart 0
#define kLandmarkIndexJawEnd 16
#define kLandmarkCountJaw (kLandmarkIndexJawEnd - kLandmarkIndexJawStart + 1)

#define kParamJaw(i) "jaw" + std::to_string(i)
#define kParamJaw(i) "jaw" + std::to_string(i)

#define kLandmarkIndexEyebrowRightStart 17
#define kLandmarkIndexEyebrowRightEnd 21
#define kLandmarkCountEyebrowRight (kLandmarkIndexEyebrowRightEnd - kLandmarkIndexEyebrowRightStart + 1)

#define kLandmarkIndexEyebrowLeftStart 22
#define kLandmarkIndexEyebrowLeftEnd 26
#define kLandmarkCountEyebrowLeft (kLandmarkIndexEyebrowLeftEnd - kLandmarkIndexEyebrowLeftStart + 1)

#define kParamEyebrowRight(i) "eyebrowRight" + std::to_string(i)
#define kParamEyebrowLeft(i) "eyebrowLeft" + std::to_string(i)

#define kLandmarkIndexNoseBridgeStart 27
#define kLandmarkIndexNoseBridgeEnd 30
#define kLandmarkCountNoseBridge (kLandmarkIndexNoseBridgeEnd - kLandmarkIndexNoseBridgeStart + 1)

#define kLandmarkIndexNoseBottomStart 31
#define kLandmarkIndexNoseBottomEnd 35
#define kLandmarkCountNoseBottom (kLandmarkIndexNoseBottomEnd - kLandmarkIndexNoseBottomStart + 1)

#define kParamNoseBridge(i) "noseBridge" + std::to_string(i)
#define kParamNoseBottom(i) "noseBottom" + std::to_string(i)

#define kLandmarkIndexEyeRightStart 36
#define kLandmarkIndexEyeRightEnd 41
#define kLandmarkCountEyeRight (kLandmarkIndexEyeRightEnd - kLandmarkIndexEyeRightStart + 1)

#define kLandmarkIndexEyeLeftStart 42
#define kLandmarkIndexEyeLeftEnd 47
#define kLandmarkCountEyeLeft (kLandmarkIndexEyeLeftEnd - kLandmarkIndexEyeLeftStart + 1)

#define kParamEyeRight(i) "eyeRight" + std::to_string(i)
#define kParamEyeLeft(i) "eyeLeft" + std::to_string(i)

#define kLandmarkIndexMouthOutsideStart 48
#define kLandmarkIndexMouthOutsideEnd 59
#define kLandmarkCountMouthOutside (kLandmarkIndexMouthOutsideEnd - kLandmarkIndexMouthOutsideStart + 1)

#define kLandmarkIndexMouthInsideStart 60
#define kLandmarkIndexMouthInsideEnd 67
#define kLandmarkCountMouthInside (kLandmarkIndexMouthInsideEnd - kLandmarkIndexMouthInsideStart + 1)

#define kParamMouthOutside(i) "mouthOutside" + std::to_string(i)
#define kParamMouthInside(i) "mouthInside" + std::to_string(i)
