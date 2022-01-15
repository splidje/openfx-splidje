#ifndef PATCHMATCHER_H
#define PATCHMATCHER_H

#include "PatchMatchPlugin.h"


class VectorGrid {
public:
    VectorGrid(const OfxRectI& rod, int componentCount);
    VectorGrid(Image* img);

    inline static VectorGrid* fromClip(Clip* clip, double t) {
        auto_ptr<Image> img(clip->fetchImage(t));
        if (!img.get()) {return NULL;}
        return new VectorGrid(img.get());
    }

    void toClip(Clip* clip, double t, const OfxRectI& window) const;

    VectorGrid* scale(double f) const;

    float* getVectorAddress(const OfxPointI& p) const;
    float* getVectorAddressNearest(const OfxPointI& p) const;

    void bilinear(double x, double y, float* outPix, bool blackOutside=true) const;

    OfxRectI getROD() const {return _rod;}

    inline float* getDataAddress() {return _data.get();}
    inline int getComponentCount() const {return _componentCount;}

    void operator/=(const OfxPointD& d);

private:
    OfxRectI _rod;
    int _componentCount;
    int _dataWidth;
    auto_ptr<float> _data;

    void _initialiseData();
    float* _getVectorAddress(const OfxPointI& p) const;
};


class PatchMatcher {
public:
    PatchMatcher(const VectorGrid* src, const VectorGrid* trg, int patchSize, PatchMatchPlugin* plugin);

    void randomInitialise();
    void iterate(int numIterations);
    void merge(const VectorGrid* mergeTranslateMap, double scale);
    VectorGrid* releaseOffsetMap();

private:
    const VectorGrid* _src;
    const VectorGrid* _trg;
    OfxRectI _trgROD;
    int _trgWidth;
    int _trgHeight;
    const int _patchSize;
    PatchMatchPlugin* _plugin;
    auto_ptr<double> _patchWeights;
    auto_ptr<VectorGrid> _offsetMap;
    auto_ptr<VectorGrid> _distances;

    int _srcComps;
    int _trgComps;
    int _maxComps;
    auto_ptr<float> _srcBilinearPix;
    int _currentIteration;
    auto_ptr<int> _lastChange;
    OfxPointI _currentPatchP;
    auto_ptr<float> _currentPatch;

    double _distance(const OfxPointD& offset, double maxDistance=-1);
    void _propagate(const OfxPointI& candP, OfxPointD* offset);
    void _search(OfxPointD* offset);
    void _improve(const OfxPointD& candOffset, OfxPointD* offset);
    void _loadCurrentPatch();
    inline int* _getLastChangePtr(const OfxPointI& p) {
        return _lastChange.get() + (p.y - _trgROD.y1) * _trgWidth + (p.x - _trgROD.x1);
    }
};

#endif // def PATCHMATCHER_H
