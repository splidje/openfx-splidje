#include "FaceTrackPlugin.h"
#include "ofxsCoords.h"


FaceTrackPlugin::FaceTrackPlugin(OfxImageEffectHandle handle)
    : FaceTrackPluginBase(handle)
{
    _srcClip = fetchClip(kSourceClip);
    assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentRGB ||
    	    _srcClip->getPixelComponents() == ePixelComponentRGBA));
    _track = fetchPushButtonParam(kParamTrack);
    _trackForward = fetchPushButtonParam(kParamTrackAll);
    fetchFaceParams(&_face, "");
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

void FaceTrackPlugin::changedParam(const InstanceChangedArgs &args, const std::string &paramName) {
    if (paramName == kParamTrack) {
        trackClipAtTime(_srcClip, &_face, args.time);
    } else if (paramName == kParamTrackAll) {
        OfxRangeD timeline;
        this->timeLineGetBounds(timeline.min, timeline.max);
        progressStart("Tracking Faces...");
        for (auto t=timeline.min; t <= timeline.max; t++) {
            trackClipAtTime(_srcClip, &_face, t);
            if (
                !progressUpdate(
                    (t - timeline.min) / (timeline.max - timeline.min)
                )
            ) {return;}
        }
        progressEnd();
    }
}
