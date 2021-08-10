#include "FaceTrackPluginFactory.h"
#include "FaceTrackPlugin.h"
#include "FaceTrackPluginInteract.h"

void FaceTrackPluginFactory::describe(ImageEffectDescriptor &desc)
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
    desc.setRenderThreadSafety(eRenderUnsafe);

    desc.setOverlayInteractDescriptor(new FaceTrackOverlayDescriptor);
}

void FaceTrackPluginFactory::describeInContext(ImageEffectDescriptor &desc, ContextEnum context)
{
    // create the mandated source clip
    ClipDescriptor *srcClip = desc.defineClip(kSourceClip);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->addSupportedComponent(ePixelComponentAlpha);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(true);
    srcClip->setIsMask(false);

    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setTemporalClipAccess(false);
    dstClip->setSupportsTiles(true);

    // Controls
    {
        auto page = desc.definePageParam("Controls");
        {
            auto param = desc.definePushButtonParam(kParamTrack);
            param->setLabel(kParamTrackLabel);
            param->setHint(kParamTrackHint);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.definePushButtonParam(kParamTrackAll);
            param->setLabel(kParamTrackAllLabel);
            param->setHint(kParamTrackAllHint);
            if (page) {
                page->addChild(*param);
            }
        }
    }

    // Face
    {
        auto page = desc.definePageParam("Face");
        FaceTrackPluginBase::defineFaceParams(&desc, page, "");
    }
}

ImageEffect* FaceTrackPluginFactory::createInstance(OfxImageEffectHandle handle, ContextEnum /*context*/)
{
    return new FaceTrackPlugin(handle);
}

static FaceTrackPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)
