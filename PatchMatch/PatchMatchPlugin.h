#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include <iostream>

using namespace OFX;

#define kPluginName "PatchMatch"
#define kPluginGrouping "Transform"
#define kPluginDescription \
"PatchMatch"

#define kPluginIdentifier "com.ajptechnical.openfx.PatchMatch"
#define kPluginVersionMajor 0
#define kPluginVersionMinor 1

#define kSourceAClip "Source"
#define kSourceBClip "Target"

#define kParamPatchSize "patchSize"
#define kParamPatchSizeLabel "Patch Size"
#define kParamPatchSizeHint "Patch Size"

#define kParamStartLevel "startLevel"
#define kParamStartLevelLabel "Start Level"
#define kParamStartLevelHint "Start Level"

#define kParamEndLevel "endLevel"
#define kParamEndLevelLabel "End Level"
#define kParamEndLevelHint "End Level"

#define kParamIterations "iterations"
#define kParamIterationsLabel "Iterations"
#define kParamIterationsHint "Iterations"

#define kParamAcceptableScore "acceptableScore"
#define kParamAcceptableScoreLabel "Acceptable Score"
#define kParamAcceptableScoreHint "Acceptable Score"

#define kParamSimilarityThreshold "similarityThreshold"
#define kParamSimilarityThresholdLabel "Similarity Threshold"
#define kParamSimilarityThresholdHint "Similarity Threshold"

#define kParamRandomSeed "seed"
#define kParamRandomSeedLabel "Random Seed"
#define kParamRandomSeedHint "Random Seed"

#define kParamLogCoords "logCoords"
#define kParamLogCoordsLabel "Log Coords"
#define kParamLogCoordsHint "Log Coords"

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

class PatchMatchPlugin : public ImageEffect
{
public:
    PatchMatchPlugin(OfxImageEffectHandle handle);
    
private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    
    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
    ) OVERRIDE FINAL;

    bool getRegionOfDefinition(const RegionOfDefinitionArguments &args, OfxRectD &rod) OVERRIDE FINAL;

    int calculateNumLevelsAtTime(double time);

    SimpleImage* resample(const Image* image, double scale);
    SimpleImage* initialiseLevel(SimpleImage* imgSrc, SimpleImage* imgTrg
                                ,SimpleImage* imgHint
                                ,int patchSize, float threshold);
    bool propagateAndSearch(SimpleImage* imgVect, SimpleImage* imgSrc
                           ,SimpleImage* imgTrg
                           ,int patchSize, float threshold
                           ,int iterationNum, int length);
    void score(int xSrc, int ySrc, int xTrg, int yTrg
                ,SimpleImage* imgSrc, SimpleImage* imgTrg
                ,int patchSize, float threshold, float* best);

private:
    // do not need to delete these, the ImageEffect is managing them for us
    Clip* _dstClip;
    Clip* _srcAClip;
    Clip* _srcBClip;
    IntParam* _patchSize;
    IntParam* _endLevel;
    IntParam* _startLevel;
    DoubleParam* _iterations;
    DoubleParam* _acceptableScore;
    DoubleParam* _similarityThreshold;
    IntParam* _randomSeed;
    Int2DParam* _logCoords;
    double _curAcceptableScore;
    OfxPointI _curLogCoords;
    bool _finalLevel;
};

inline int boundsWidth(const OfxRectI& bounds) {return bounds.x2 - bounds.x1;}
inline int boundsHeight(const OfxRectI& bounds) {return bounds.y2 - bounds.y1;}
inline double boundsWidth(const OfxRectD& bounds) {return bounds.x2 - bounds.x1;}
inline double boundsHeight(const OfxRectD& bounds) {return bounds.y2 - bounds.y1;}
