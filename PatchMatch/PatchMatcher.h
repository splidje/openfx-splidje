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

    void bilinear(double x, double y, float* outPix) const;

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
    PatchMatcher(const VectorGrid* src, const VectorGrid* trg, int patchSize, const PatchMatchPlugin* plugin);

    void randomInitialise();
    void iterate(int numIterations);
    void merge(const VectorGrid* mergeTranslateMap, double scale);
    VectorGrid* releaseTranslateMap();

private:
    const VectorGrid* _src;
    const VectorGrid* _trg;
    const int _patchSize;
    const PatchMatchPlugin* _plugin;
    auto_ptr<double> _patchWeights;
    auto_ptr<VectorGrid> _translateMap;
    auto_ptr<VectorGrid> _distances;

    int _srcComps;
    int _trgComps;
    int _maxComps;
    auto_ptr<float> _trgBilinearPix;

    double _distance(OfxPointI& p, OfxPointD& offset);
    void _propagate(OfxPointI& p, OfxPointI& candP, OfxPointD* offset);
    void _search(OfxPointI& p, OfxPointD* offset);
    void _improve(OfxPointI& p, OfxPointD& candOffset, OfxPointD* offset);
};

#endif // def PATCHMATCHER_H
