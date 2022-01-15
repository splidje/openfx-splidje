#include "PatchMatcher.h"
#include "PatchMatchUtils.h"


VectorGrid::VectorGrid(const OfxRectI& rod, int componentCount)
    : _rod(rod), _componentCount(componentCount)
{
    _initialiseData();
}

VectorGrid::VectorGrid(Image* img)
    : _rod(img->getRegionOfDefinition())
    , _componentCount(img->getPixelComponentCount())
{
    _initialiseData();
    auto dataPtr = _data.get();
    for (auto y = _rod.y1; y < _rod.y2; y++) {
        for (auto x = _rod.x1; x < _rod.x2; x++) {
            auto pixPtr = (float*)img->getPixelAddress(x, y);
            for (auto c=0; c < _componentCount; c++, dataPtr++) {
                if (pixPtr) {
                    *dataPtr = pixPtr[c];
                } else {
                    *dataPtr = 0;
                }
            }
        }
    }
}

void VectorGrid::_initialiseData() {
    _dataWidth = rectWidth(_rod) * _componentCount;
    _data.reset(new float[_dataWidth * rectHeight(_rod)]);
}

void VectorGrid::toClip(Clip* clip, double t, const OfxRectI& window) const {
    auto_ptr<Image> img(clip->fetchImage(t));
    auto imgComponentCount = img->getPixelComponentCount();
    OfxPointI p;
    for (p.y = window.y1; p.y < window.y2; p.y++) {
        for (p.x = window.x1; p.x < window.x2; p.x++) {
            auto pixPtr = (float*)img->getPixelAddress(p.x, p.y);
            auto dataPtr = getVectorAddress(p);
            for (auto c=0; c < imgComponentCount; c++) {
                if (dataPtr && c < _componentCount) {
                    pixPtr[c] = dataPtr[c];
                } else {
                    pixPtr[c] = 0;
                }
            }
        }
    }
}

VectorGrid* VectorGrid::scale(double f) const {
    auto newROD = round(_rod * f);
    if (rectWidth(newROD) == 0 || rectHeight(newROD) == 0) {
        return NULL;
    }
    auto vg = new VectorGrid(newROD, _componentCount);
    auto dataPtr = vg->_data.get();
    for (auto y = vg->_rod.y1; y < vg->_rod.y2; y++) {
        for (auto x = vg->_rod.x1; x < vg->_rod.x2; x++, dataPtr += _componentCount) {
            bilinear(x / f, y / f, dataPtr, false);
        }
    }
    return vg;
}

inline float* VectorGrid::_getVectorAddress(const OfxPointI& p) const {
    return _data.get() + (p.y - _rod.y1) * _dataWidth + (p.x - _rod.x1) * _componentCount;
}

float* VectorGrid::getVectorAddress(const OfxPointI& p) const {
    if (insideRect(p, _rod)) {
        return _getVectorAddress(p);
    } else {
        return NULL;
    }
}

float* VectorGrid::getVectorAddressNearest(const OfxPointI& p) const {
    OfxPointI insideP;
    insideP.x = std::max(_rod.x1, std::min(_rod.x2 - 1, p.x));
    insideP.y = std::max(_rod.y1, std::min(_rod.y2 - 1, p.y));
    return _getVectorAddress(insideP);
}

void VectorGrid::bilinear(double x, double y, float* outPix, bool blackOutside) const {
    auto floorX = (int)floor(x);
    auto floorY = (int)floor(y);
    double xWeights[2];
    xWeights[1] = x - floorX;
    xWeights[0] = 1 - xWeights[1];
    double yWeights[2];
    yWeights[1] = y - floorY;
    yWeights[0] = 1 - yWeights[1];
    float* dataPtr;
    for (int c=0; c < _componentCount; c++) {outPix[c] = 0;}
    for (auto bY=0; bY < 2; bY++) {
        for (auto bX=0; bX < 2; bX++) {
            if (blackOutside) {
                dataPtr = getVectorAddress({floorX + bX, floorY + bY});
                if (!dataPtr) {continue;}
            } else {
                dataPtr = getVectorAddressNearest({floorX + bX, floorY + bY});
            }
            for (int c=0; c < _componentCount; c++, dataPtr++) {
                outPix[c] += xWeights[bX] * yWeights[bY] * *dataPtr;
            }
        }
    }
}

void VectorGrid::operator/=(const OfxPointD& d) {
    auto dataPtr = _data.get();
    for (auto y = _rod.y1; y < _rod.y2; y++) {
        for (auto x = _rod.x1; x < _rod.x2; x++, dataPtr += 2) {
            dataPtr[0] /= d.x;
            dataPtr[1] /= d.y;
        }
    }
}

