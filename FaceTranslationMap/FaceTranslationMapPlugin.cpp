#include "FaceTranslationMapPlugin.h"
#include "ofxsCoords.h"
#include "../TriangleMaths/TriangleMaths.h"


FaceTranslationMapPlugin::FaceTranslationMapPlugin(OfxImageEffectHandle handle)
    : FaceTrackPluginBase(handle)
{
    _srcClip = fetchClip(kSourceClip);
    assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentRGB ||
    	    _srcClip->getPixelComponents() == ePixelComponentRGBA));
    _trgClip = fetchClip(kTargetClip);
    assert(_trgClip && (_trgClip->getPixelComponents() == ePixelComponentRGB ||
    	    _trgClip->getPixelComponents() == ePixelComponentRGBA));
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentRGB ||
    	    _dstClip->getPixelComponents() == ePixelComponentRGBA));
    _srcTrack = fetchPushButtonParam(kParamTrackSource);
    _srcTrackAll = fetchPushButtonParam(kParamTrackSourceAll);
    _trgTrack = fetchPushButtonParam(kParamTrackTarget);
    _trgTrackAll = fetchPushButtonParam(kParamTrackTargetAll);
    _referenceFrame = fetchIntParam(kParamReferenceFrame);
    _calcRel = fetchPushButtonParam(kParamCalculateRelative);
    _calcRelAll = fetchPushButtonParam(kParamCalculateRelativeAll);
    _output = fetchChoiceParam(kParamOutput);
    fetchFaceParams(&_srcFaceParams, kFaceParamsPrefixSource);
    fetchFaceParams(&_trgFaceParams, kFaceParamsPrefixTarget);
    fetchFaceParams(&_relFaceParams, kFaceParamsPrefixRelative);
}

std::string FaceTranslationMapPlugin::getSelectedOuputOptionLabel(double t) {
    auto outputIdx = _output->getValueAtTime(t);
    std::string outputLabel;
    _output->getOption(outputIdx, outputLabel);
    return outputLabel;
}

bool FaceTranslationMapPlugin::isIdentity(const IsIdentityArguments &args, 
                                  Clip * &identityClip, double &/*identityTime*/
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
)
{
    auto outputLabel = getSelectedOuputOptionLabel(args.time);
    if (outputLabel == kParamOutputChoiceSourceLabel) {
        identityClip = _srcClip;
        return true;
    } else if (outputLabel == kParamOutputChoiceTargetLabel) {
        identityClip = _trgClip;
        return true;
    }
    return false;
}

void FaceTranslationMapPlugin::render(const OFX::RenderArguments &args) {
    auto outputLabel = getSelectedOuputOptionLabel(args.time);
    if (outputLabel != kParamOutputChoiceTranslationMapLabel) {
        return;
    }

    // collect points on target face
    std::vector<OfxPointD> trgPoints(kLandmarkCount);
    // collect translations
    std::vector<OfxPointD> transPoints(kLandmarkCount);
    for (int i=0; i < kLandmarkCount; i++) {
        trgPoints[i] = _trgFaceParams.landmarks[i]->getValueAtTime(args.time);
        transPoints[i] = _relFaceParams.landmarks[i]->getValueAtTime(args.time);
    }


    // work out mesh triangles
    auto mesh = TriangleMaths::delaunay(trgPoints);

    auto_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    auto par = dstImg->getPixelAspectRatio();
    auto componentCount = dstImg->getPixelComponentCount();

    int x;
    OfxPointD p;
    TriangleMaths::BarycentricWeights weights;
    OfxPointD trans;
    for (int y = args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        x = args.renderWindow.x1;
        auto dstPIX = (float*)dstImg->getPixelAddress(x, y);
        for (; x < args.renderWindow.x2; x++) {
            trans.x = 0;
            trans.y = 0;
            p.x = par * x / args.renderScale.x;
            p.y = y / args.renderScale.y;
            for (auto ptr = mesh.begin(); ptr < mesh.end(); ptr++) {
                if (abort()) {return;}
                auto tri = TriangleMaths::indexerToTri(*ptr, trgPoints);
                if (!TriangleMaths::circumCircleContains(tri, p)) {
                    continue;
                }
                weights = TriangleMaths::toBarycentric(p, tri);
                if (
                    weights.w1 >= 0 && weights.w1 <= 1
                    && weights.w2 >= 0 && weights.w2 <= 1
                    && weights.w3 >= 0 && weights.w3 <= 1
                ) {
                    trans.x = (
                        weights.w1 * transPoints[ptr->i1].x
                        + weights.w2 * transPoints[ptr->i2].x
                        + weights.w3 * transPoints[ptr->i3].x
                    );
                    trans.y = (
                        weights.w1 * transPoints[ptr->i1].y
                        + weights.w2 * transPoints[ptr->i2].y
                        + weights.w3 * transPoints[ptr->i3].y
                    );
                    break;
                }
            }
            for (int c=0; c < componentCount; c++, dstPIX++) {
                if (c == 0) {
                    *dstPIX = trans.x;
                } else if (c == 1) {
                    *dstPIX = trans.y;
                } else {
                    *dstPIX = 0;
                }
            }
        }
    }
}

