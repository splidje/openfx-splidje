#define kPluginName "BoneWarp"
#define kPluginGrouping "Transform"
#define kPluginDescription \
"Bone Warp"

#define kPluginIdentifier "com.ajptechnical.openfx.BoneWarp"
#define kPluginVersionMajor 0
#define kPluginVersionMinor 1

#define kSupportsTiles 1
#define kSupportsMultiResolution 1
#define kSupportsRenderScale 1
#define kSupportsMultipleClipPARs false
#define kSupportsMultipleClipDepths false
#define kRenderThreadSafety eRenderFullySafe

#define kSourceClip "Source"

#define kParamJointFromCentre(i) "joint" + std::to_string(i + 1) + "FromCentre"

#define kParamJointToCentre(i) "joint" + std::to_string(i + 1) + "ToCentre"

#define kParamJointRadius(i) "joint" + std::to_string(i + 1) + "Radius"
