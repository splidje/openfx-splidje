#include "ofxsImageEffect.h"
#include "ofxsProcessing.H"
#include "ofxsMacros.h"
#include "ofxsMaskMix.h"
#include "triangleMaths.hpp"
#include <cmath>

#ifdef __APPLE__
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION // Yes, we are still doing OpenGL 2.1
#endif
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

using namespace OFX;
//using namespace std;

#define kPluginName "MeshWarp"
#define kPluginGrouping "Transform"
#define kPluginDescription \
"Mesh Warp"

#define kPluginIdentifier "com.ajptechnical.openfx.MeshWarp"
#define kPluginVersionMajor 0
#define kPluginVersionMinor 1

#define kSupportsTiles 1
#define kSupportsMultiResolution 1
#define kSupportsRenderScale 1
#define kSupportsMultipleClipPARs false
#define kSupportsMultipleClipDepths true
#define kRenderThreadSafety eRenderFullySafe

// Base class for the RGBA and the Alpha processor
class MeshWarpBase
    : public ImageProcessor
{
protected:
    const Image *_srcImg;
    const Image *_maskImg;
    std::vector<std::pair<OfxPointD, OfxPointD>> _fromTos;

public:
    /** @brief no arg ctor */
    MeshWarpBase(ImageEffect &instance)
        : ImageProcessor(instance)
        , _srcImg(NULL)
        , _maskImg(NULL)
    {
    }

    /** @brief set the src image */
    void setSrcImg(const Image *v) {_srcImg = v; }

    void addFromTo(OfxPointD fromP, OfxPointD toP)
    {
        _fromTos.push_back({fromP, toP});
    }
};

