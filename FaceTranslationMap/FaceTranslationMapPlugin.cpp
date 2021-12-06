#include "FaceTranslationMapPlugin.h"
#include "ofxsCoords.h"
#include <chrono>


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
    _srcClearKeyframeAll = fetchPushButtonParam(kParamClearSourceKeyframeAll);
    _srcHighFreqRemovalCount = fetchIntParam(kParamSourceHighFreqRemovalCount);
    _srcRemoveHighFreqs = fetchPushButtonParam(kParamRemoveSourceHighFreqs);
    _trgTrack = fetchPushButtonParam(kParamTrackTarget);
    _trgTrackAll = fetchPushButtonParam(kParamTrackTargetAll);
    _referenceFrame = fetchIntParam(kParamReferenceFrame);
    _stabSrc = fetchPushButtonParam(kParamStabiliseSource);
    _stabCentre = fetchDouble2DParam(kParamStabilisedCentre);
    _stabTrans = fetchDouble2DParam(kParamStabilisedTranslate);
    _stabScale = fetchDoubleParam(kParamStabilisedScale);
    _stabRot = fetchDoubleParam(kParamStabilisedRotate);
    _calcRel = fetchPushButtonParam(kParamCalculateRelative);
    _calcRelAll = fetchPushButtonParam(kParamCalculateRelativeAll);
    _output = fetchChoiceParam(kParamOutput);
    _feather = fetchDoubleParam(kParamFeather);
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

std::pair<long, long> createEdgeMapKey(long i1, long i2) {
    if (i1 <= i2) {return {i1, i2};}
    else {return {i2, i1};}
}

