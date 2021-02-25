#include "PatchMatchPluginFactory.h"
#include "PatchMatchPlugin.h"

void PatchMatchPluginFactory::describe(ImageEffectDescriptor &desc)
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
    desc.setRenderThreadSafety(eRenderFullySafe);
}

void PatchMatchPluginFactory::describeInContext(ImageEffectDescriptor &desc, ContextEnum context)
{
    // create the mandated source clip
    ClipDescriptor *srcClip = desc.defineClip(kSourceClip);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(true);
    srcClip->setIsMask(false);

    // create the mandated target clip
    ClipDescriptor *trgClip = desc.defineClip(kTargetClip);
    trgClip->addSupportedComponent(ePixelComponentRGBA);
    trgClip->addSupportedComponent(ePixelComponentRGB);
    trgClip->addSupportedComponent(ePixelComponentAlpha);
    trgClip->setTemporalClipAccess(false);
    trgClip->setSupportsTiles(true);
    trgClip->setIsMask(false);

    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->setSupportsTiles(true);

    // create the optional initial clip
    ClipDescriptor *initClip = desc.defineClip(kInitialClip);
    initClip->setOptional(true);
    initClip->addSupportedComponent(ePixelComponentRGBA);
    initClip->addSupportedComponent(ePixelComponentRGB);
    initClip->addSupportedComponent(ePixelComponentAlpha);
    initClip->setTemporalClipAccess(false);
    initClip->setSupportsTiles(true);
    initClip->setIsMask(false);

    PageParamDescriptor *page = desc.definePageParam("Controls");
    {
        IntParamDescriptor* param = desc.defineIntParam(kParamPatchSize);
        param->setLabel(kParamPatchSizeLabel);
        param->setHint(kParamPatchSizeHint);
        param->setDefault(5);
        param->setDisplayRange(3, 11);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        IntParamDescriptor* param = desc.defineIntParam(kParamStartLevel);
        param->setLabel(kParamStartLevelLabel);
        param->setHint(kParamStartLevelHint);
        param->setDefault(1);
        param->setDisplayRange(1, 10);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        IntParamDescriptor* param = desc.defineIntParam(kParamEndLevel);
        param->setLabel(kParamEndLevelLabel);
        param->setHint(kParamEndLevelHint);
        param->setDefault(1);
        param->setDisplayRange(1, 10);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineDoubleParam(kParamIterations);
        param->setLabel(kParamIterationsLabel);
        param->setHint(kParamIterationsHint);
        param->setDefault(0);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineDoubleParam(kParamAcceptableScore);
        param->setLabel(kParamAcceptableScoreLabel);
        param->setHint(kParamAcceptableScoreHint);
        param->setDefault(kParamAcceptableScoreDefault);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        IntParamDescriptor* param = desc.defineIntParam(kParamRandomSeed);
        param->setLabel(kParamRandomSeedLabel);
        param->setHint(kParamRandomSeedHint);
        param->setDefault(0);
        if (page) {
            page->addChild(*param);
        }
    }

    PageParamDescriptor *pageAnal = desc.definePageParam("Analysis");
    {
        auto param = desc.defineInt2DParam(kParamLogCoords);
        param->setLabel(kParamLogCoordsLabel);
        param->setHint(kParamLogCoordsHint);
        param->setDefault(-1, -1);
        if (pageAnal) {
            pageAnal->addChild(*param);
        }
    }
}

ImageEffect* PatchMatchPluginFactory::createInstance(OfxImageEffectHandle handle, ContextEnum /*context*/)
{
    return new PatchMatchPlugin(handle);
}

static PatchMatchPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)
