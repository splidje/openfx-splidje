#include "FaceTranslationMapPluginFactory.h"
#include "FaceTranslationMapPlugin.h"
#include "FaceTranslationMapPluginInteract.h"

void FaceTranslationMapPluginFactory::describe(ImageEffectDescriptor &desc)
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

    desc.setOverlayInteractDescriptor(new FaceTranslationMapPluginOverlayDescriptor);
}

void FaceTranslationMapPluginFactory::describeInContext(ImageEffectDescriptor &desc, ContextEnum context)
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
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    // dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setTemporalClipAccess(false);
    dstClip->setSupportsTiles(true);

    // Controls
    {
        auto page = desc.definePageParam("Controls");
        {
            auto param = desc.definePushButtonParam(kParamTrackSource);
            param->setLabel(kParamTrackSourceLabel);
            param->setHint(kParamTrackSourceHint);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.definePushButtonParam(kParamTrackSourceAll);
            param->setLabel(kParamTrackSourceAllLabel);
            param->setHint(kParamTrackSourceAllHint);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.defineInt2DParam(kParamSourceNoiseProfileRange);
            param->setLabel(kParamSourceNoiseProfileRangeLabel);
            param->setHint(kParamSourceNoiseProfileRangeHint);
            param->setAnimates(false);
            param->setDefault(1, 10);
            param->setDimensionLabels("first", "last");
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.definePushButtonParam(kParamRemoveSourceeNoise);
            param->setLabel(kParamRemoveSourceNoiseLabel);
            param->setHint(kParamRemoveSourceNoiseHint);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.definePushButtonParam(kParamTrackTarget);
            param->setLabel(kParamTrackTargetLabel);
            param->setHint(kParamTrackTargetHint);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.definePushButtonParam(kParamTrackTargetAll);
            param->setLabel(kParamTrackTargetAllLabel);
            param->setHint(kParamTrackTargetAllHint);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.defineIntParam(kParamReferenceFrame);
            param->setLabel(kParamReferenceFrameLabel);
            param->setHint(kParamReferenceFrameHint);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.definePushButtonParam(kParamCalculateRelative);
            param->setLabel(kParamCalculateRelativeLabel);
            param->setHint(kParamCalculateRelativeHint);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.definePushButtonParam(kParamCalculateRelativeAll);
            param->setLabel(kParamCalculateRelativeAllLabel);
            param->setHint(kParamCalculateRelativeAllHint);
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.defineChoiceParam(kParamOutput);
            param->setLabel(kParamOutputLabel);
            param->setHint(kParamOutputHint);
            param->appendOption(
                kParamOutputChoiceSourceLabel
                ,kParamOutputChoiceSourceHint
                ,kParamOutputChoiceSource
            );
            param->appendOption(
                kParamOutputChoiceTargetLabel
                ,kParamOutputChoiceTargetHint
                ,kParamOutputChoiceTarget
            );
            param->appendOption(
                kParamOutputChoiceUVMapLabel
                ,kParamOutputChoiceUVMapHint
                ,kParamOutputChoiceUVMap
            );
            param->appendOption(
                kParamOutputChoiceTranslationMapLabel
                ,kParamOutputChoiceTranslationMapHint
                ,kParamOutputChoiceTranslationMap
            );
            if (page) {
                page->addChild(*param);
            }
        }
        {
            auto param = desc.defineDoubleParam(kParamFeather);
            param->setLabel(kParamFeatherLabel);
            param->setHint(kParamFeatherHint);
            param->setDefault(10);
            if (page) {
                page->addChild(*param);
            }
        }
    }

    // Source Face
    {
        auto page = desc.definePageParam("Source Face");
        {
            auto param = desc.definePushButtonParam(kParamClearSourceKeyframeAll);
            param->setLabel(kParamClearSourceKeyframeAllLabel);
            param->setHint(kParamClearSourceKeyframeAllHint);
            if (page) {
                page->addChild(*param);
            }
        }
        FaceTrackPluginBase::defineFaceParams(&desc, page, kFaceParamsPrefixSource);
    }

    // Target Face
    {
        auto page = desc.definePageParam("Target Face");
        FaceTrackPluginBase::defineFaceParams(&desc, page, kFaceParamsPrefixTarget);
    }

    // Relative Face
    {
        auto page = desc.definePageParam("Relative Face");
        FaceTrackPluginBase::defineFaceParams(&desc, page, kFaceParamsPrefixRelative);
    }
}

ImageEffect* FaceTranslationMapPluginFactory::createInstance(OfxImageEffectHandle handle, ContextEnum /*context*/)
{
    return new FaceTranslationMapPlugin(handle);
}

static FaceTranslationMapPluginFactory p(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor);
mRegisterPluginFactoryInstance(p)
