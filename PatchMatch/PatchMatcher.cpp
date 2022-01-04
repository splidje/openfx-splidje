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
    auto vg = new VectorGrid(_rod * f, _componentCount);
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
    for (p.y = rod.y1; p.y < rod.y2; p.y++) {
        for (p.x = rod.x1; p.x < rod.x2; p.x++, dataPtr += 2, distPtr++) {
            if (_plugin->abort()) {return;}
            dataPtr[0] = offset.x = rand() % trgWidth + trgROD.x1 - p.x;
            dataPtr[1] = offset.y = rand() % trgHeight + trgROD.y1 - p.y;
            *distPtr = _distance(p, offset);
        }
    }
}

void PatchMatcher::iterate(int numIterations) {
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