// template to do the RGBA processing
template <class PIX, int nComponents, int maxValue>
class MeshWarper
    : public MeshWarpBase
{
public:
    // ctor
    MeshWarper(ImageEffect &instance)
        : MeshWarpBase(instance)
    {
    }

private:
    // and do some processing
    void multiThreadProcessImages(const OfxRectI& procWindow, const OfxPointD& rs) OVERRIDE FINAL
    {
#     ifndef __COVERITY__ // too many coverity[dead_error_line] errors
        const bool r = true && (nComponents != 1);
        const bool g = true && (nComponents >= 2);
        const bool b = true && (nComponents >= 3);
        const bool a = true && (nComponents == 1 || nComponents == 4);
        if (r) {
            if (g) {
                if (b) {
                    if (a) {
                        return process<true, true, true, true >(procWindow, rs); // RGBA
                    } else {
                        return process<true, true, true, false>(procWindow, rs); // RGBa
                    }
                } else {
                    if (a) {
                        return process<true, true, false, true >(procWindow, rs); // RGbA
                    } else {
                        return process<true, true, false, false>(procWindow, rs); // RGba
                    }
                }
            } else {
                if (b) {
                    if (a) {
                        return process<true, false, true, true >(procWindow, rs); // RgBA
                    } else {
                        return process<true, false, true, false>(procWindow, rs); // RgBa
                    }
                } else {
                    if (a) {
                        return process<true, false, false, true >(procWindow, rs); // RgbA
                    } else {
                        return process<true, false, false, false>(procWindow, rs); // Rgba
                    }
                }
            }
        } else {
            if (g) {
                if (b) {
                    if (a) {
                        return process<false, true, true, true >(procWindow, rs); // rGBA
                    } else {
                        return process<false, true, true, false>(procWindow, rs); // rGBa
                    }
                } else {
                    if (a) {
                        return process<false, true, false, true >(procWindow, rs); // rGbA
                    } else {
                        return process<false, true, false, false>(procWindow, rs); // rGba
                    }
                }
            } else {
                if (b) {
                    if (a) {
                        return process<false, false, true, true >(procWindow, rs); // rgBA
                    } else {
                        return process<false, false, true, false>(procWindow, rs); // rgBa
                    }
                } else {
                    if (a) {
                        return process<false, false, false, true >(procWindow, rs); // rgbA
                    } else {
                        return process<false, false, false, false>(procWindow, rs); // rgba
                    }
                }
            }
        }
#     endif // ifndef __COVERITY__
    } // multiThreadProcessImages

private:
    template<bool processR, bool processG, bool processB, bool processA>
    void process(const OfxRectI& procWindow, const OfxPointD& rs)
    {
        unused(rs);
        float unpPix[4];
        float tmpPix[4];

        // TODO: actually work with variable number of points

        // TODO: do the below with an algorithm
        // establish triangles
        const OfxRectI boundsI = _srcImg->getBounds();
        const OfxRectD boundsD = {(double)boundsI.x1, (double)boundsI.y1, (double)boundsI.x2, (double)boundsI.y2};

        std::vector<std::pair<OfxPointD, OfxPointD>> pins = {
            {{boundsD.x1, boundsD.y1}, {boundsD.x1, boundsD.y1}}
            ,{{boundsD.x2, boundsD.y1}, {boundsD.x2, boundsD.y1}}
            ,{{boundsD.x2, boundsD.y2}, {boundsD.x2, boundsD.y2}}
            ,{{boundsD.x1, boundsD.y2}, {boundsD.x1, boundsD.y2}}
            ,{  {_fromTos[0].first.x * rs.x, _fromTos[0].first.y * rs.y}
                ,{_fromTos[0].second.x * rs.x, _fromTos[0].second.y * rs.y}
            }
        };

        typedef struct TriIndexer {
            unsigned int i1, i2, i3;
        } TriIndexer;

        std::vector<TriIndexer> triangles = {
            {0, 1, 4}
            ,{1, 2, 4}
            ,{2, 3, 4}
            ,{3, 4, 0}
        };

        // go through every destination coord
        for (int y = procWindow.y1; y < procWindow.y2; y++) {
            if ( _effect.abort() ) {
                break;
            }
            PIX *dstPix = (PIX *) _dstImg->getPixelAddress(procWindow.x1, y);
            for (int x = procWindow.x1; x < procWindow.x2; x++) {
                OfxPointD p = {x, y};

                // default to black
                for (int c = 0; c < nComponents; c++) {
                    dstPix[c] = 0;
                }

                // go through each tri
                for (int ti = 0; ti < triangles.size(); ++ti)
                {
                    // establish the points of this tri
                    TriIndexer triI = triangles[ti];
                    TriangleMaths::Triangle toTri = {
                        pins[triI.i1].second
                        ,pins[triI.i2].second
                        ,pins[triI.i3].second
                    };
                    // get barycentric coords in to tri
                    TriangleMaths::BarycentricWeights bw = TriangleMaths::toBarycentric(p, toTri);
                    // is it in this tri?
                    if (!(bw.w1 < 0 || bw.w2 < 0 || bw.w3 < 0)) {
                        TriangleMaths::Triangle fromTri = {
                            pins[triI.i1].first
                            ,pins[triI.i2].first
                            ,pins[triI.i3].first
                        };

                        // convert to image coords in from tri
                        OfxPointD pFromD = TriangleMaths::fromBarycentric(bw, fromTri);

                        // inside bounds of src?
                        if (pFromD.y >= boundsI.y1
                            && pFromD.y < boundsI.y2 - 1
                            && pFromD.x >= boundsI.x1
                            && pFromD.x < boundsI.x2 - 1
                        ) {
                            int baseX = (int)pFromD.x;
                            int baseY = (int)pFromD.y;

                            double errX = pFromD.x - baseX;
                            double errY = pFromD.y - baseY;

                            double mixPix[nComponents] = {0};
                            double weights[2][2];
                            weights[0][0] = (1 - errX) * (1 - errY);
                            weights[0][1] = (1 - errX) * errY;
                            weights[1][0] = errX * (1 - errY);
                            weights[1][1] = errX * errY;
                            for (int pxX = 0; pxX < 2; pxX++) {
                                for (int pxY = 0; pxY < 2; pxY++) {
                                    double weight = weights[pxX][pxY];
                                    const PIX* pxPix = (const PIX *)(_srcImg ? _srcImg->getPixelAddress(baseX + pxX, baseY + pxY) : 0);
                                    for (int c = 0; c < nComponents; c++) {
                                        mixPix[c] += pxPix[c] * weight;
                                    }
                                }
                            }
                            for (int c = 0; c < nComponents; c++) {
                                dstPix[c] = (PIX)mixPix[c];
                            }
                        }
                        break;
                    }
                }

                // increment the dst pixel
                dstPix += nComponents;
            }
        }
    }
};

