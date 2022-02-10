#include "OffsetMapPluginFactory.h"
#include "OffsetMapPlugin.h"

void OffsetMapPluginFactory::describe(ImageEffectDescriptor &desc)
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
    desc.setTemporalClipAccess(true);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(true);
    desc.setSupportsMultipleClipDepths(false);
    desc.setRenderThreadSafety(eRenderInstanceSafe);
    desc.setSequentialRender(true);
}

void OffsetMapPluginFactory::describeInContext(ImageEffectDescriptor &desc, ContextEnum context)
{
    // create the mandated offsets clip
    ClipDescriptor *offClip = desc.defineClip(kOffsetsClip);
    offClip->addSupportedComponent(ePixelComponentRGB);
    offClip->setTemporalClipAccess(true);
    offClip->setSupportsTiles(true);
    offClip->setIsMask(false);

    // create the mandated source clip
    ClipDescriptor *srcClip = desc.defineClip(kSourceClip);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    srcClip->setTemporalClipAccess(true);
    srcClip->setSupportsTiles(true);
    srcClip->setIsMask(false);

    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setTemporalClipAccess(true);
    dstClip->setSupportsTiles(true);

    PageParamDescriptor *page = desc.definePageParam("Controls");
    {
        auto param = desc.defineBooleanParam(kParamIterateTemporally);
        param->setLabel(kParamIterateTemporallyLabel);
        param->setHint(kParamIterateTemporallyHint);
        param->setDefault(false);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineIntParam(kParamReferenceFrame);
        param->setLabel(kParamReferenceFrameLabel);
        param->setHint(kParamReferenceFrameHint);
        param->setDefault(1);
        if (page) {
            page->addChild(*param);
        }
    }
}

ImageEffect* OffsetMapPluginFactory::createInstance(OfxImageEffectHandle handle, ContextEnum /*context*/)
{
    return new OffsetMapPlugin(handle);
}

static OffsetMapPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)
