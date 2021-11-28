#ifndef PATCHMATCHER_H
#define PATCHMATCHER_H

#include "PatchMatchPlugin.h"


class SimpleImage {
public:
    int width;
    int height;
    int components;
    float* data;
    bool _ours;
    SimpleImage(int w, int h, int c, float* d) {
        width = w;
        height = h;
        components = c;
        data = d;
        _ours = false;
    }
    SimpleImage(int w, int h, int c) {
        width = w;
        height = h;
        components = c;
        data = new float[w*h*c];
        _ours = true;
    }
    ~SimpleImage() {
        if (_ours) {delete[] data;}
    }
    inline bool valid(int x, int y) {
        return x >= 0 && y >= 0 && x < width && y < height;
    }
    inline float* pix(int x, int y) {
        return data + (y * width + x) * components;
    }
    inline OfxPointI vect(int x, int y) {
        OfxPointI p;
        if (valid(x,y)) {
            auto a = pix(x, y);
            p.x = a[0];
            p.y = a[1];
        }
        else {
            p.x = 0;
            p.y = 0;
        }
        return p;
    }
};


class PatchMatcher {
public:
    PatchMatcher(PatchMatchPlugin* plugin, const RenderArguments &args);
    ~PatchMatcher() {delete[] _patch.offXs;}
    void render();

private:
    SimpleImage* resample(const Image* image, double scale);
    void initialiseLevel();
    bool propagateAndSearch(int iterNum, int iterLen);
    inline void score(int xSrc, int ySrc, int xTrg, int yTrg, float* best
                     ,float idealRadSq=-1, float idealRad=0, float idealX=0, float idealY=0);
    inline void initScan(int xSrc, int ySrc, int xTrg, int yTrg);
    inline void nextScanRow();

    PatchMatchPlugin* _plugin;
    RenderArguments _renderArgs;

    auto_ptr<Image> _srcA;
    auto_ptr<Image> _srcB;
    auto_ptr<Image> _srcInit;

    auto_ptr<SimpleImage> _imgSrc;
    auto_ptr<SimpleImage> _imgTrg;
    auto_ptr<SimpleImage> _imgVect;
    int _numLevels, _startLevel, _endLevel, _level, _iterationNum, _iterationLength, _offX, _offY, _initFrame;
    double _iterations, _acceptableScore, _spatialImpairmentFactor, _maxDist;
    bool _initFromFrame;
    OfxPointI _logCoords;

    struct {
        int offY;
        int *offXs;
        int *endOffXs;
        int count;
    } _patch;

    struct {
        int numRows, numCols;
        float *pixSrc, *pixTrg;
        int *_offX;
        int _curOffX, _offXMax, _offXMin;
    } _scan;
};

inline int boundsWidth(const OfxRectI& bounds) {return bounds.x2 - bounds.x1;}
inline int boundsHeight(const OfxRectI& bounds) {return bounds.y2 - bounds.y1;}
inline double boundsWidth(const OfxRectD& bounds) {return bounds.x2 - bounds.x1;}
inline double boundsHeight(const OfxRectD& bounds) {return bounds.y2 - bounds.y1;}

#endif // def PATCHMATCHER_H