class MeshWarpPlugin : public OFX::ImageEffect
{
public:
    MeshWarpPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
    , _dstClip(0)
    , _srcClip(0)
    , _maskClip(0)
    {
        _dstClip = fetchClip(kOfxImageEffectOutputClipName);
        assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentRGB ||
			    _dstClip->getPixelComponents() == ePixelComponentRGBA));
        _srcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
        assert(_srcClip && (_srcClip->getPixelComponents() == ePixelComponentRGB ||
			    _srcClip->getPixelComponents() == ePixelComponentRGBA));
        _maskClip = getContext() == OFX::eContextFilter ? NULL : fetchClip(getContext() == OFX::eContextPaint ? "Brush" : "Mask");
        assert(!_maskClip || _maskClip->getPixelComponents() == ePixelComponentAlpha);
        _from001 = fetchDouble2DParam("from001");
        _to001 = fetchDouble2DParam("to001");
    }
    
private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;
    
    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
    ) OVERRIDE FINAL;

    /** @brief called when a clip has just been changed in some way (a rewire maybe) */
    virtual void changedClip(const InstanceChangedArgs &args, const std::string &clipName) OVERRIDE FINAL;

    template <int nComponents>
    void renderInternal(const RenderArguments &args,
                        BitDepthEnum dstBitDepth);

    /* set up and run a processor */
    void setupAndProcess(MeshWarpBase &, const RenderArguments &args);

private:
    // do not need to delete these, the ImageEffect is managing them for us
    Clip *_dstClip;
    Clip *_srcClip;
    Clip *_maskClip;
    Double2DParam* _from001;
    Double2DParam* _to001;
};

// the overridden render function
void
MeshWarpPlugin::render(const OFX::RenderArguments &args)
{
    //std::cout << "render!\n";
    // instantiate the render code based on the pixel depth of the dst clip
    BitDepthEnum dstBitDepth    = _dstClip->getPixelDepth();
    PixelComponentEnum dstComponents  = _dstClip->getPixelComponents();

    assert( kSupportsMultipleClipPARs   || !_srcClip || !_srcClip->isConnected() || _srcClip->getPixelAspectRatio() == _dstClip->getPixelAspectRatio() );
    assert( kSupportsMultipleClipDepths || !_srcClip || !_srcClip->isConnected() || _srcClip->getPixelDepth()       == _dstClip->getPixelDepth() );
    // do the rendering
    if (dstComponents == ePixelComponentRGBA) {
        renderInternal<4>(args, dstBitDepth);
    } else if (dstComponents == ePixelComponentRGB) {
        renderInternal<3>(args, dstBitDepth);
#ifdef OFX_EXTENSIONS_NATRON
    } else if (dstComponents == ePixelComponentXY) {
        renderInternal<2>(args, dstBitDepth);
#endif
    } else {
        assert(dstComponents == ePixelComponentAlpha);
        renderInternal<1>(args, dstBitDepth);
    }
    //std::cout << "render! OK\n";
}

/* set up and run a processor */
void
MeshWarpPlugin::setupAndProcess(MeshWarpBase &processor,
                              const RenderArguments &args)
{
    //std::cout << "setupAndProcess!\n";
    // get a dst image
    auto_ptr<Image> dst( _dstClip->fetchImage(args.time) );

    if ( !dst.get() ) {
        //std::cout << "setupAndProcess! can' fetch dst\n";
        throwSuiteStatusException(kOfxStatFailed);
    }
# ifndef NDEBUG
    BitDepthEnum dstBitDepth    = dst->getPixelDepth();
    PixelComponentEnum dstComponents  = dst->getPixelComponents();
    if ( ( dstBitDepth != _dstClip->getPixelDepth() ) ||
         ( dstComponents != _dstClip->getPixelComponents() ) ) {
        //std::cout << "setupAndProcess! OFX Host gave image with wrong depth or components\n";
        setPersistentMessage(Message::eMessageError, "", "OFX Host gave image with wrong depth or components");
        throwSuiteStatusException(kOfxStatFailed);
    }
    checkBadRenderScaleOrField(dst, args);
# endif

    // fetch main input image
    auto_ptr<const Image> src( ( _srcClip && _srcClip->isConnected() ) ?
                                    _srcClip->fetchImage(args.time) : 0 );

# ifndef NDEBUG
    // make sure bit depths are sane
    if ( src.get() ) {
        checkBadRenderScaleOrField(src, args);
        BitDepthEnum srcBitDepth      = src->getPixelDepth();
        PixelComponentEnum srcComponents = src->getPixelComponents();

        // see if they have the same depths and bytes and all
        if ( (srcBitDepth != dstBitDepth) || (srcComponents != dstComponents) ) {
            throwSuiteStatusException(kOfxStatErrImageFormat);
        }
    }
# endif

    // set the images
    processor.setDstImg( dst.get() );
    processor.setSrcImg( src.get() );

    processor.addFromTo(_from001->getValueAtTime(args.time), _to001->getValueAtTime(args.time));

    // set the render window
    processor.setRenderWindow(args.renderWindow, args.renderScale);

    // Call the base class process member, this will call the derived templated process code
    //std::cout << "setupAndProcess! process\n";
    processor.process();
    //std::cout << "setupAndProcess! OK\n";
} // MeshWarpPlugin::setupAndProcess