void setRelParam(Double2DParam* param, double fromTime, double toTime, Double2DParam* result) {
    auto fromValue = param->getValueAtTime(fromTime);
    auto toValue = param->getValueAtTime(toTime);
    OfxPointD diff;
    diff.x = toValue.x - fromValue.x;
    diff.y = toValue.y - fromValue.y;
    result->setValueAtTime(toTime, diff);
}

void FaceTranslationMapPlugin::calculateRelative(double t) {
    auto refFrame = _referenceFrame->getValueAtTime(t);
    setRelParam(_srcFaceParams.bottomLeft, refFrame, t, _relFaceParams.bottomLeft);
    setRelParam(_srcFaceParams.topRight, refFrame, t, _relFaceParams.topRight);
    for (int i=0; i < kLandmarkCount; i++) {
        setRelParam(_srcFaceParams.landmarks[i], refFrame, t, _relFaceParams.landmarks[i]);
    }
}

void FaceTranslationMapPlugin::changedParam(const InstanceChangedArgs &args, const std::string &paramName) {
    if (paramName == kParamTrackSource) {
        trackClipAtTime(_srcClip, &_srcFaceParams, args.time);
    } else if (paramName == kParamTrackSourceAll) {
        OfxRangeD timeline;
        timeLineGetBounds(timeline.min, timeline.max);
        progressStart("Tracking Source Face...");
        for (auto t=timeline.min; t < timeline.max; t++) {
            trackClipAtTime(_srcClip, &_srcFaceParams, t);
            if (!progressUpdate(t / (timeline.max - 1))) {return;}
        }
        progressEnd();
    } if (paramName == kParamTrackTarget) {
        trackClipAtTime(_trgClip, &_trgFaceParams, args.time);
    } else if (paramName == kParamTrackTargetAll) {
        OfxRangeD timeline;
        timeLineGetBounds(timeline.min, timeline.max);
        progressStart("Tracking Target Face...");
        for (auto t=timeline.min; t < timeline.max; t++) {
            trackClipAtTime(_trgClip, &_trgFaceParams, t);
            if (!progressUpdate(t / (timeline.max - 1))) {return;}
        }
        progressEnd();
    } else if (paramName == kParamCalculateRelative) {
        calculateRelative(args.time);
    } else if (paramName == kParamCalculateRelativeAll) {
        OfxRangeD timeline;
        timeLineGetBounds(timeline.min, timeline.max);
        progressStart("Calculating Relative Face...");
        for (auto t=timeline.min; t < timeline.max; t++) {
            calculateRelative(t);
            if (!progressUpdate(t / (timeline.max - 1))) {return;}
        }
        progressEnd();
    }
}
