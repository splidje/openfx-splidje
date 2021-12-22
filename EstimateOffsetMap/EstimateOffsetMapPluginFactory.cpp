#include "EstimateOffsetMapPluginFactory.h"
#include "EstimateOffsetMapPlugin.h"
#include "EstimateOffsetMapPluginInteract.h"


void EstimateOffsetMapPluginFactory::describe(ImageEffectDescriptor &desc)
{
    // basic labels
    const ImageEffectHostDescription &gHostDescription = *getImageEffectHostDescription();
    desc.setLabel(kPluginName);
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(kPluginDescription);

    desc.addSupportedContext(eContextFilter);
    desc.addSupportedContext(eContextGeneral);
    desc.addSupportedContext(eContextPaint);
    desc.addSupportedBitDepth(eBitDepthFloat);
    
    // set a few flags
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(true);
    desc.setSupportsTiles(true);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(true);
    desc.setSupportsMultipleClipDepths(false);
    desc.setRenderThreadSafety(eRenderUnsafe);

    desc.setOverlayInteractDescriptor(new EstimateOffsetMapPluginOverlayDescriptor);
}

void EstimateOffsetMapPluginFactory::describeInContext(ImageEffectDescriptor &desc, ContextEnum context)
{
    // create the mandated source clip
    ClipDescriptor *srcClip = desc.defineClip(kSourceClip);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(true);
    srcClip->setIsMask(false);

    // create the optional target clip
    ClipDescriptor *trgClip = desc.defineClip(kTargetClip);
    trgClip->setOptional(true);
    trgClip->addSupportedComponent(ePixelComponentRGBA);
    trgClip->addSupportedComponent(ePixelComponentRGB);
    trgClip->addSupportedComponent(ePixelComponentAlpha);
    trgClip->setTemporalClipAccess(false);
    trgClip->setSupportsTiles(true);
    trgClip->setIsMask(false);

    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->setTemporalClipAccess(false);
    dstClip->setSupportsTiles(true);

    // Controls
    {
        auto page = desc.definePageParam("Controls");
        {
            auto param = desc.defineBooleanParam(kParamBlackOutside);
            param->setDefault(true);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.defineDoubleParam(kParamMinRadius);
            param->setDefault(5);
            param->setRange(0, INT_MAX);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.defineDoubleParam(kParamMaxRadius);
            param->setDefault(20);
            param->setRange(0, INT_MAX);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.defineDoubleParam(kParamMinRotate);
            param->setDefault(-30);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.defineDoubleParam(kParamMaxRotate);
            param->setDefault(30);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.defineDoubleParam(kParamMinScale);
            param->setDefault(0.5);
            param->setRange(0, INT_MAX);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.defineDoubleParam(kParamMaxScale);
            param->setDefault(2);
            param->setRange(0, INT_MAX);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.defineDoubleParam(kParamMaxTranslate);
            param->setDefault(50);
            param->setRange(0, INT_MAX);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.defineIntParam(kParamIterations);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.defineIntParam(kParamSeed);
            if (page) {
                page->addChild(*param);
            }
        }
    }
}

ImageEffect* EstimateOffsetMapPluginFactory::createInstance(OfxImageEffectHandle handle, ContextEnum /*context*/)
{
    return new EstimateOffsetMapPlugin(handle);
}

static EstimateOffsetMapPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)
