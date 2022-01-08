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
    auto vg = new VectorGrid(round(_rod * f), _componentCount);
    auto dataPtr = vg->_data.get();
    for (auto y = vg->_rod.y1; y < vg->_rod.y2; y++) {
        for (auto x = vg->_rod.x1; x < vg->_rod.x2; x++, dataPtr += _componentCount) {
            bilinear(x / f, y / f, dataPtr);
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

void VectorGrid::bilinear(double x, double y, float* outPix) const {
    auto floorX = (int)floor(x);
    auto floorY = (int)floor(y);
    double xWeights[2];
    xWeights[1] = x - floorX;
    xWeights[0] = 1 - xWeights[1];
    double yWeights[2];
    yWeights[1] = y - floorY;
    yWeights[0] = 1 - yWeights[1];
    for (int c=0; c < _componentCount; c++) {outPix[c] = 0;}
    for (auto y=0; y < 2; y++) {
        for (auto x=0; x < 2; x++) {
            auto dataPtr = getVectorAddressNearest({floorX + x, floorY + y});
            for (int c=0; c < _componentCount; c++, dataPtr++) {
                outPix[c] += xWeights[x] * yWeights[y] * *dataPtr;
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

PatchMatcher::PatchMatcher(const VectorGrid* src, const VectorGrid* trg, int patchSize, const PatchMatchPlugin* plugin)
    : _src(src), _trg(trg), _patchSize(patchSize), _plugin(plugin)
{
    _translateMap.reset(new VectorGrid(src->getROD(), 2));
    _distances.reset(new VectorGrid(src->getROD(), 1));
    _patchWeights.reset(new double[_patchSize * _patchSize]);
    auto m = (_patchSize - 1) / 2;
    auto s = sqrt(2 * (2*m + 1) * m * (m + 1) / (6 * (double)_patchSize));
    auto wPtr = _patchWeights.get();
    for (auto y = -m; y <= m; y++) {
        if (_plugin->abort()) {return;}
        for (auto x = -m; x <= m; x++, wPtr++) {
            *wPtr = pdf(x, s) * pdf(y, s);
        }
    }
    _srcComps = _src->getComponentCount();
    _trgComps = _trg->getComponentCount();
    _maxComps = std::max(_srcComps, _trgComps);
    _trgBilinearPix.reset(new float[_trgComps]);
}

void PatchMatcher::randomInitialise() {
    auto dataPtr = _translateMap->getDataAddress();
    auto distPtr = _distances->getDataAddress();
    auto rod = _translateMap->getROD();
    auto trgROD = _trg->getROD();
    auto trgWidth = rectWidth(trgROD);
    auto trgHeight = rectHeight(trgROD);
    OfxPointI p;
    OfxPointD offset;
    std::uniform_real_distribution<float> distrX(0, trgWidth);
    std::uniform_real_distribution<float> distrY(0, trgHeight);
    for (p.y = rod.y1; p.y < rod.y2; p.y++) {
        for (p.x = rod.x1; p.x < rod.x2; p.x++, dataPtr += 2, distPtr++) {
            if (_plugin->abort()) {return;}
            dataPtr[0] = offset.x = distrX(*_plugin->randEng) + trgROD.x1 - p.x;
            dataPtr[1] = offset.y = distrY(*_plugin->randEng) + trgROD.y1 - p.y;
            *distPtr = _distance(p, offset);
        }
    }
}

void PatchMatcher::iterate(int numIterations) {
    for (auto i=0; i < numIterations; i++) {
        OfxPointI p, adjP;
        auto rod = _translateMap->getROD();
        int beginX, beginY, endX, endY, step;
        if (i % 2) {
            step = -1;
            beginX = rod.x2 - 1;
            beginY = rod.y2 - 2;
            endX = 0;
            endY = 0;
        } else {
            step = 1;
            beginX = 0;
            beginY = 1;
            endX = rod.x2;
            endY = rod.y2;
        }
        for (p.y = beginY; p.y != endY; p.y += step) {
            for (p.x = beginX; p.x != endX; p.x += step) {
                adjP = {p.x - step, p.y};
                _propagate(p, adjP);
                adjP = {p.x, p.y - step};
                _propagate(p, adjP);
                _search(p);
            }
        }
    }
}

void PatchMatcher::_propagate(OfxPointI& p, OfxPointI& candP) {
    if (!insideRect(candP, _translateMap->getROD())) {return;}
    auto candVPtr = _translateMap->getVectorAddress(candP);
    OfxPointD offset = {candVPtr[0], candVPtr[1]};
    _improve(p, offset);
}

void PatchMatcher::_search(OfxPointI& p) {
    auto rod = _translateMap->getROD();
    OfxPointD limits = {(double)rectWidth(rod), (double)rectHeight(rod)};
    for (; limits.x >= 1 || limits.y >= 1; limits /= 2) {
        std::uniform_real_distribution<float> distrX(-limits.x, limits.x);
        std::uniform_real_distribution<float> distrY(-limits.y, limits.y);
        OfxPointD offset{
            distrX(*_plugin->randEng)
            ,distrY(*_plugin->randEng)
        };
        _improve(p, offset);
    }
}

void PatchMatcher::_improve(OfxPointI& p, OfxPointD& offset) {
    auto distPtr = _distances->getVectorAddress(p);
    auto dist = _distance(p, offset);
    auto vPtr = _translateMap->getVectorAddress(p);
    if (dist < *distPtr) {
        vPtr[0] = offset.x;
        vPtr[1] = offset.y;
        *distPtr = dist;
    }
}

void PatchMatcher::merge(const VectorGrid* mergeTranslateMap, double scale) {
    auto dataPtr = _translateMap->getDataAddress();
    auto distPtr = _distances->getDataAddress();
    auto rod = _translateMap->getROD();
    OfxPointD offset;
    OfxPointI p;
    float mergeOffset[2];
    OfxPointD mergeOffsetSc;
    for (p.y = rod.y1; p.y < rod.y2; p.y++) {
        for (p.x = rod.x1; p.x < rod.x2; p.x++, dataPtr += 2, distPtr++) {
            if (_plugin->abort()) {return;}
            auto mergeP = p * scale;
            mergeTranslateMap->bilinear(mergeP.x, mergeP.y, mergeOffset);
            mergeOffsetSc = {mergeOffset[0] / scale, mergeOffset[1] / scale};
            auto newDist = _distance(p, mergeOffsetSc);
            if (newDist < *distPtr) {
                dataPtr[0] = mergeOffsetSc.x;
                dataPtr[1] = mergeOffsetSc.y;
                *distPtr = newDist;
            }
        }
    }
}

VectorGrid* PatchMatcher::releaseTranslateMap() {
    return _translateMap.release();
}

double PatchMatcher::_distance(OfxPointI& p, OfxPointD& offset) {
    OfxPointI patchP;
    auto m = (_patchSize - 1) / 2;
    auto weightPtr = _patchWeights.get();
    auto trgCentre = p + offset;
    double dist = 0;
    for (patchP.y = -m; patchP.y <= m; patchP.y++) {
        for (patchP.x = -m; patchP.x <= m; patchP.x++, weightPtr++) {
            auto srcP = p + patchP;
            auto srcPixPtr = _src->getVectorAddressNearest(srcP);
            auto trgP = trgCentre + patchP;
            _trg->bilinear(trgP.x, trgP.y, _trgBilinearPix.get());
            for (auto c=0; c < _maxComps; c++) {
                float srcVal, trgVal;
                if (c < _srcComps) {
                    srcVal = srcPixPtr[c];
                } else {
                    srcVal = 0;
                }
                if (c < _trgComps) {
                    trgVal = _trgBilinearPix.get()[c];
                } else {
                    trgVal = 0;
                }
                auto diff = trgVal - srcVal;
                dist += (diff * diff) * *weightPtr;
            }
        }
    }
    return dist;
}
