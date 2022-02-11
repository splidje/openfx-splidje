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
    _srcTrackRange = fetchInt3DParam(kParamSourceTrackRange);
    _srcTrackRangeButt = fetchPushButtonParam(kParamTrackSourceRange);
    _srcClearKeyframeAll = fetchPushButtonParam(kParamClearSourceKeyframeAll);
    _srcNoiseProfileRange = fetchInt2DParam(kParamSourceNoiseProfileRange);
    _srcRemoveNoise = fetchPushButtonParam(kParamRemoveSourceNoise);
    _trgTrack = fetchPushButtonParam(kParamTrackTarget);
    _trgTrackAll = fetchPushButtonParam(kParamTrackTargetAll);
    _referenceFrame = fetchIntParam(kParamReferenceFrame);
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

    // create a lookup
    std::map<std::pair<long, long>, long> edgeIndicesToEdge;
    auto edgePtr = _facePerimeter;
    for (auto i=0; i < 23; i++, edgePtr++) {
        edgeIndicesToEdge.insert({createEdgeMapKey(edgePtr->indexer.i1, edgePtr->indexer.i2), i});
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
                        auto edge = _facePerimeter[*ptr];
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
                            auto nextEdge = _facePerimeter[(*ptr + 1) % 23];
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

int _faceMeshTriIndices[111][3] = {
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
    {1, 17, 36},
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
    {1, 36, 41},
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
    {25, 16, 45},
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
    for (auto i=0; i < 111; i++) {
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

    // perimeter
    int i = 0;
    for (; i < 16; i++) {
        _facePerimeter[i] = TriangleMaths::Edge(vertices, i, i + 1);
    }
    _facePerimeter[i++] = TriangleMaths::Edge(vertices, 16, 26);
    _facePerimeter[i++] = TriangleMaths::Edge(vertices, 26, 25);
    _facePerimeter[i++] = TriangleMaths::Edge(vertices, 25, 24);
    _facePerimeter[i++] = TriangleMaths::Edge(vertices, 24, 19);
    _facePerimeter[i++] = TriangleMaths::Edge(vertices, 19, 18);
    _facePerimeter[i++] = TriangleMaths::Edge(vertices, 18, 17);
    _facePerimeter[i++] = TriangleMaths::Edge(vertices, 17, 0);
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
    auto jawStartParam = _srcFaceParams.landmarks[kLandmarkIndexJawStart];
    auto jawEndParam = _srcFaceParams.landmarks[kLandmarkIndexJawEnd];
    _edge_t refEdge = {
        jawStartParam->getValueAtTime(refFrame)
        ,jawEndParam->getValueAtTime(refFrame)
    };
    _edge_t edge = {
        jawStartParam->getValueAtTime(t)
        ,jawEndParam->getValueAtTime(t)
    };
    auto refCentre = _edgeCentre(refEdge);
    auto centre = _edgeCentre(edge);
    auto trans = _vectSub(refCentre, centre);
    auto refVect = _vectSub(refEdge.second, refEdge.first);
    auto vect = _vectSub(edge.second, edge.first);
    auto scale = sqrt(_vectMagSq(refVect) / _vectMagSq(vect));
    auto refAng = atan2(refVect.y, refVect.x);
    auto ang = atan2(vect.y, vect.x);
    auto rot = refAng - ang;
    // transform the landmarks at time t
    for (auto i = kLandmarkIndexJawStart; i <= kLandmarkIndexJawEnd; i++) {
        _transformPointParam(_srcFaceParams.landmarks[i], t, centre, rot, scale, trans);
    }
}

void FaceTranslationMapPlugin::changedParam(const InstanceChangedArgs &args, const std::string &paramName) {
    if (paramName == kParamTrackSource) {
        trackClipAtTime(_srcClip, &_srcFaceParams, args.time);
    } else if (paramName == kParamTrackSourceRange) {
        progressStart("Track Source Face");
        auto range = _srcTrackRange->getValue();
        for (auto t=range.x; t <= range.y; t += range.z) {
            trackClipAtTime(_srcClip, &_srcFaceParams, t);
            if (!progressUpdate((t - range.x) / (double)(range.y - range.x))) {return;}
        }
        progressEnd();
    } else if (paramName == kParamClearSourceKeyframeAll) {
        _srcFaceParams.bottomLeft->deleteKeyAtTime(args.time);
        _srcFaceParams.topRight->deleteKeyAtTime(args.time);
        for (auto i=0; i < kLandmarkCount; i++) {
            _srcFaceParams.landmarks[i]->deleteKeyAtTime(args.time);
        }
    } else if (paramName == kParamRemoveSourceNoise) {
        auto profRange = _srcNoiseProfileRange->getValue();

        OfxRangeD timeline;
        timeLineGetBounds(timeline.min, timeline.max);
        auto count = timeline.max - timeline.min + 1;
        std::vector<std::array<OfxPointD, kLandmarkCount>> data(count);
        progressStart("Reading Face Params");
        for (int t=0; t < count; t++) {
            for (int i=0; i < kLandmarkCount; i++) {
                data[t][i] = _srcFaceParams.landmarks[i]->getValueAtTime(timeline.min + t);
            }
            if (!progressUpdate(t / count)) {return;}
        }
        progressEnd();
        double profCount = profRange.y - profRange.x + 1;
        auto profOffset = profRange.x - timeline.min;
        auto freqCount = round(profCount / 2);
        std::vector<std::array<std::array<OfxPointD, 2>, kLandmarkCount>> freqResp(freqCount);
        progressStart("Calculating Profile Freq Resp");
        for (auto f=0; f < freqCount; f++) {
            auto d = profCount / 2;
            for (auto i=0; i < kLandmarkCount; i++) {
                freqResp[f][i][0] = {0, 0};
                freqResp[f][i][1] = {0, 0};
                for (int t=profOffset; t < profOffset + profCount; t++) {
                    auto tScaled = 2 * M_PI * t * (f + 1) / profCount;
                    auto s = sin(tScaled);
                    auto c = cos(tScaled);
                    freqResp[f][i][0].x += s * data[t][i].x;
                    freqResp[f][i][0].y += s * data[t][i].y;
                    freqResp[f][i][1].x += c * data[t][i].x;
                    freqResp[f][i][1].y += c * data[t][i].y;
                }
                freqResp[f][i][0].x /= d;
                freqResp[f][i][0].y /= d;
                freqResp[f][i][1].x /= d;
                freqResp[f][i][1].y /= d;
            }
            if (!progressUpdate(f / freqCount)) {return;}
        }
        progressEnd();

        // remove from whole timeline
        progressStart("Removing Freqs");
        for (int f=0; f < freqCount; f++) {
            for (int i=0; i < kLandmarkCount; i++) {
                for (int t=0; t < count; t++) {
                    auto tScaled = 2 * M_PI * t * (f + 1) / profCount;
                    auto sinVal = sin(tScaled);
                    auto cosVal = cos(tScaled);
                    data[t][i].x -= freqResp[f][i][0].x * sinVal + freqResp[f][i][1].x * cosVal;
                    data[t][i].y -= freqResp[f][i][0].y * sinVal + freqResp[f][i][1].y * cosVal;
                }
            }
            if (!progressUpdate(f / freqCount)) {return;}
        }
        progressEnd();

        progressStart("Updating Face Params");
        for (int t=0; t < count; t++) {
            for (int i=0; i < kLandmarkCount; i++) {
                _srcFaceParams.landmarks[i]->setValueAtTime(timeline.min + t, data[t][i]);
            }
            if (!progressUpdate(t / (count - 1))) {return;}
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