// the internal render function
template <int nComponents>
void
MeshWarpPlugin::renderInternal(const RenderArguments &args,
                             BitDepthEnum dstBitDepth)
{
    switch (dstBitDepth) {
    case eBitDepthUByte: {
        MeshWarper<unsigned char, nComponents, 255> warper(*this);
        setupAndProcess(warper, args);
        break;
    }
    case eBitDepthUShort: {
        MeshWarper<unsigned short, nComponents, 65535> warper(*this);
        setupAndProcess(warper, args);
        break;
    }
    case eBitDepthFloat: {
        MeshWarper<float, nComponents, 1> warper(*this);
        setupAndProcess(warper, args);
        break;
    }
    default:
        throwSuiteStatusException(kOfxStatErrUnsupported);
    }
}

bool
MeshWarpPlugin::isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &/*identityTime*/
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
)
{
    /*
    double mix;
    _mix->getValueAtTime(args.time, mix);

    if (mix == 0.) {
        identityClip = _srcClip;
        return true;
    }

    string expr1, expr2, exprR,  exprG,  exprB,  exprA;
    _expr1->getValue(expr1);
    _expr2->getValue(expr2);
    _exprR->getValue(exprR);
    _exprG->getValue(exprG);
    _exprB->getValue(exprB);
    _exprA->getValue(exprA);
    RGBAValues param1;
    _param1->getValueAtTime(args.time, param1.r, param1.g, param1.b, param1.a);
    if (exprR.empty() && exprG.empty() && exprB.empty() && exprA.empty()) {
        identityClip = _srcClip;
        return true;
    }
    */
    return false;
}

void
MeshWarpPlugin::changedClip(const InstanceChangedArgs &args, const std::string &clipName)
{
    /*
    if (clipName == kOfxImageEffectSimpleSourceClipName && _srcClip && args.reason == OFX::eChangeUserEdit) {
        switch (_srcClip->getPreMultiplication()) {
            case eImageOpaque:
                _premult->setValue(false);
                break;
            case eImagePreMultiplied:
                _premult->setValue(true);
                break;
            case eImageUnPreMultiplied:
                _premult->setValue(false);
                break;
        }
    }
    */
}

mDeclarePluginFactory(MeshWarpPluginFactory, {}, {});

class MeshWarpInteract
    : public OverlayInteract
{
public:
    MeshWarpInteract(OfxInteractHandle handle, OFX::ImageEffect* effect);

    virtual bool draw(const DrawArgs &args);

    virtual bool penDown(const PenArgs &args) OVERRIDE FINAL {
        printf("TEST\n");
        return true;
    }

private:
    Double2DParam* _from001;
    Double2DParam* _to001;
};

class MeshWarpOverlayDescriptor
    : public DefaultEffectOverlayDescriptor<MeshWarpOverlayDescriptor, MeshWarpInteract>
{
};

void MeshWarpPluginFactory::describe(ImageEffectDescriptor &desc)
{
    // basic labels
    const ImageEffectHostDescription &gHostDescription = *getImageEffectHostDescription();
    desc.setLabel(kPluginName);
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(kPluginDescription);

    desc.addSupportedContext(eContextFilter);
    desc.addSupportedContext(eContextGeneral);
    desc.addSupportedContext(eContextPaint);
    desc.addSupportedBitDepth(eBitDepthUByte);
    desc.addSupportedBitDepth(eBitDepthUShort);
    desc.addSupportedBitDepth(eBitDepthFloat);
    
    // set a few flags
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(kSupportsMultiResolution);
    desc.setSupportsTiles(kSupportsTiles);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);
    desc.setSupportsMultipleClipDepths(kSupportsMultipleClipDepths);
    desc.setRenderThreadSafety(kRenderThreadSafety);

    desc.setOverlayInteractDescriptor(new MeshWarpOverlayDescriptor);
}

