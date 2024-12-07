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

    // create the optional target clip
    ClipDescriptor *trgClip = desc.defineClip(kTargetClip);
    trgClip->addSupportedComponent(ePixelComponentRGB);
    trgClip->addSupportedComponent(ePixelComponentRGBA);
    trgClip->addSupportedComponent(ePixelComponentAlpha);
    trgClip->setTemporalClipAccess(false);
    trgClip->setSupportsTiles(true);
    trgClip->setIsMask(false);
    trgClip->setOptional(true);

    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setTemporalClipAccess(false);
    dstClip->setSupportsTiles(true);

    PageParamDescriptor *page = desc.definePageParam("Controls");
    {
        auto param = desc.defineChoiceParam(kParamMapping);
        param->setLabel(kParamMappingLabel);
        param->setHint(kParamMappingHint);
        param->appendOption("Gamma");
        param->appendOption("S-Curve");
        param->appendOption("3-Point-Curve");
        param->appendOption("Matrix");
        param->appendOption("Cube");
        param->setAnimates(false);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineIntParam(kParamSamples);
        param->setLabel(kParamSamplesLabel);
        param->setHint(kParamSamplesHint);
        param->setDefault(1000);
        if (page) {
            page->addChild(*param);
        }
    }
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
        auto param = desc.defineRGBAParam(kParamBlackPoint);
        param->setLabel(kParamBlackPointLabel);
        param->setHint(kParamBlackPointHint);
        param->setDefault(0, 0, 0, 0);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamWhitePoint);
        param->setLabel(kParamWhitePointLabel);
        param->setHint(kParamWhitePointHint);
        param->setDefault(1, 1, 1, 1);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamCentrePoint);
        param->setLabel(kParamCentrePointLabel);
        param->setHint(kParamCentrePointHint);
        param->setDefault(0.5, 0.5, 0.5, 0.5);
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamSlope);
        param->setLabel(kParamSlopeLabel);
        param->setHint(kParamSlopeHint);
        param->setDefault(1, 1, 1, 1);
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamGamma);
        param->setLabel(kParamGammaLabel);
        param->setHint(kParamGammaHint);
        param->setDefault(1, 1, 1, 1);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamMatrixRed);
        param->setLabel(kParamMatrixRedLabel);
        param->setHint(kParamMatrixRedHint);
        param->setDefault(1, 0, 0, 0);
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamMatrixGreen);
        param->setLabel(kParamMatrixGreenLabel);
        param->setHint(kParamMatrixGreenHint);
        param->setDefault(0, 1, 0, 0);
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamMatrixBlue);
        param->setLabel(kParamMatrixBlueLabel);
        param->setHint(kParamMatrixBlueHint);
        param->setDefault(0, 0, 1, 0);
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamMatrixAlpha);
        param->setLabel(kParamMatrixAlphaLabel);
        param->setHint(kParamMatrixAlphaHint);
        param->setDefault(0, 0, 0, 1);
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamX1);
        param->setDefault(0, 0, 0, 0);
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamY1);
        param->setDefault(0, 0, 0, 0);
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamSlope1);
        param->setDefault(1, 1, 1, 1);
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamX2);
        param->setDefault(0.5, 0.5, 0.5, 0.5);
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamY2);
        param->setDefault(0.5, 0.5, 0.5, 0.5);
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamX3);
        param->setDefault(1, 1, 1, 1);
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamY3);
        param->setDefault(1, 1, 1, 1);
        param->setIsSecretAndDisabled(true);
        if (page) {
            page->addChild(*param);
        }
    }
    {
        auto param = desc.defineRGBAParam(kParamSlope3);
        param->setDefault(1, 1, 1, 1);
        param->setIsSecretAndDisabled(true);
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