inline double pdf(double x, double s) {
    return exp(-pow(x / s, 2) / 2) / (s * sqrt(2 * M_PI));
}

PatchMatcher::PatchMatcher(const VectorGrid* src, const VectorGrid* trg, int patchSize, PatchMatchPlugin* plugin)
    : _src(src), _trg(trg), _patchSize(patchSize), _plugin(plugin)
{
    _trgROD = trg->getROD();
    _trgWidth = rectWidth(_trgROD);
    _trgHeight = rectHeight(_trgROD);
    _offsetMap.reset(new VectorGrid(_trgROD, 2));
    _distances.reset(new VectorGrid(_trgROD, 1));
    _lastChange.reset(new int[_trgWidth * _trgHeight]);
    _srcComps = _src->getComponentCount();
    _trgComps = _trg->getComponentCount();
    _maxComps = std::max(_srcComps, _trgComps);
    _srcBilinearPix.reset(new float[_maxComps]);
    for (auto c=0; c < _maxComps; c++) {
        _srcBilinearPix.get()[c] = 0;
    }
    _currentPatch.reset(new float[_patchSize * _patchSize * _maxComps]);
    _patchWeights.reset(new double[_patchSize * _patchSize]);
    auto m = (_patchSize - 1) / 2;
    // auto s = sqrt(2 * (2*m + 1) * m * (m + 1) / (6 * (double)_patchSize));
    auto wPtr = _patchWeights.get();
    for (auto y = -m; y <= m; y++) {
        if (_plugin->abort()) {return;}
        for (auto x = -m; x <= m; x++, wPtr++) {
            *wPtr = pdf(x, 1) * pdf(y, 1);
        }
    }
}

void PatchMatcher::randomInitialise() {
    auto dataPtr = _offsetMap->getDataAddress();
    auto distPtr = _distances->getDataAddress();
    auto lastChangePtr = _lastChange.get();
    auto srcROD = _src->getROD();
    auto srcWidth = rectWidth(srcROD);
    auto srcHeight = rectHeight(srcROD);
    OfxPointD offset;
    std::uniform_real_distribution<float> distrX(0, srcWidth);
    std::uniform_real_distribution<float> distrY(0, srcHeight);
    for (_currentPatchP.y = _trgROD.y1; _currentPatchP.y < _trgROD.y2; _currentPatchP.y++) {
        for (_currentPatchP.x = _trgROD.x1; _currentPatchP.x < _trgROD.x2;
            _currentPatchP.x++, dataPtr += 2, distPtr++, lastChangePtr++)
        {
            if (_plugin->abort()) {return;}
            _loadCurrentPatch();
            dataPtr[0] = offset.x = distrX(*_plugin->randEng) + srcROD.x1 - _currentPatchP.x;
            dataPtr[1] = offset.y = distrY(*_plugin->randEng) + srcROD.y1 - _currentPatchP.y;
            *distPtr = _distance(offset);
            *lastChangePtr = 0;
        }
    }
}

void PatchMatcher::iterate(int numIterations) {
    OfxPointD offset;
    for (_currentIteration=0; _currentIteration < numIterations; _currentIteration++) {
        OfxPointI adjP;
        int beginX, beginY, endX, endY, step;
        if (_currentIteration % 2) {
            step = -1;
            beginX = _trgROD.x2 - 1;
            beginY = _trgROD.y2 - 2;
            endX = 0;
            endY = 0;
        } else {
            step = 1;
            beginX = 0;
            beginY = 0;
            endX = _trgROD.x2;
            endY = _trgROD.y2;
        }
        for (_currentPatchP.y = beginY; _currentPatchP.y != endY; _currentPatchP.y += step) {
            for (_currentPatchP.x = beginX; _currentPatchP.x != endX; _currentPatchP.x += step) {
                auto vPtr = _offsetMap->getVectorAddress(_currentPatchP);
                offset = {vPtr[0], vPtr[1]};
                _loadCurrentPatch();
                adjP = {_currentPatchP.x - step, _currentPatchP.y};
                _propagate(adjP, &offset);
                adjP = {_currentPatchP.x, _currentPatchP.y - step};
                _propagate(adjP, &offset);
                _search(&offset);
            }
        }
    }
}