void MeshWarpPluginFactory::describeInContext(ImageEffectDescriptor &desc, ContextEnum context)
{
    // Source clip only in the filter context
    // create the mandated source clip
    ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setIsMask(false);
    
    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setSupportsTiles(kSupportsTiles);
    
    if (context == eContextGeneral || context == eContextPaint) {
        ClipDescriptor *maskClip = context == eContextGeneral ? desc.defineClip("Mask") : desc.defineClip("Brush");
        maskClip->addSupportedComponent(ePixelComponentAlpha);
        maskClip->setTemporalClipAccess(false);
        if (context == eContextGeneral)
            maskClip->setOptional(true);
        maskClip->setSupportsTiles(kSupportsTiles);
        maskClip->setIsMask(true);
    }

    PageParamDescriptor *page = desc.definePageParam("Controls");
    {
        Double2DParamDescriptor* param = desc.defineDouble2DParam("from001");
        param->setLabel("From 001");
        param->setHint("From position");
        param->setDefault(0, 0);
        param->setLayoutHint(eLayoutHintNoNewLine, 1);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        Double2DParamDescriptor* param = desc.defineDouble2DParam("to001");
        param->setLabel("To 001");
        param->setHint("To position");
        param->setDefault(0, 0);
        if (page) {
            page->addChild(*param);
        }
    }
}

OFX::ImageEffect* MeshWarpPluginFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum /*context*/)
{
    return new MeshWarpPlugin(handle);
}

static MeshWarpPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)

MeshWarpInteract::MeshWarpInteract(OfxInteractHandle handle, OFX::ImageEffect* effect)
    : OFX::OverlayInteract(handle)
{
    _from001 = effect->fetchDouble2DParam("from001");
    _to001 = effect->fetchDouble2DParam("to001");
    addParamToSlaveTo(_from001);
    addParamToSlaveTo(_to001);
}

bool
MeshWarpInteract::draw(const DrawArgs &args)
{
    GLdouble projection[16];
    glGetDoublev( GL_PROJECTION_MATRIX, projection);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    OfxPointD box_half;
    box_half.x = 8. / (projection[0] * viewport[2]);
    box_half.y = 8. / (projection[5] * viewport[3]);

    OfxRGBColourD from_color = {0.5, 1, 0.5};
    OfxRGBColourD to_color = {1, 0.5, 0.5};

    getSuggestedColour(from_color);
    getSuggestedColour(to_color);

    // shadow (uses GL_PROJECTION)
    glMatrixMode(GL_PROJECTION);
    glMatrixMode(GL_MODELVIEW); // Modelview should be used on Nuke

    OfxPointD from001 = _from001->getValueAtTime(args.time);
    OfxPointD to001 = _to001->getValueAtTime(args.time);

    glEnable(GL_LINE_STIPPLE);    

    glLineStipple(1, 0x3333);
    glBegin(GL_LINES);
    glColor3f((float)from_color.r, (float)from_color.g, (float)from_color.b);
    glVertex2d(from001.x, from001.y);
    glColor3f((float)to_color.r, (float)to_color.g, (float)to_color.b);
    glVertex2d(to001.x, to001.y);
    glEnd();

    glLineStipple(1, 0xFFFF);

    glBegin(GL_LINE_LOOP);
    glColor3f((float)from_color.r, (float)from_color.g, (float)from_color.b);
    glVertex2d(from001.x - box_half.x, from001.y - box_half.y);
    glVertex2d(from001.x + box_half.x, from001.y - box_half.y);
    glVertex2d(from001.x + box_half.x, from001.y + box_half.y);
    glVertex2d(from001.x - box_half.x, from001.y + box_half.y);
    glEnd();

    glBegin(GL_LINE_LOOP);
    glColor3f((float)to_color.r, (float)to_color.g, (float)to_color.b);
    glVertex2d(to001.x - box_half.x, to001.y - box_half.y);
    glVertex2d(to001.x + box_half.x, to001.y - box_half.y);
    glVertex2d(to001.x + box_half.x, to001.y + box_half.y);
    glVertex2d(to001.x - box_half.x, to001.y + box_half.y);
    glEnd();

    return true;
} // draw
