#include "PlotAlgebraicPlugin.h"
#include "ofxsCoords.h"
#include <random>
#include <set>
#include <symengine/expression.h>
#include <symengine/solve.h>
#include <symengine/series.h>
#include <symengine/polys/basic_conversions.h>


PlotAlgebraicPlugin::PlotAlgebraicPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
{
    _destinationClip = fetchClip(kOfxImageEffectOutputClipName);
    assert(
        _destinationClip && (
            _destinationClip->getPixelComponents() == ePixelComponentRGB
            || _destinationClip->getPixelComponents() == ePixelComponentRGBA
            || _destinationClip->getPixelComponents() == ePixelComponentAlpha
        )
    );
    _maxLeadingCoefficient = fetchIntParam(kParamMaxCoefficient);
    _iterationCount = fetchIntParam(kParamIterationCount);
    _randomSeed = fetchIntParam(kParamRandomSeed);
    _generate = fetchPushButtonParam(kParamGenerate);
}

bool compareRoots(const SymEngine::RCP<const SymEngine::Basic> lhs, SymEngine::RCP<const SymEngine::Basic> rhs) {
    return lhs->hash() < rhs->hash();
}

void PlotAlgebraicPlugin::changedParam(const InstanceChangedArgs &args, const std::string &paramName) {
    if (paramName != kParamGenerate) {return;}

    _roots.clear();

    std::set<SymEngine::RCP<const SymEngine::Basic>, decltype(compareRoots)*> uniqueRoots(compareRoots);
    auto x = SymEngine::symbol("x");

    auto maxLeadingCoefficient = _maxLeadingCoefficient->getValue();
    auto iterationCount = _iterationCount->getValue();
    auto randomSeed = _randomSeed->getValue();

    std::mt19937 mersenneTwisterEngine(randomSeed);
    for (auto i=0; i < iterationCount; i++) {
        SymEngine::RCP<const SymEngine::Basic> polynomial = SymEngine::integer(0);
        for (auto c=0, maxCoefficient = maxLeadingCoefficient; c < COEFFICIENT_COUNT; c++, maxCoefficient *= maxLeadingCoefficient) {
            std::uniform_int_distribution<int> uniformDistribution(-maxCoefficient, maxCoefficient);
            polynomial = SymEngine::add(
                polynomial,
                SymEngine::mul(
                    SymEngine::integer(COEFFICIENT_FACTORS[c]),
                    SymEngine::mul(
                        SymEngine::integer(uniformDistribution(mersenneTwisterEngine)),
                        SymEngine::pow(x, SymEngine::integer(COEFFICIENT_COUNT - 1 - c))
                    )
                )
            );
        }
        // std::cout << *polynomial << std::endl;
        auto roots = SymEngine::solve_poly(polynomial, x);
        for (auto root : SymEngine::rcp_static_cast<const SymEngine::FiniteSet>(roots)->get_container()) {
            // std::cout << *root << std::endl;
            uniqueRoots.insert(root);
        }
    }

    for (auto blah : uniqueRoots) {
        _roots.push_back(SymEngine::eval_complex_double(*blah));
    }
}


void PlotAlgebraicPlugin::render(const RenderArguments &args)
{
    std::unique_ptr<Image> destinationImage(_destinationClip->fetchImage(args.time));

    auto componentCount = destinationImage->getPixelComponentCount();
    auto rod = destinationImage->getRegionOfDefinition();
    auto halfWidth = (rod.x2 - rod.x1) << 1;
    auto halfHeight = (rod.y2 - rod.y1) << 1;

    for (auto y = args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        auto PIX = (float*)destinationImage->getPixelAddress(args.renderWindow.x1, y);
        for (auto x = args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            for (auto c = 0; c < componentCount; c++, PIX++) {
                *PIX = c == 3;
            }
        }
    }

    auto maxLeadingCoefficient = _maxLeadingCoefficient->getValue();

    for (auto root : _roots) {
        auto real = root.real();
        auto imaginary = root.imag();
        if (real < -maxLeadingCoefficient || real >= maxLeadingCoefficient || imaginary < -maxLeadingCoefficient || imaginary >= maxLeadingCoefficient) {
            continue;
        }
        auto x = (int)round(halfWidth * root.real() / maxLeadingCoefficient) + rod.x1 + halfWidth;
        if (x < args.renderWindow.x1 || x >= args.renderWindow.x2) {
            continue;
        }
        auto y = (int)round(halfHeight * root.imag() / maxLeadingCoefficient) + rod.y1 + halfHeight;
        if (y < args.renderWindow.y1 || y >= args.renderWindow.y2) {
            continue;
        }
        auto PIX = (float*)destinationImage->getPixelAddress(x, y);
        for (auto c = 0; c < std::max(3, componentCount); c++, PIX++) {
            *PIX += 1;
        }
    }
}
