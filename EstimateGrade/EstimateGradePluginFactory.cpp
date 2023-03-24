#include "EstimateGradePluginFactory.h"
#include "EstimateGradePlugin.h"

void EstimateGradePluginFactory::describe(ImageEffectDescriptor &desc)
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
    desc.setRenderThreadSafety(eRenderInstanceSafe);
    desc.setSequentialRender(false);
}

void EstimateGradePluginFactory::describeInContext(ImageEffectDescriptor &desc, ContextEnum context)
{
    // create the mandated source clip
    ClipDescriptor *srcClip = desc.defineClip(kSourceClip);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(true);
    srcClip->setIsMask(false);

    // create the mandated offsets clip
    ClipDescriptor *trgClip = desc.defineClip(kTargetClip);
    trgClip->addSupportedComponent(ePixelComponentRGB);
    trgClip->addSupportedComponent(ePixelComponentRGBA);
    trgClip->addSupportedComponent(ePixelComponentAlpha);
    trgClip->setTemporalClipAccess(false);
    trgClip->setSupportsTiles(true);
    trgClip->setIsMask(false);

    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setTemporalClipAccess(false);
    dstClip->setSupportsTiles(true);

    PageParamDescriptor *page = desc.definePageParam("Controls");
    {
        auto param = desc.defineIntParam(kParamIterations);
        param->setLabel(kParamIterationsLabel);
        param->setHint(kParamIterationsHint);
        param->setDefault(1000);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.definePushButtonParam(kParamEstimate);
        param->setLabel(kParamEstimateLabel);
        param->setHint(kParamEstimateHint);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamCentrePoint);
        param->setLabel(kParamCentrePointLabel);
        param->setHint(kParamCentrePointHint);
        param->setDefault(0.5, 0.5, 0.5, 0.5);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamSlope);
        param->setLabel(kParamSlopeLabel);
        param->setHint(kParamCentrePointHint);
        param->setDefault(1, 1, 1, 1);
        if (page) {
            page->addChild(*param);
        }
    }
}

ImageEffect* EstimateGradePluginFactory::createInstance(OfxImageEffectHandle handle, ContextEnum /*context*/)
{
    return new EstimateGradePlugin(handle);
}

static EstimateGradePluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)
