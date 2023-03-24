#include "EstimateGradePlugin.h"
#include "ofxsCoords.h"
#include <cmath>
#include <vector>
#include <gsl/gsl_math.h>
#include <gsl/gsl_multifit_nlinear.h>

#define SRC_COUNT 1000


EstimateGradePlugin::EstimateGradePlugin(OfxImageEffectHandle handle)
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
    _trgClip = fetchClip(kTargetClip);
    assert(
        _trgClip && (
            _trgClip->getPixelComponents() == ePixelComponentRGB
            || _trgClip->getPixelComponents() == ePixelComponentRGBA
            || _trgClip->getPixelComponents() == ePixelComponentAlpha
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
    _iterations = fetchIntParam(kParamIterations);
    _estimate = fetchPushButtonParam(kParamEstimate);
    _centrePoint = fetchRGBAParam(kParamCentrePoint);
    _slope = fetchRGBAParam(kParamSlope);
    _gamma = fetchRGBAParam(kParamGamma);
}

bool EstimateGradePlugin::isIdentity(const IsIdentityArguments &args, 
                                  Clip * &identityClip, double &/*identityTime*/
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
)
{
    return false;
}

double _sCurve(double srcVal, double centrePoint, double slope, double gamma) {
    return pow(1 / (1 + exp(-slope * (srcVal - centrePoint))), gamma);
}

void EstimateGradePlugin::render(const RenderArguments &args)
{
    std::unique_ptr<Image> srcImg(_srcClip->fetchImage(args.time));
    std::unique_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    auto components = srcImg->getPixelComponentCount();

    auto centrePointRGBA = _centrePoint->getValueAtTime(args.time);
    auto centrePoint = reinterpret_cast<double*>(&centrePointRGBA);
    auto slopeRGBA = _slope->getValueAtTime(args.time);
    auto slope = reinterpret_cast<double*>(&slopeRGBA);
    auto gammaRGBA = _gamma->getValueAtTime(args.time);
    auto gamma = reinterpret_cast<double*>(&gammaRGBA);

    for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {        
        for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            auto srcPix = (float*)srcImg->getPixelAddress(x, y);
            auto dstPix = (float*)dstImg->getPixelAddress(x, y);
            if (!srcPix || !dstPix) {continue;}
            for (int c=0; c < components; c++, srcPix++, dstPix++) {
                *dstPix = _sCurve(*srcPix, centrePoint[c], slope[c], gamma[c]);
            }
        }
    }
}

void EstimateGradePlugin::changedParam(const InstanceChangedArgs &args, const std::string &paramName) {
    if (paramName == kParamEstimate) {
        estimate(args.time);
    }
}


int _sCurveFunction(const gsl_vector *x, void *data, gsl_vector *f) {
    double centrePoint = gsl_vector_get(x, 0);
    double slope = gsl_vector_get(x, 1);
    double gamma = gsl_vector_get(x, 2);

    auto srcAndTrg = (std::vector<OfxPointD>*)data;

    for (int i = 0; i < srcAndTrg->size(); i++) {
        auto srcVal = (*srcAndTrg)[i].x;
        auto trgVal = (*srcAndTrg)[i].y;
        gsl_vector_set(f, i, _sCurve(srcVal, centrePoint, slope, gamma) - trgVal);
    }

    return GSL_SUCCESS;
}

int _sCurveDerivative(const gsl_vector *x, void *data, gsl_matrix *J) {
    double centrePoint = gsl_vector_get(x, 0);
    double slope = gsl_vector_get(x, 1);
    double gamma = gsl_vector_get(x, 2);

    auto srcAndTrg = (std::vector<OfxPointD>*)data;

    for (int i = 0; i < srcAndTrg->size(); i++) {
        auto srcVal = (*srcAndTrg)[i].x;
        auto offExpr = srcVal - centrePoint;
        auto expExpr = exp(-slope * offExpr);
        auto invExpr = 1 / (1 + expExpr);
        auto powExpr = pow(invExpr, gamma);
        auto powPlusExpr = invExpr * powExpr;
        // centrePoint partial derivative
        gsl_matrix_set(
            J, i, 0,
            -gamma * slope * expExpr * powPlusExpr
        );
        // slope partial derivative
        gsl_matrix_set(
            J, i, 1,
            gamma * offExpr * expExpr * powPlusExpr
        );
        // gamma partial derivative
        gsl_matrix_set(
            J, i, 2,
            powExpr * log(invExpr)
        );
    }

    return GSL_SUCCESS;
}


