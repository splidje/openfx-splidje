#include "PlotAlgebraicPlugin.h"
#include "ofxsCoords.h"
#include <cmath>
#include <set>




PlotAlgebraicPlugin::PlotAlgebraicPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
{
    _srcClip = fetchClip(kSourceClip);
    assert(
        _srcClip && (
            _srcClip->getPixelComponents() == ePixelComponentRGB
            || _srcClip->getPixelComponents() == ePixelComponentRGBA
            || _srcClip->getPixelComponents() == ePixelComponentAlpha
        )
    );
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    assert(
        _dstClip && (
            _dstClip->getPixelComponents() == ePixelComponentRGB
            || _dstClip->getPixelComponents() == ePixelComponentRGBA
            || _dstClip->getPixelComponents() == ePixelComponentAlpha
        )
    );
    _randomSeed = fetchIntParam(kParamRandomSeed);
    _maxCoeff = fetchIntParam(kParamMaxCoeff);
    _maxRoot = fetchDouble2DParam(kParamMaxRoot);
    _numIters = fetchIntParam(kParamNumIters);
}

inline int gcd (int a, int b)
{
    while (b != 0) {
        auto t = b;
        b = a % b;
        a = t;
    } 
    return a;
}

inline int coeffsGcd(Eigen::Matrix<double, NUM_COEFFS, 1> &coeffs) {
    int gcdResult = 0;
    for (auto coeff : coeffs) {
        if (!coeff) {continue;}
        if (!gcdResult) {gcdResult = coeff;}
        else {gcdResult = gcd(gcdResult, coeff);}
    }
    return gcdResult;
}

inline void randomCoeffs(int maxCoeff, Eigen::Matrix<double, NUM_COEFFS, 1> &coeffs) {
    for (auto c = 0; c < NUM_COEFFS - 1; c++) {
        coeffs[c] = (rand() % ((maxCoeff << 1) + 1)) - maxCoeff;
    }
    coeffs[NUM_COEFFS - 1] = (rand() % (maxCoeff << 1)) - maxCoeff;
    if (!coeffs[NUM_COEFFS - 1]) {
        coeffs[NUM_COEFFS - 1] = maxCoeff;
    }
    coeffs /= coeffsGcd(coeffs);
}

bool coeffsLess(const Eigen::Matrix<double, NUM_COEFFS, 1> lhs, const Eigen::Matrix<double, NUM_COEFFS, 1> rhs) {
    for (int i = 0; i < NUM_COEFFS; i++) {
        if (lhs[i] < rhs[i]) {return true;}
        if (lhs[i] > rhs[i]) {return false;}
    }
    return false;
}

void PlotAlgebraicPlugin::render(const RenderArguments &args)
{
    std::unique_ptr<Image> dstImg(_dstClip->fetchImage(args.time));

    std::set<
        Eigen::Matrix<double, NUM_COEFFS, 1>, bool(*)(const Eigen::Matrix<double, NUM_COEFFS, 1>, const Eigen::Matrix<double, NUM_COEFFS, 1>)
    > visited(coeffsLess);

    auto numComponents = _dstClip->getPixelComponentCount();
    auto rod = _dstClip->getRegionOfDefinition(args.time);
    auto width = rod.x2 - rod.x1;
    auto height = rod.y2 - rod.y1;

    auto maxCoeff = _maxCoeff->getValueAtTime(args.time);
    auto maxRoot = _maxRoot->getValueAtTime(args.time);
    auto numIters = _numIters->getValueAtTime(args.time);

    for (auto y = args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        auto PIX = (float*)dstImg->getPixelAddress(args.renderWindow.x1, y);
        for (auto x = args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            for (auto c = 0; c < numComponents; c++, PIX++) {
                *PIX = c == 3;
            }
        }
    }

    for (auto i = 0; i < numIters; i++) {
        do {
            randomCoeffs(maxCoeff, _coeffs);
        } while (visited.count(_coeffs));
        visited.insert(_coeffs);
        _solver.compute(_coeffs);
        for (auto root : _solver.roots()) {
            auto x = args.renderScale.x * width * (root.real() + maxRoot.x) / (maxRoot.x * 2);
            auto y = args.renderScale.y * height * (root.imag() + maxRoot.y) / (maxRoot.y * 2);
            if (x >= args.renderWindow.x1 && x < args.renderWindow.x2 && y >= args.renderWindow.y1 && y < args.renderWindow.y2) {
                auto PIX = (float*)dstImg->getPixelAddress(x, y);
                for (auto c = 0; c < std::max(3, numComponents); c++, PIX++) {
                    *PIX += 1 / _coeffs.norm();
                }
            }
        }
    }
}
