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
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(true);
    desc.setSupportsMultipleClipDepths(false);
    desc.setRenderThreadSafety(eRenderInstanceSafe);
    desc.setSequentialRender(false);
}

void OffsetMapPluginFactory::describeInContext(ImageEffectDescriptor &desc, ContextEnum context)
{
    // create the mandated offsets clip
    ClipDescriptor *offClip = desc.defineClip(kOffsetsClip);
    offClip->addSupportedComponent(ePixelComponentRGB);
    offClip->addSupportedComponent(ePixelComponentAlpha);
    offClip->setTemporalClipAccess(false);
    offClip->setSupportsTiles(true);
    offClip->setIsMask(false);

    // create the mandated source clip
    ClipDescriptor *srcClip = desc.defineClip(kSourceClip);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
#ifdef OFX_EXTENSIONS_NATRON
    srcClip->addSupportedComponent(ePixelComponentXY);
#endif
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(true);
    srcClip->setIsMask(false);

    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
#ifdef OFX_EXTENSIONS_NATRON
    dstClip->addSupportedComponent(ePixelComponentXY);
#endif
    dstClip->setTemporalClipAccess(false);
    dstClip->setSupportsTiles(true);
}

ImageEffect* OffsetMapPluginFactory::createInstance(OfxImageEffectHandle handle, ContextEnum /*context*/)
{
    return new OffsetMapPlugin(handle);
}

static OffsetMapPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)
