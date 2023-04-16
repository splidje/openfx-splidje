#include "ofxsImageEffect.h"
#include "ofxsMacros.h"
#include "tetgen.h"
#include <eigen3/Eigen/Dense>
#include <set>

using namespace OFX;

#define kPluginName "EstimateCubeLUT"
#define kPluginGrouping "Color"
#define kPluginDescription \
"EstimateCubeLUT"

#define kPluginIdentifier "com.ajptechnical.splidje.openfx.EstimateCubeLUT"
#define kPluginVersionMajor 0
#define kPluginVersionMinor 1

#define kSourceClip "Source"
#define kTargetClip "Target"

#define kParamFile "file"

#define kParamWriteCubeSize "writeCubeSize"

#define kParamEstimate "estimate"

#define TETRA_GRID_SIZE 31


class EstimateCubeLUTPlugin : public ImageEffect
{
public:
    EstimateCubeLUTPlugin(OfxImageEffectHandle handle);

private:
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args) OVERRIDE FINAL;

    virtual bool isIdentity(const IsIdentityArguments &args, Clip * &identityClip, double &identityTime
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
    ) OVERRIDE FINAL;

    virtual void changedParam(const InstanceChangedArgs &args, const std::string &paramName);

    void readFile();

    void estimate(double time);

private:
    Clip* _srcClip;
    Clip* _trgClip;
    Clip* _dstClip;
    StringParam* _file;
    IntParam* _writeCubeSize;
    PushButtonParam* _estimate;
    int _cubeSize;
    std::map<OfxRGBColourF, OfxRGBColourF> _cubeLUT;
    std::set<int> _cubeLUTFromTetraGrid[TETRA_GRID_SIZE * TETRA_GRID_SIZE * TETRA_GRID_SIZE];
    std::vector<std::pair<Eigen::Vector3f, Eigen::Vector3f>> _cubeLUTFromTetraBounds;
    std::vector<Eigen::Matrix4f> _cubeLUTFromTetraMatrices;
    std::vector<std::array<Eigen::Vector3f, 4>> _cubeLUTToTetras;
};
