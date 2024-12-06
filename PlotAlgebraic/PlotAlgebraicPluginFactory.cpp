#include "PlotAlgebraicPluginFactory.h"
#include "PlotAlgebraicPlugin.h"

void PlotAlgebraicPluginFactory::describe(ImageEffectDescriptor &desc)
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

void PlotAlgebraicPluginFactory::describeInContext(ImageEffectDescriptor &desc, ContextEnum context)
{
    // create the mandated source clip
    ClipDescriptor *srcClip = desc.defineClip(kSourceClip);
    srcClip->setOptional(true);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(true);
    srcClip->setIsMask(false);

    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setTemporalClipAccess(false);
    dstClip->setSupportsTiles(true);

    PageParamDescriptor *page = desc.definePageParam("Controls");
    {
        auto param = desc.defineIntParam(kParamRandomSeed);
        param->setLabel(kParamRandomSeedLabel);
        if (page) {
            page->addChild(*param);
        }
    }

    {
        auto param = desc.defineIntParam(kParamMaxCoeff);
        param->setLabel(kParamMaxCoeffLabel);
        param->setDefault(1023);
        if (page) {
            page->addChild(*param);
        }
    }

    {
        auto param = desc.defineDouble2DParam(kParamMaxRoot);
        param->setLabel(kParamMaxRootLabel);
        param->setDefault(10, 10);
        if (page) {
            page->addChild(*param);
        }
    }

    {
        auto param = desc.defineIntParam(kParamNumIters);
        param->setLabel(kParamNumItersLabel);
        if (page) {
            page->addChild(*param);
        }
    }
}

ImageEffect* PlotAlgebraicPluginFactory::createInstance(OfxImageEffectHandle handle, ContextEnum /*context*/)
{
    return new PlotAlgebraicPlugin(handle);
}

static PlotAlgebraicPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)