void FaceTranslationMapPlugin::render(const OFX::RenderArguments &args) {
    auto outputLabel = getSelectedOuputOptionLabel(args.time);
    if (
        outputLabel != kParamOutputChoiceTranslationMapLabel
        && outputLabel != kParamOutputChoiceUVMapLabel
    ) {
        return;
    }

    auto_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    auto par = dstImg->getPixelAspectRatio();
    auto componentCount = dstImg->getPixelComponentCount();

    // collect points on target face
    std::vector<OfxPointD> trgPoints(kLandmarkCount);
    // collect translations
    std::vector<OfxPointD> transPoints(kLandmarkCount);
    for (int i=0; i < kLandmarkCount; i++) {
        trgPoints[i] = _trgFaceParams.landmarks[i]->getValueAtTime(args.time);
        trgPoints[i].x *= args.renderScale.x / par;
        trgPoints[i].y *= args.renderScale.y;
        transPoints[i] = _relFaceParams.landmarks[i]->getValueAtTime(args.time);
        transPoints[i].x *= args.renderScale.x / par;
        transPoints[i].y *= args.renderScale.y;
        if (outputLabel == kParamOutputChoiceUVMapLabel) {
            // translate the target
            trgPoints[i].x += transPoints[i].x;
            trgPoints[i].y += transPoints[i].y;
        }
    }

    _generateFaceMesh(&trgPoints);

    // find the perimeter edges
    auto perimeter = TriangleMaths::grahamScan(trgPoints);

    // create a lookup
    std::map<std::pair<long, long>, long> edgeIndicesToEdge;
    long i = 0;
    for (auto ptr=perimeter.begin(); ptr < perimeter.end(); ptr++, i++) {
        edgeIndicesToEdge.insert({createEdgeMapKey(ptr->indexer.i1, ptr->indexer.i2), i});
    }

    // TODO: handle different y render scale and par
    auto feather = _feather->getValueAtTime(args.time) * args.renderScale.x;

    OfxPointD p;
    TriangleMaths::BarycentricWeights weights;
    OfxPointD trans;

    // default to black
    for (p.y = args.renderWindow.y1; p.y < args.renderWindow.y2; p.y++) {
        p.x = args.renderWindow.x1;
        auto dstPIX = (float*)dstImg->getPixelAddress(p.x, p.y);
        for (; p.x < args.renderWindow.x2; p.x++) {
            for (int c=0; c < componentCount; c++, dstPIX++) {
                *dstPIX = 0;
            }
        }
    }

    // go through each triangle
    OfxRectD bounds;
    for (auto tri : _faceMesh) {
        if (abort()) {return;}
        OfxRectI extra({0, 0, 0, 0});
        std::vector<long> perimEdges;
        auto key = createEdgeMapKey(tri.i1, tri.i2);
        auto itemPtr = edgeIndicesToEdge.find(key);
        if (itemPtr != edgeIndicesToEdge.end()) {perimEdges.push_back(itemPtr->second);}
        key = createEdgeMapKey(tri.i2, tri.i3);
        itemPtr = edgeIndicesToEdge.find(key);
        if (itemPtr != edgeIndicesToEdge.end()) {perimEdges.push_back(itemPtr->second);}
        key = createEdgeMapKey(tri.i3, tri.i1);
        itemPtr = edgeIndicesToEdge.find(key);
        if (itemPtr != edgeIndicesToEdge.end()) {perimEdges.push_back(itemPtr->second);}
        if (perimEdges.size() > 0) {
            extra.x1 = -feather;
            extra.y1 = -feather;
            extra.x2 = feather;
            extra.y2 = feather;
        }
        bounds.x1 = std::max(tri.bounds.x1 + extra.x1, args.renderWindow.x1);
        bounds.y1 = std::max(tri.bounds.y1 + extra.y1, args.renderWindow.y1);
        bounds.x2 = std::min(tri.bounds.x2 + extra.x2, args.renderWindow.x2);
        bounds.y2 = std::min(tri.bounds.y2 + extra.y2, args.renderWindow.y2);
        for (p.y = bounds.y1; p.y < bounds.y2; p.y++) {
            p.x = bounds.x1;
            auto dstPIX = (float*)dstImg->getPixelAddress(p.x, p.y);
            for (; p.x < bounds.x2; p.x++) {
                weights = tri.toBarycentric(p);
                auto inside = (
                    weights.w1 >= 0 && weights.w1 <= 1
                    && weights.w2 >= 0 && weights.w2 <= 1
                    && weights.w3 >= 0 && weights.w3 <= 1
                );
                if (inside) {
                    trans.x = (
                        weights.w1 * transPoints[tri.i1].x
                        + weights.w2 * transPoints[tri.i2].x
                        + weights.w3 * transPoints[tri.i3].x
                    );
                    trans.y = (
                        weights.w1 * transPoints[tri.i1].y
                        + weights.w2 * transPoints[tri.i2].y
                        + weights.w3 * transPoints[tri.i3].y
                    );
                } else {
                    for (auto ptr=perimEdges.begin(); ptr < perimEdges.end(); ptr++) {
                        auto edge = perimeter[*ptr];
                        auto vectComp = edge.vectComp(p);
                        if (vectComp >= 0 && vectComp <= edge.magnitude) {
                            auto normComp = edge.normComp(p);
                            if (normComp > 0 && normComp <= feather) {
                                auto w2 = vectComp / edge.magnitude;
                                auto w1 = 1 - w2;
                                auto slope = pow(sin(M_PI_2 * (1 - normComp / feather)), 2);
                                trans.x = (
                                    (
                                        w1 * transPoints[edge.indexer.i1].x
                                        + w2 * transPoints[edge.indexer.i2].x
                                    ) * slope
                                );
                                trans.y = (
                                    (
                                        w1 * transPoints[edge.indexer.i1].y
                                        + w2 * transPoints[edge.indexer.i2].y
                                    ) * slope
                                );
                                inside = true;
                                break;
                            }
                        } else if (vectComp > edge.magnitude) {
                            auto nextEdge = perimeter[(*ptr + 1) % perimeter.size()];
                            vectComp = nextEdge.vectComp(p);
                            if (vectComp < 0) {
                                auto dist = sqrt(pow(p.x - edge.p2.x, 2) + pow(p.y - edge.p2.y, 2));
                                if (dist <= feather) {
                                    auto slope = pow(sin(M_PI_2 * (1 - dist / feather)), 2);
                                    trans.x = transPoints[edge.indexer.i2].x * slope;
                                    trans.y = transPoints[edge.indexer.i2].y * slope;
                                    inside = true;
                                }
                            }
                        }
                    }
                }
                if (outputLabel == kParamOutputChoiceUVMapLabel) {
                    trans.x = -trans.x;
                    trans.y = -trans.y;
                }
                if (inside) {
                    for (int c=0; c < componentCount; c++, dstPIX++) {
                        if (c == 0) {
                            *dstPIX = par * trans.x / args.renderScale.x;
                        } else if (c == 1) {
                            *dstPIX = trans.y / args.renderScale.y;
                        } else if (c == 3) {
                            *dstPIX = 1;
                        } else {
                            *dstPIX = 0;
                        }
                    }
                } else {
                    dstPIX += componentCount;
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

int _faceMeshTriIndices[113][3] = {
    {0, 1, 17},
    {21, 20, 23},
    {22, 21, 23},
    {20, 19, 24},
    {23, 20, 24},
    {25, 16, 26},
    {21, 22, 27},
    {1, 29, 30},
    {3, 2, 31},
    {2, 1, 31},
    {1, 30, 31},
    {31, 30, 32},
    {32, 30, 33},
    {33, 30, 34},
    {30, 29, 35},
    {34, 30, 35},
    {0, 17, 36},
    {17, 18, 37},
    {18, 19, 37},
    {36, 17, 37},
    {19, 20, 38},
    {37, 19, 38},
    {20, 21, 38},
    {21, 27, 39},
    {38, 21, 39},
    {27, 28, 39},
    {37, 38, 40},
    {28, 29, 40},
    {39, 28, 40},
    {38, 39, 40},
    {1, 0, 41},
    {0, 36, 41},
    {29, 1, 41},
    {40, 29, 41},
    {36, 37, 41},
    {37, 40, 41},
    {27, 22, 42},
    {28, 27, 42},
    {29, 28, 42},
    {35, 29, 42},
    {23, 24, 43},
    {22, 23, 43},
    {42, 22, 43},
    {24, 25, 44},
    {43, 24, 44},
    {16, 15, 45},
    {26, 16, 45},
    {25, 26, 45},
    {44, 25, 45},
    {15, 14, 46},
    {45, 15, 46},
    {44, 45, 46},
    {42, 43, 47},
    {43, 44, 47},
    {44, 46, 47},
    {35, 42, 47},
    {46, 35, 47},
    {4, 3, 48},
    {3, 31, 48},
    {5, 4, 48},
    {31, 32, 49},
    {48, 31, 49},
    {32, 33, 50},
    {49, 32, 50},
    {50, 33, 51},
    {34, 35, 52},
    {33, 34, 52},
    {51, 33, 52},
    {52, 35, 53},
    {14, 13, 54},
    {35, 46, 54},
    {46, 14, 54},
    {12, 11, 54},
    {13, 12, 54},
    {53, 35, 54},
    {11, 10, 54},
    {10, 9, 55},
    {54, 10, 55},
    {9, 8, 56},
    {55, 9, 56},
    {8, 7, 57},
    {56, 8, 57},
    {7, 6, 58},
    {57, 7, 58},
    {6, 5, 59},
    {58, 6, 59},
    {5, 48, 59},
    {48, 49, 60},
    {49, 60, 61},
    {49, 50, 61},
    {50, 61, 62},
    {50, 51, 62},
    {51, 62, 63},
    {51, 52, 63},
    {52, 53, 63},
    {53, 63, 64},
    {53, 54, 64},
    {54, 55, 64},
    {55, 64, 65},
    {55, 56, 65},
    {56, 65, 66},
    {56, 57, 66},
    {57, 66, 67},
    {57, 58, 67},
    {58, 59, 67},
    {59, 60, 67},
    {48, 59, 60},
    {60, 61, 67},
    {61, 66, 67},
    {61, 62, 66},
    {62, 65, 66},
    {62, 63, 65},
    {63, 64, 65},
};

inline void FaceTranslationMapPlugin::_generateFaceMesh(std::vector<OfxPointD>* vertices) {
    _faceMeshLock.lock();
    for (auto i=0; i < 113; i++) {
        _faceMesh[i] = TriangleMaths::Triangle(
            vertices,
            _faceMeshTriIndices[i][0],
            _faceMeshTriIndices[i][1],
            _faceMeshTriIndices[i][2]
        );
    }
    _faceMeshInitialised = true;
    _faceMeshLock.unlock();
    redrawOverlays();
}

typedef std::pair<OfxPointD, OfxPointD> _edge_t;

inline OfxPointD _vectAdd(OfxPointD p1, OfxPointD p2) {
    OfxPointD res;
    res.x = p1.x + p2.x;
    res.y = p1.y + p2.y;
    return res;
}

inline OfxPointD _vectSub(OfxPointD p1, OfxPointD p2) {
    OfxPointD res;
    res.x = p1.x - p2.x;
    res.y = p1.y - p2.y;
    return res;
}

inline OfxPointD _vectMult(OfxPointD p, double f) {
    OfxPointD res;
    res.x = p.x * f;
    res.y = p.y * f;
    return res;
}

inline OfxPointD _vectDiv(OfxPointD p, double f) {
    OfxPointD res;
    res.x = p.x / f;
    res.y = p.y / f;
    return res;
}

inline OfxPointD _edgeCentre(_edge_t e) {
    OfxPointD res;
    res.x = (e.first.x + e.second.x) / 2;
    res.y = (e.first.y + e.second.y) / 2;
    return res;
}

inline double _vectMagSq(OfxPointD e) {
    return e.x * e.x + e.y * e.y;
}

inline OfxPointD _vectRot(OfxPointD v, double ang) {
    OfxPointD res;
    auto cosAng = cos(ang);
    auto sinAng = sin(ang);
    res.x = cosAng * v.x - sinAng * v.y;
    res.y = sinAng * v.x + cosAng * v.y;
    return res;
}

inline void _transformPointParam(Double2DParam* param, double t, OfxPointD centre, double rot, double scale, OfxPointD trans) {
    auto p = param->getValueAtTime(t);
    auto vect = _vectSub(p, centre);
    p = _vectAdd(_vectAdd(_vectMult(_vectRot(vect, rot), scale), centre), trans);
    param->setValueAtTime(t, p);
}

void FaceTranslationMapPlugin::stabiliseSourceAtTime(double t) {
    auto refFrame = _referenceFrame->getValueAtTime(t);

    // average point at ref time
    OfxPointD refAvg({0, 0});
    OfxPointD avg({0, 0});
    for (auto i=0; i < kLandmarkCount; i++) {
        refAvg = _vectAdd(refAvg, _srcFaceParams.landmarks[i]->getValueAtTime(refFrame));
        avg = _vectAdd(avg, _srcFaceParams.landmarks[i]->getValueAtTime(t));
    }
    refAvg = _vectDiv(refAvg, kLandmarkCount);
    avg = _vectDiv(avg, kLandmarkCount);
    // std::cout << refAvg.x << "," << refAvg.y << " " << avg.x << "," << avg.y << std::endl;
    auto trans = _vectSub(refAvg, avg);
    _stabTrans->setValueAtTime(t, trans);
    // transform the landmarks at time t
    for (auto i = 0; i < kLandmarkCount; i++) {
        _transformPointParam(_srcFaceParams.landmarks[i], t, {0, 0}, 0, 1, trans);
    }
}

void FaceTranslationMapPlugin::changedParam(const InstanceChangedArgs &args, const std::string &paramName) {
    if (paramName == kParamTrackSource) {
        trackClipAtTime(_srcClip, &_srcFaceParams, args.time);
    } else if (paramName == kParamTrackSourceAll) {
        OfxRangeD timeline;
        timeLineGetBounds(timeline.min, timeline.max);
        progressStart("Track Source Face");
        for (auto t=timeline.min; t <= timeline.max; t++) {
            trackClipAtTime(_srcClip, &_srcFaceParams, t);
            if (!progressUpdate((t - timeline.min) / (timeline.max - timeline.min))) {return;}
        }
        progressEnd();
    } else if (paramName == kParamClearSourceKeyframeAll) {
        _srcFaceParams.bottomLeft->deleteKeyAtTime(args.time);
        _srcFaceParams.topRight->deleteKeyAtTime(args.time);
        for (auto i=0; i < kLandmarkCount; i++) {
            _srcFaceParams.landmarks[i]->deleteKeyAtTime(args.time);
        }
    } else if (paramName == kParamRemoveSourceHighFreqs) {
        progressStart("Removing Source High Frequencies");
        auto freqCount = _srcHighFreqRemovalCount->getValue();
        OfxRangeD timeline;
        timeLineGetBounds(timeline.min, timeline.max);
        long count = timeline.max - timeline.min + 1;
        std::vector<std::array<OfxPointD, kLandmarkCount>> data(count);
        for (int t=0; t < count; t++) {
            for (int i=0; i < kLandmarkCount; i++) {
                data[t][i] = _srcFaceParams.landmarks[i]->getValueAtTime(timeline.min + t);
            }
            if (!progressUpdate(0.2 * t / (count - 1))) {return;}
        }
        // ensure even
        auto evenCount = (count >> 1) << 1;
        std::vector<std::array<std::array<OfxPointD, 2>, kLandmarkCount>> freqResp(freqCount);
        for (int f=0; f < freqCount; f++) {
            for (int i=0; i < kLandmarkCount; i++) {
                for (int ph=0; ph < 2; ph++) {
                    freqResp[f][i][ph].x = 0;
                    freqResp[f][i][ph].y = 0;
                }
            }
            if (!progressUpdate(0.2 + 0.2 * f / (freqCount - 1))) {return;}
        }
        for (double t=0; t < evenCount; t++) {
            for (int i=0; i < kLandmarkCount; i++) {
                auto p = data[t][i];
                for (int f=0; f < freqCount; f++) {
                    auto tScaled = M_PI * t * (evenCount - 2 * f) / evenCount;
                    auto sinVal = sin(tScaled);
                    freqResp[f][i][0].x += p.x * sinVal;
                    freqResp[f][i][0].y += p.y * sinVal;
                    auto cosVal = cos(tScaled);
                    freqResp[f][i][1].x += p.x * cosVal;
                    freqResp[f][i][1].y += p.y * cosVal;
                }
            }
            if (!progressUpdate(0.4 + 0.2 * t / (evenCount - 1))) {return;}
        }
        for (int f=0; f < freqCount; f++) {
            for (int i=0; i < kLandmarkCount; i++) {
                for (int ph=0; ph < 2; ph++) {
                    freqResp[f][i][ph].x /= evenCount >> (f > 0);
                    freqResp[f][i][ph].y /= evenCount >> (f > 0);
                }
                for (int t=0; t < count; t++) {
                    auto tScaled = M_PI * t * (evenCount - 2 * f) / evenCount;
                    auto sinVal = sin(tScaled);
                    auto cosVal = cos(tScaled);
                    data[t][i].x -= freqResp[f][i][0].x * sinVal + freqResp[f][i][1].x * cosVal;
                    data[t][i].y -= freqResp[f][i][0].y * sinVal + freqResp[f][i][1].y * cosVal;
                }
            }
            if (!progressUpdate(0.6 + 0.2 * f / (evenCount - 1))) {return;}
        }
        for (int t=0; t < count; t++) {
            for (int i=0; i < kLandmarkCount; i++) {
                _srcFaceParams.landmarks[i]->setValueAtTime(timeline.min + t, data[t][i]);
            }
            if (!progressUpdate(0.8 + 0.2 * t / (count - 1))) {return;}
        }
        progressEnd();
    } if (paramName == kParamTrackTarget) {
        trackClipAtTime(_trgClip, &_trgFaceParams, args.time);
    } else if (paramName == kParamTrackTargetAll) {
        OfxRangeD timeline;
        timeLineGetBounds(timeline.min, timeline.max);
        progressStart("Tracking Target Face");
        for (auto t=timeline.min; t <= timeline.max; t++) {
            trackClipAtTime(_trgClip, &_trgFaceParams, t);
            if (!progressUpdate((t - timeline.min) / (timeline.max - timeline.min))) {return;}
        }
        progressEnd();
    } else if (paramName == kParamStabiliseSource) {
        OfxRangeD timeline;
        timeLineGetBounds(timeline.min, timeline.max);
        progressStart("Stabilising Source");
        for (auto t=timeline.min; t <= timeline.max; t++) {
            stabiliseSourceAtTime(t);
            if (!progressUpdate((t - timeline.min) / (timeline.max - timeline.min))) {return;}
        }
        progressEnd();
    } else if (paramName == kParamCalculateRelative) {
        calculateRelative(args.time);
    } else if (paramName == kParamCalculateRelativeAll) {
        OfxRangeD timeline;
        timeLineGetBounds(timeline.min, timeline.max);
        progressStart("Calculating Relative Face");
        for (auto t=timeline.min; t < timeline.max; t++) {
            calculateRelative(t);
            if (!progressUpdate(t / (timeline.max - 1))) {return;}
        }
        progressEnd();
    }
}