inline void PatchMatcher::_loadCurrentPatch() {
    auto m = (_patchSize - 1) / 2;
    auto patchPtr = _currentPatch.get();
    OfxPointI o;
    for (o.y=-m; o.y <= m; o.y++) {
        for (o.x=-m; o.x <= m; o.x++) {
            auto trgP = _currentPatchP + o;
            auto trgPixPtr = _trg->getVectorAddressNearest(trgP);
            for (auto c=0; c < _maxComps; c++, patchPtr++, trgPixPtr++) {
                if (c < _trgComps) {
                    *patchPtr = *trgPixPtr;
                } else {
                    *patchPtr = 0;
                }
            }
        }
    }
}

inline void PatchMatcher::_propagate(const OfxPointI& candP, OfxPointD* offset) {
    if (!insideRect(candP, _trgROD)) {return;}
    auto lastChangePtr = _getLastChangePtr(candP);
    if ((_currentIteration - *lastChangePtr) > 1) {
        return;
    }
    auto candVPtr = _offsetMap->getVectorAddress(candP);
    OfxPointD candOffset = {candVPtr[0], candVPtr[1]};
    _improve(candOffset, offset);
}

inline void PatchMatcher::_search(OfxPointD* offset) {
    auto srcROD = _src->getROD();
    OfxPointD limits = {(double)rectWidth(srcROD), (double)rectHeight(srcROD)};
    OfxPointD newOffset;
    for (; limits.x >= 1 || limits.y >= 1; limits /= 2) {
        std::uniform_real_distribution<float> distrX(
            -std::min(limits.x, offset->x - srcROD.x1)
            ,std::min(limits.x, srcROD.x2 - offset->x)
        );
        std::uniform_real_distribution<float> distrY(
            -std::min(limits.y, offset->y - srcROD.y1)
            ,std::min(limits.y, srcROD.y2 - offset->y)
        );
        OfxPointD candOffset{
            offset->x + distrX(*_plugin->randEng)
            ,offset->y + distrY(*_plugin->randEng)
        };
        _improve(candOffset, &newOffset);
    }
    *offset = newOffset;
}

inline void PatchMatcher::_improve(const OfxPointD& candOffset, OfxPointD* offset) {
    auto distPtr = _distances->getVectorAddress(_currentPatchP);
    auto dist = _distance(candOffset, *distPtr);
    auto vPtr = _offsetMap->getVectorAddress(_currentPatchP);
    if (dist < *distPtr) {
        offset->x = vPtr[0] = candOffset.x;
        offset->y = vPtr[1] = candOffset.y;
        *distPtr = dist;
        *_getLastChangePtr(_currentPatchP) = _currentIteration;
    }
}

void PatchMatcher::merge(const VectorGrid* mergeTranslateMap, double scale) {
    auto dataPtr = _offsetMap->getDataAddress();
    auto rod = _offsetMap->getROD();
    OfxPointD offset;
    float mergeOffset[2];
    OfxPointD mergeOffsetSc;
    _currentIteration = 0;
    for (_currentPatchP.y = rod.y1; _currentPatchP.y < rod.y2; _currentPatchP.y++) {
        for (_currentPatchP.x = rod.x1; _currentPatchP.x < rod.x2;
            _currentPatchP.x++, dataPtr += 2)
        {
            if (_plugin->abort()) {return;}
            _loadCurrentPatch();
            auto mergeP = _currentPatchP * scale;
            mergeTranslateMap->bilinear(mergeP.x, mergeP.y, mergeOffset);
            mergeOffsetSc = {mergeOffset[0] / scale, mergeOffset[1] / scale};
            _improve(mergeOffsetSc, &offset);
        }
    }
}

VectorGrid* PatchMatcher::releaseOffsetMap() {
    return _offsetMap.release();
}

inline double PatchMatcher::_distance(const OfxPointD& offset, double maxDistance) {
    OfxPointI patchP;
    auto m = (_patchSize - 1) / 2;
    auto weightPtr = _patchWeights.get();
    auto srcCentre = _currentPatchP + offset;
    double dist = 0;
    auto trgPixPtr = _currentPatch.get();
    for (patchP.y = -m; patchP.y <= m; patchP.y++) {
        for (patchP.x = -m; patchP.x <= m; patchP.x++, weightPtr++) {
            auto srcP = srcCentre + patchP;
            _src->bilinear(srcP.x, srcP.y, _srcBilinearPix.get());
            auto srcPixPtr = _srcBilinearPix.get();
            for (auto c=0; c < _maxComps; c++, srcPixPtr++, trgPixPtr++) {
                float srcVal, trgVal;
                srcVal = *srcPixPtr;
                trgVal = *trgPixPtr;
                auto diff = trgVal - srcVal;
                dist += (diff * diff) * *weightPtr;
                if (maxDistance >= 0 && dist >= maxDistance) {
                    return dist;
                }
            }
        }
    }
    return dist;
}
