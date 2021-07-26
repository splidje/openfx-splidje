#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include <iostream>
#include <mutex>

using namespace OFX;


class CornerPinPlugin : public ImageEffect
{
public:
    CornerPinPlugin(OfxImageEffectHandle handle);

    std::vector<std::vector<OfxPointD>> getIntersections();

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    
    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
    ) OVERRIDE FINAL;

    virtual void getRegionsOfInterest(const RegionsOfInterestArguments &args, RegionOfInterestSetter &rois);

    void setIntersections(std::vector<std::vector<OfxPointD>> intersections);

private:
    Clip* _srcClip;
    Clip* _dstClip;
    Double2DParam* _bottomLeft;
    Double2DParam* _bottomRight;
    Double2DParam* _topLeft;
    Double2DParam* _topRight;

    std::vector<std::vector<OfxPointD>> _intersections;
    std::mutex _intersectionsLock;
};