void EstimateGradePlugin::estimate(double time) {
    std::unique_ptr<Image> srcImg(_srcClip->fetchImage(time));
    std::unique_ptr<Image> trgImg(_trgClip->fetchImage(time));
    auto srcROD = srcImg->getRegionOfDefinition();
    auto trgROD = trgImg->getRegionOfDefinition();
    auto horizScale = trgImg->getPixelAspectRatio() / srcImg->getPixelAspectRatio();
    trgROD.x1 *= horizScale;
    trgROD.x2 *= horizScale;

    OfxRectI isect;
    Coords::rectIntersection(srcROD, trgROD, &isect);

    auto components = srcImg->getPixelComponentCount();

    std::vector<OfxPointD[SRC_COUNT]> sums(components);
    std::vector<int[SRC_COUNT]> counts(components);

    for (int c=0; c < components; c++) {
        for (int i=0; i < SRC_COUNT; i++) {
            sums[c][i] = {0, 0};
            counts[c][i] = 0;
        }
    }

    int i = 0;
    for (auto y=isect.y1; y < isect.y2; y+=100) {        
        for (auto x=isect.x1; x < isect.x2; x+=100) {
            auto srcPix = (float*)srcImg->getPixelAddress(x, y);
            auto trgPix = (float*)trgImg->getPixelAddress(x, y);
            if (!srcPix || !trgPix) {continue;}
            for (int c=0; c < components; c++, srcPix++, trgPix++) {
                if (*srcPix < 0 || *srcPix >= 1) {continue;}
                int i = floor(*srcPix * SRC_COUNT);
                sums[c][i].x += *srcPix;
                sums[c][i].y += *trgPix;
                counts[c][i]++;
            }
            i++;
        }
    }

    std::vector<std::vector<OfxPointD>> srcAndTrgs(components);

    for (int c=0; c < components; c++) {
        auto srcAndTrgPtr = &srcAndTrgs[c];
        for (int i=0; i < SRC_COUNT; i++) {
            auto count = counts[c][i];
            if (!count) {continue;}
            auto sum = sums[c][i];
            srcAndTrgPtr->push_back({sum.x / count, sum.y / count});
        }
    }

    const gsl_multifit_nlinear_type * T = gsl_multifit_nlinear_trust;

    double centrePoint[4];
    double slope[4];
    double gamma[4];

    for (int c=0; c < components; c++) {
        auto srcAndTrgPtr = &(srcAndTrgs[c]);
        printf("count %d: %ld\n", c, srcAndTrgPtr->size());

        gsl_multifit_nlinear_parameters fdfParams = gsl_multifit_nlinear_default_parameters();
        gsl_multifit_nlinear_workspace * w = gsl_multifit_nlinear_alloc(T, &fdfParams, srcAndTrgPtr->size(), 3);
        gsl_multifit_nlinear_fdf fdf;

        fdf.f = &_sCurveFunction;
        fdf.df = &_sCurveDerivative;
        fdf.n = srcAndTrgPtr->size();
        fdf.p = 3;
        fdf.params = srcAndTrgPtr;
        
        double start[3] = {0.5, 1.0, 1.0};
        gsl_vector_view startView = gsl_vector_view_array(start, 3);
        std::unique_ptr<double> weights(new double[srcAndTrgPtr->size()]);
        for (int i=0; i < srcAndTrgPtr->size(); i++) {weights.get()[i] = 1;}
        gsl_vector_view weightsView = gsl_vector_view_array(weights.get(), srcAndTrgPtr->size());

        gsl_multifit_nlinear_winit(&startView.vector, &weightsView.vector, &fdf, w);

        int info;
        auto status = gsl_multifit_nlinear_driver(_iterations->getValue(), 1e-6, 1e-6, 1e-6, NULL, NULL, &info, w);

        centrePoint[c] = gsl_vector_get(w->x, 0);
        slope[c] = gsl_vector_get(w->x, 1);
        gamma[c] = gsl_vector_get(w->x, 2);

        printf("done %d: %d. %f %f %f\n", c, status, centrePoint[c], slope[c], gamma[c]);

        gsl_multifit_nlinear_free(w);
    }

    _centrePoint->setValue(centrePoint[0], centrePoint[1], centrePoint[2], centrePoint[3]);
    _slope->setValue(slope[0], slope[1], slope[2], slope[3]);
    _gamma->setValue(gamma[0], gamma[1], gamma[2], gamma[3]);
}
