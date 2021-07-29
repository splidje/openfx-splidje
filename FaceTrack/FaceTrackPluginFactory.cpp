#include "FaceTrackPluginFactory.h"
#include "FaceTrackPlugin.h"
#include "FaceTrackInteract.h"

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
            auto param = desc.definePushButtonParam(kParamTrackForward);
            param->setLabel(kParamTrackForwardLabel);
            param->setHint(kParamTrackForwardHint);
            if (page) {
                page->addChild(*param);
            }
        }
    }
    
    // Face
    {
        auto page = desc.definePageParam("Face");
        {
            auto param = desc.defineDouble2DParam(kParamFaceBottomLeft);
            param->setLabel(kParamFaceBottomLeftLabel);
            param->setHint(kParamFaceBottomLeftHint);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.defineDouble2DParam(kParamFaceTopRight);
            param->setLabel(kParamFaceTopRightLabel);
            param->setHint(kParamFaceTopRightHint);
            if (page) {
                page->addChild(*param);
            }
        }
    }

    // Jaw
    {
        auto page = desc.definePageParam("Jaw");
        for (int i=0; i < kLandmarkCountJaw; i++) {
            auto param = desc.defineDouble2DParam(kParamJaw(i));
            if (page) {
                page->addChild(*param);
            }
        }
    }

    // Eyebrows
    {
        auto page = desc.definePageParam("Eyebrows");
        {
            for (int i=0; i < kLandmarkCountEyebrowRight; i++) {
                auto param = desc.defineDouble2DParam(kParamEyebrowRight(i));
                if (page) {
                    page->addChild(*param);
                }
            }
            for (int i=0; i < kLandmarkCountEyebrowLeft; i++) {
                auto param = desc.defineDouble2DParam(kParamEyebrowLeft(i));
                if (page) {
                    page->addChild(*param);
                }
            }
        }
    }

    // Nose
    {
        auto page = desc.definePageParam("Nose");
        {
            for (int i=0; i < kLandmarkCountNoseBridge; i++) {
                auto param = desc.defineDouble2DParam(kParamNoseBridge(i));
                if (page) {
                    page->addChild(*param);
                }
            }
            for (int i=0; i < kLandmarkCountNoseBottom; i++) {
                auto param = desc.defineDouble2DParam(kParamNoseBottom(i));
                if (page) {
                    page->addChild(*param);
                }
            }
        }
    }

    // Eyes
    {
        auto page = desc.definePageParam("Eyes");
        {
            for (int i=0; i < kLandmarkCountEyeRight; i++) {
                auto param = desc.defineDouble2DParam(kParamEyeRight(i));
                if (page) {
                    page->addChild(*param);
                }
            }
            for (int i=0; i < kLandmarkCountEyeLeft; i++) {
                auto param = desc.defineDouble2DParam(kParamEyeLeft(i));
                if (page) {
                    page->addChild(*param);
                }
            }
        }
    }

    // Mouth
    {
        auto page = desc.definePageParam("Mouth");
        {
            for (int i=0; i < kLandmarkCountMouthOutside; i++) {
                auto param = desc.defineDouble2DParam(kParamMouthOutside(i));
                if (page) {
                    page->addChild(*param);
                }
            }
            for (int i=0; i < kLandmarkCountMouthInside; i++) {
                auto param = desc.defineDouble2DParam(kParamMouthInside(i));
                if (page) {
                    page->addChild(*param);
                }
            }
        }
    }
}

ImageEffect* FaceTrackPluginFactory::createInstance(OfxImageEffectHandle handle, ContextEnum /*context*/)
{
    return new FaceTrackPlugin(handle);
}

static FaceTrackPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)
