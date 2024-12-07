#include "EstimateGradePlugin.h"
#include "ofxsCoords.h"
#include <cmath>
#include <set>
#include <gsl/gsl_math.h>
#include <gsl/gsl_multifit_nlinear.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_linalg.h>
#include <libqhull_r/libqhull_r.h>


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
    _mapping = fetchChoiceParam(kParamMapping);
    _samples = fetchIntParam(kParamSamples);
    _iterations = fetchIntParam(kParamIterations);
    _estimate = fetchPushButtonParam(kParamEstimate);
    _blackPoint = fetchRGBAParam(kParamBlackPoint);
    _whitePoint = fetchRGBAParam(kParamWhitePoint);
    _centrePoint = fetchRGBAParam(kParamCentrePoint);
    _slope = fetchRGBAParam(kParamSlope);
    _gamma = fetchRGBAParam(kParamGamma);
    _matrixRed = fetchRGBAParam(kParamMatrixRed);
    _matrixGreen = fetchRGBAParam(kParamMatrixGreen);
    _matrixBlue = fetchRGBAParam(kParamMatrixBlue);
    _matrixAlpha = fetchRGBAParam(kParamMatrixAlpha);
    _x1 = fetchRGBAParam(kParamX1);
    _y1 = fetchRGBAParam(kParamY1);
    _slope1 = fetchRGBAParam(kParamSlope1);
    _x2 = fetchRGBAParam(kParamX2);
    _y2 = fetchRGBAParam(kParamY2);
    _x3 = fetchRGBAParam(kParamX3);
    _y3 = fetchRGBAParam(kParamY3);
    _slope3 = fetchRGBAParam(kParamSlope3);
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

double _gammaMapping(double srcVal, double blackPoint, double whitePoint, double gamma) {
    return pow((srcVal - blackPoint) / (whitePoint - blackPoint), 1.0 / gamma);
}

int _gammaMappingFunction(const gsl_vector *x, void *data, gsl_vector *f) {
    double blackPoint = gsl_vector_get(x, 0);
    double whitePoint = gsl_vector_get(x, 1);
    double gamma = gsl_vector_get(x, 2);

    auto srcAndTrg = (std::vector<OfxPointD>*)data;

    for (int i = 0; i < srcAndTrg->size(); i++) {
        auto srcVal = (*srcAndTrg)[i].x;
        auto trgVal = (*srcAndTrg)[i].y;
        gsl_vector_set(f, i, _gammaMapping(srcVal, blackPoint, whitePoint, gamma) - trgVal);
    }

    return GSL_SUCCESS;
}

int _gammaMappingDerivative(const gsl_vector *x, void *data, gsl_matrix *J) {
    double blackPoint = gsl_vector_get(x, 0);
    double whitePoint = gsl_vector_get(x, 1);
    double gamma = gsl_vector_get(x, 2);

    auto srcAndTrg = (std::vector<OfxPointD>*)data;

    for (int i = 0; i < srcAndTrg->size(); i++) {
        auto srcVal = (*srcAndTrg)[i].x;
        auto widthExpr = whitePoint - blackPoint;
        auto scaleExpr = (srcVal - blackPoint) / widthExpr;
        auto f = pow(scaleExpr, 1.0 / gamma);
        // blackPoint partial derivative
        gsl_matrix_set(
            J, i, 0,
            (srcVal - whitePoint) * f / (scaleExpr * gamma * widthExpr * widthExpr)
        );
        // whitePoint partial derivative
        gsl_matrix_set(
            J, i, 1,
            -f / (gamma * widthExpr)
        );
        // gamma partial derivative
        gsl_matrix_set(
            J, i, 2,
            -f * log(scaleExpr) / (gamma * gamma)
        );
    }

    return GSL_SUCCESS;
}

double _sCurveMapping(double srcVal, double centrePoint, double slope, double gamma) {
    return pow(1.0 / (1.0 + exp(-slope * (srcVal - centrePoint))), gamma);
}

int _sCurveMappingFunction(const gsl_vector *x, void *data, gsl_vector *f) {
    double centrePoint = gsl_vector_get(x, 0);
    double slope = gsl_vector_get(x, 1);
    double gamma = gsl_vector_get(x, 2);

    auto srcAndTrg = (std::vector<OfxPointD>*)data;

    for (int i = 0; i < srcAndTrg->size(); i++) {
        auto srcVal = (*srcAndTrg)[i].x;
        auto trgVal = (*srcAndTrg)[i].y;
        gsl_vector_set(f, i, _sCurveMapping(srcVal, centrePoint, slope, gamma) - trgVal);
    }

    return GSL_SUCCESS;
}

int _sCurveMappingDerivative(const gsl_vector *x, void *data, gsl_matrix *J) {
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

void _calc3PointCurveCoeffs(
    double x1, double y1, double slope1, double x2, double y2, double x3, double y3, double slope3,
    double* a1, double* b1, double* c1, double* d1,
    double* a2, double* b2, double* c2, double* d2
) {
    *a1 = (
        1.0/2.0*(
            (slope1 - slope3)*pow(x2, 3) - slope1* x2 * pow(x3, 2) + (
                (2*slope1 - slope3)*x2 - (2*slope1 - slope3)*x3 + 3*y2 - 3*y3
            )*pow(x1, 2) + (slope3*x3 - 3*y3)*pow(x2, 2) - (
                (3*slope1 - 2*slope3)*pow(x2, 2) - slope1*pow(x3, 2) - 2*(
                    (slope1 - slope3)*x3 + 3*y3
                )*x2 + 2*(x2 + 2*x3)*y2
            )*x1 - (
                4*x1*(x2 - x3)
                - 3*pow(x2, 2) + 2*x2*x3 + pow(x3, 2)
            )*y1 + (2*x2*x3 + pow(x3, 2))*y2
        )/(
            pow(x1, 4)*(x2 - x3)
            + pow(x2, 4)*x3
            - pow(x2, 3)*pow(x3, 2)
            - (
                3*pow(x2, 2)
                - 2*x2*x3
                - pow(x3, 2)
            )*pow(x1, 3)
            + 3*(
                pow(x2, 3) - x2*pow(x3, 2)
            )*pow(x1, 2)
            - (
                pow(x2, 4) + 2*pow(x2, 3)*x3
                - 3*pow(x2, 2)*pow(x3, 2)
            )*x1
        )
    );
    *b1 = (
        -1.0/2.0*(
            (slope1 - slope3)*pow(x2, 4) - 3*slope1*pow(x2, 2)*pow(x3, 2) + 2*(
                (slope1 - slope3)*x2 - (slope1 - slope3)*x3 + 3*y2 - 3*y3
            )*pow(x1, 3) + (
                (2*slope1 + slope3)*x3 - 3*y3
            )*pow(x2, 3) + 3*x2*pow(x3, 2)*y2 + 3*(
                slope3*pow(x2, 2) - (
                    slope3*x3 - 3*y3
                )*x2 - (x2 + 2*x3)*y2
            )*pow(x1, 2) - 3*(
                slope1*pow(x2, 3) - slope1*x2*pow(x3, 2)
            )*x1 - 3*(
                2*pow(x1, 2)*(x2 - x3)
                - pow(x2, 3) + x2*pow(x3, 2)
            )*y1
        )/(
            pow(x1, 4)*(x2 - x3)
            + pow(x2, 4)*x3 - pow(x2, 3)*pow(x3, 2)
            - (
                3*pow(x2, 2) - 2*x2*x3 - pow(x3, 2)
            )*pow(x1, 3) + 3*(
                pow(x2, 3) - x2*pow(x3, 2)
            )*pow(x1, 2)
            - (
                pow(x2, 4) + 2*pow(x2, 3)*x3
                - 3*pow(x2, 2)*pow(x3, 2)
            )*x1
        )
    );
    *c1 = (
        1.0/2.0*(
            2*slope1*pow(x2, 4)*x3 - 2*slope1*pow(x2, 3)*pow(x3, 2) - (
                slope3*x2 - slope3*x3 - 3*y2 + 3*y3
            )*pow(x1, 4) + (
                3*slope1*pow(x2, 2) - 2*slope1*x2*x3 - slope1*pow(x3, 2)
            )*pow(x1, 3) - 3*((slope1 - slope3)*pow(x2, 3)
            - slope1*x2*pow(x3, 2) + (slope3*x3 - 3*y3)*pow(x2, 2)
            + (2*x2*x3 + pow(x3, 2))*y2)*pow(x1, 2) - 2*(
                slope3*pow(x2, 4) - (slope3*x3 - 3*y3)*pow(x2, 3) - 3*x2*pow(x3, 2)*y2
            )*x1 - 3*(
                (3*pow(x2, 2) - 2*x2*x3 - pow(x3, 2)
            )*pow(x1, 2) - 2*(pow(x2, 3) - x2*pow(x3, 2))*x1)*y1
        )/(
            pow(x1, 4)*(x2 - x3)
            + pow(x2, 4)*x3 - pow(x2, 3)*pow(x3, 2)
            - (3*pow(x2, 2) - 2*x2*x3 - pow(x3, 2))*pow(x1, 3)
            + 3*(pow(x2, 3) - x2*pow(x3, 2))*pow(x1, 2) - (
                pow(x2, 4) + 2*pow(x2, 3)*x3 - 3*pow(x2, 2)*pow(x3, 2)
            )*x1
        )
    );
    *d1 = (
        1.0/2.0*((slope3*pow(x2, 2) - (slope3*x3 - 3*y3)*x2 - (x2 + 2*x3)*y2)*pow(x1, 4) - ((slope1 + 2*slope3)*pow(x2, 3) - slope1*x2*pow(x3, 2) - 2*(slope3*x3 - 3*y3)*pow(x2, 2) - 2*(2*x2*x3 + pow(x3, 2))*y2)*pow(x1, 3) + ((slope1 + slope3)*pow(x2, 4) - 3*slope1*pow(x2, 2)*pow(x3, 2) + ((2*slope1 - slope3)*x3 + 3*y3)*pow(x2, 3) - 3*x2*pow(x3, 2)*y2)*pow(x1, 2) - 2*(slope1*pow(x2, 4)*x3 - slope1*pow(x2, 3)*pow(x3, 2))*x1 + (2*pow(x2, 4)*x3 - 2*pow(x2, 3)*pow(x3, 2) + 3*(pow(x2, 3) - x2*pow(x3, 2))*pow(x1, 2) - 2*(pow(x2, 4) + 2*pow(x2, 3)*x3 - 3*pow(x2, 2)*pow(x3, 2))*x1)*y1)/(pow(x1, 4)*(x2 - x3) + pow(x2, 4)*x3 - pow(x2, 3)*pow(x3, 2) - (3*pow(x2, 2) - 2*x2*x3 - pow(x3, 2))*pow(x1, 3) + 3*(pow(x2, 3) - x2*pow(x3, 2))*pow(x1, 2) - (pow(x2, 4) + 2*pow(x2, 3)*x3 - 3*pow(x2, 2)*pow(x3, 2))*x1)
    );
    *a2 = (
        1.0/2.0*((slope1 - slope3)*pow(x2, 3) + (slope3*x2 - slope3*x3 - y2 + y3)*pow(x1, 2) - ((2*slope1 - 3*slope3)*x3 + 3*y3)*pow(x2, 2) - (slope1*pow(x2, 2) + (slope1 - 2*slope3)*pow(x3, 2) - 2*((slope1 - slope3)*x3 + y3)*x2 + 2*(x2 - 2*x3)*y2 + 4*x3*y3)*x1 + ((slope1 - 2*slope3)*pow(x3, 2) + 4*x3*y3)*x2 + 3*(pow(x2, 2) - 2*x2*x3 + pow(x3, 2))*y1 + (2*x2*x3 - 3*pow(x3, 2))*y2)/(pow(x2, 4)*x3 - 3*pow(x2, 3)*pow(x3, 2) + 3*pow(x2, 2)*pow(x3, 3) - x2*pow(x3, 4) + (pow(x2, 3) - 3*pow(x2, 2)*x3 + 3*x2*pow(x3, 2) - pow(x3, 3))*pow(x1, 2) - (pow(x2, 4) - 2*pow(x2, 3)*x3 + 2*x2*pow(x3, 3) - pow(x3, 4))*x1)
    );
    *b2 = (
        -1.0/2.0*((slope1 - slope3)*pow(x2, 4) - 3*slope1*pow(x2, 2)*pow(x3, 2) + 3*(slope3*x3 - y3)*pow(x2, 3) + 3*(slope3*pow(x2, 2) - (slope3*x3 - y3)*x2 - x2*y2)*pow(x1, 2) - ((slope1 + 2*slope3)*pow(x2, 3) - 3*slope1*x2*pow(x3, 2) + 2*(slope1 - slope3)*pow(x3, 3) - 6*pow(x3, 2)*y2 + 6*pow(x3, 2)*y3)*x1 + 2*((slope1 - slope3)*pow(x3, 3) + 3*pow(x3, 2)*y3)*x2 + 3*(pow(x2, 3) - 3*x2*pow(x3, 2) + 2*pow(x3, 3))*y1 + 3*(x2*pow(x3, 2) - 2*pow(x3, 3))*y2)/(pow(x2, 4)*x3 - 3*pow(x2, 3)*pow(x3, 2) + 3*pow(x2, 2)*pow(x3, 3) - x2*pow(x3, 4) + (pow(x2, 3) - 3*pow(x2, 2)*x3 + 3*x2*pow(x3, 2) - pow(x3, 3))*pow(x1, 2) - (pow(x2, 4) - 2*pow(x2, 3)*x3 + 2*x2*pow(x3, 3) - pow(x3, 4))*x1)
    );
    *c2 = (
        1.0/2.0*(2*slope1*pow(x2, 4)*x3 + slope1*x2*pow(x3, 4) - 3*pow(x3, 4)*y2 - 3*((slope1 - slope3)*pow(x3, 2) + 2*x3*y3)*pow(x2, 3) + (2*slope3*pow(x2, 3) + slope3*pow(x3, 3) - 3*pow(x3, 2)*y3 - 3*(slope3*pow(x3, 2) - 2*x3*y3)*x2 - 3*(2*x2*x3 - pow(x3, 2))*y2)*pow(x1, 2) - 3*(slope3*pow(x3, 3) - 3*pow(x3, 2)*y3)*pow(x2, 2) - (2*slope3*pow(x2, 4) + 2*slope1*pow(x2, 3)*x3 - 3*slope1*pow(x2, 2)*pow(x3, 2) + slope1*pow(x3, 4) - 6*x2*pow(x3, 2)*y2 - 2*(slope3*pow(x3, 3) - 3*pow(x3, 2)*y3)*x2)*x1 + 3*(2*pow(x2, 3)*x3 - 3*pow(x2, 2)*pow(x3, 2) + pow(x3, 4))*y1)/(pow(x2, 4)*x3 - 3*pow(x2, 3)*pow(x3, 2) + 3*pow(x2, 2)*pow(x3, 3) - x2*pow(x3, 4) + (pow(x2, 3) - 3*pow(x2, 2)*x3 + 3*x2*pow(x3, 2) - pow(x3, 3))*pow(x1, 2) - (pow(x2, 4) - 2*pow(x2, 3)*x3 + 2*x2*pow(x3, 3) - pow(x3, 4))*x1)
    );
    *d2 = (
        -1.0/2.0*(slope1*pow(x2, 2)*pow(x3, 4) - x2*pow(x3, 4)*y2 + ((slope1 + slope3)*pow(x3, 2) - 2*x3*y3)*pow(x2, 4) - ((2*slope1 + slope3)*pow(x3, 3) - 3*pow(x3, 2)*y3)*pow(x2, 3) + (2*(slope3*x3 - y3)*pow(x2, 3) - 3*(slope3*pow(x3, 2) - 2*x3*y3)*pow(x2, 2) + (slope3*pow(x3, 3) - 3*pow(x3, 2)*y3)*x2 - (3*x2*pow(x3, 2) - 2*pow(x3, 3))*y2)*pow(x1, 2) + (2*slope1*pow(x2, 2)*pow(x3, 3) - slope1*x2*pow(x3, 4) - 2*(slope3*x3 - y3)*pow(x2, 4) - ((slope1 - 2*slope3)*pow(x3, 2) + 4*x3*y3)*pow(x2, 3) + 2*(2*x2*pow(x3, 3) - pow(x3, 4))*y2)*x1 + 3*(pow(x2, 3)*pow(x3, 2) - 2*pow(x2, 2)*pow(x3, 3) + x2*pow(x3, 4))*y1)/(pow(x2, 4)*x3 - 3*pow(x2, 3)*pow(x3, 2) + 3*pow(x2, 2)*pow(x3, 3) - x2*pow(x3, 4) + (pow(x2, 3) - 3*pow(x2, 2)*x3 + 3*x2*pow(x3, 2) - pow(x3, 3))*pow(x1, 2) - (pow(x2, 4) - 2*pow(x2, 3)*x3 + 2*x2*pow(x3, 3) - pow(x3, 4))*x1)
    );
}

double _3PointCurveMapping(
    double srcVal,
    double x2,
    double a1, double b1, double c1, double d1,
    double a2, double b2, double c2, double d2
) {
    if (srcVal < x2) {
        return a1*pow(srcVal, 3) + b1*pow(srcVal, 2) + c1*srcVal + d1;
    } else {
        return a2*pow(srcVal, 3) + b2*pow(srcVal, 2) + c2*srcVal + d2;
    }
}

int _3PointCurveMappingFunction(const gsl_vector *x, void *data, gsl_vector *f) {
    double x1 = gsl_vector_get(x, 0);
    double y1 = gsl_vector_get(x, 1);
    double slope1 = gsl_vector_get(x, 2);
    double x2 = gsl_vector_get(x, 3);
    double y2 = gsl_vector_get(x, 4);
    double x3 = gsl_vector_get(x, 5);
    double y3 = gsl_vector_get(x, 6);
    double slope3 = gsl_vector_get(x, 7);

    double a1, b1, c1, d1, a2, b2, c2, d2;

    _calc3PointCurveCoeffs(
        x1, y1, slope1, x2, y2, x3, y3, slope3,
        &a1, &b1, &c1, &d1, &a2, &b2, &c2, &d2
    );

    auto srcAndTrg = (std::vector<OfxPointD>*)data;

    for (int i = 0; i < srcAndTrg->size(); i++) {
        auto srcVal = (*srcAndTrg)[i].x;
        auto trgVal = (*srcAndTrg)[i].y;
        gsl_vector_set(
            f, i,
            _3PointCurveMapping(
                srcVal,
                x2, a1, b1, c1, d1, a2, b2, c2, d2
            ) - trgVal
        );
    }

    return GSL_SUCCESS;
}


void _matrixMapping(const double* srcVal, const double matrix[4][4], double* dstVal) {
    for (int r=0; r < 4; r++) {
        dstVal[r] = 0;
        for (int c=0; c < 4; c++) {
            dstVal[r] += srcVal[c] * matrix[r][c];
        }
    }
}

void EstimateGradePlugin::render(const RenderArguments &args)
{
    std::unique_ptr<Image> srcImg(_srcClip->fetchImage(args.time));
    std::unique_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    auto components = srcImg->getPixelComponentCount();

    auto mapping = _mapping->getValue();

    switch (mapping) {
        case 0:
        case 1:
        case 2:
            renderCurve(args, mapping, srcImg.get(), dstImg.get(), components);
            break;
        case 3:
            renderMatrix(args, srcImg.get(), dstImg.get(), components);
            break;
        case 4:
            renderCube(args, srcImg.get(), dstImg.get(), components);
            break;
    }
}

void EstimateGradePlugin::renderCurve(const RenderArguments &args, int mapping, Image* srcImg, Image* dstImg, int components) {
    auto blackPointRGBA = _blackPoint->getValueAtTime(args.time);
    auto blackPoint = reinterpret_cast<double*>(&blackPointRGBA);
    auto whitePointRGBA = _whitePoint->getValueAtTime(args.time);
    auto whitePoint = reinterpret_cast<double*>(&whitePointRGBA);
    auto centrePointRGBA = _centrePoint->getValueAtTime(args.time);
    auto centrePoint = reinterpret_cast<double*>(&centrePointRGBA);
    auto slopeRGBA = _slope->getValueAtTime(args.time);
    auto slope = reinterpret_cast<double*>(&slopeRGBA);
    auto gammaRGBA = _gamma->getValueAtTime(args.time);
    auto gamma = reinterpret_cast<double*>(&gammaRGBA);
    auto x1RGBA = _x1->getValueAtTime(args.time);
    auto x1 = reinterpret_cast<double*>(&x1RGBA);
    auto y1RGBA = _y1->getValueAtTime(args.time);
    auto y1 = reinterpret_cast<double*>(&y1RGBA);
    auto slope1RGBA = _slope1->getValueAtTime(args.time);
    auto slope1 = reinterpret_cast<double*>(&slope1RGBA);
    auto x2RGBA = _x2->getValueAtTime(args.time);
    auto x2 = reinterpret_cast<double*>(&x2RGBA);
    auto y2RGBA = _y2->getValueAtTime(args.time);
    auto y2 = reinterpret_cast<double*>(&y2RGBA);
    auto x3RGBA = _x3->getValueAtTime(args.time);
    auto x3 = reinterpret_cast<double*>(&x3RGBA);
    auto y3RGBA = _y3->getValueAtTime(args.time);
    auto y3 = reinterpret_cast<double*>(&y3RGBA);
    auto slope3RGBA = _slope3->getValueAtTime(args.time);
    auto slope3 = reinterpret_cast<double*>(&slope3RGBA);

    double a1[4], b1[4], c1[4], d1[4], a2[4], b2[4], c2[4], d2[4];

    if (mapping == 2) {
        for (int c=0; c < 4; c++) {
            _calc3PointCurveCoeffs(
                x1[c], y1[c], slope1[c], x2[c], y2[c], x3[c], y3[c], slope3[c],
                &a1[c], &b1[c], &c1[c], &d1[c], &a2[c], &b2[c], &c2[c], &d2[c]
            );
            // std::cout << a1[c] << " " << b1[c] << " " << c1[c] << " " << d1[c] << std::endl;
            // std::cout << a2[c] << " " << b2[c] << " " << c2[c] << " " << d2[c] << std::endl;
        }
    }

    for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {        
        for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            auto srcPix = (float*)srcImg->getPixelAddress(x, y);
            auto dstPix = (float*)dstImg->getPixelAddress(x, y);
            if (!srcPix || !dstPix) {continue;}
            for (int c=0; c < components; c++, srcPix++, dstPix++) {
                switch (mapping) {
                    case 0:
                        *dstPix = _gammaMapping(*srcPix, blackPoint[c], whitePoint[c], gamma[c]);
                        break;
                    case 1:
                        *dstPix = _sCurveMapping(*srcPix, centrePoint[c], slope[c], gamma[c]);
                        break;
                    case 2:
                        *dstPix = _3PointCurveMapping(
                            *srcPix, x2[c], a1[c], b1[c], c1[c], d1[c], a2[c], b2[c], c2[c], d2[c]
                        );
                        break;
                }
            }
        }
    }
}

void fillArrayFromRGBA(double* array, OfxRGBAColourD rgba) {
    array[0] = rgba.r;
    array[1] = rgba.g;
    array[2] = rgba.b;
    array[3] = rgba.a;
}

void EstimateGradePlugin::renderMatrix(const RenderArguments &args, Image* srcImg, Image* dstImg, int components) {
    double matrix[4][4];
    fillArrayFromRGBA(matrix[0], _matrixRed->getValueAtTime(args.time));
    fillArrayFromRGBA(matrix[1], _matrixGreen->getValueAtTime(args.time));
    fillArrayFromRGBA(matrix[2], _matrixBlue->getValueAtTime(args.time));
    fillArrayFromRGBA(matrix[3], _matrixAlpha->getValueAtTime(args.time));
    for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {        
        for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            auto srcPix = (float*)srcImg->getPixelAddress(x, y);
            auto dstPix = (float*)dstImg->getPixelAddress(x, y);
            if (!srcPix || !dstPix) {continue;}
            double srcVal[4];
            double dstVal[4];
            for (int c=0; c < 4; c++, srcPix++) {
                if (c < components) {
                    srcVal[c] = *srcPix;
                } else {
                    srcVal[c] = 0;
                }
            }
            _matrixMapping(srcVal, matrix, dstVal);
            for (int c=0; c < components; c++, dstPix++) {
                *dstPix = dstVal[c];
            }
        }
    }
}

void EstimateGradePlugin::renderCube(const RenderArguments &args, Image* srcImg, Image* dstImg, int components) {
    for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {        
        for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            auto srcPix = (float*)srcImg->getPixelAddress(x, y);
            auto dstPix = (float*)dstImg->getPixelAddress(x, y);
            if (!srcPix || !dstPix) {continue;}
            dstPix[3] = srcPix[3];

            // TODO: for each tetra and source RGB
            for (int i=0; i < _cube_src_tetrahedra.size(); i++) {
                auto tetra = _cube_src_tetrahedra[i];
                auto bounds_minimum = _cube_src_tetrahedron_bounds_minimum[i];
                auto bounds_maximum = _cube_src_tetrahedron_bounds_maximum[i];
                bool inside_bounds = true;
                for (int c=0; c < 3; c++) {
                    inside_bounds = srcPix[c] < bounds_minimum[c] || srcPix[c] > bounds_maximum[c];
                    if (!inside_bounds) {break;}
                }
                if (!inside_bounds) {continue;}

                auto barycentric_coordinates = _cube_src_tetrahedron_inverse_barycentric_matrices[i] * Eigen::Vector4d(srcPix[0], srcPix[1], srcPix[2], 1.0);
                for (int c=0; c < 3; c++) {
                    inside_bounds = barycentric_coordinates[c] >= 0;
                    if (!inside_bounds) {break;}
                }
                if (!inside_bounds) {continue;}

                Eigen::Matrix4d trg_matrix;
                for (int j=0; j < 4; j++) {
                    auto trg_point = _cube_trg_points.data() + tetra[j] * 3;
                    trg_matrix.row(j) = Eigen::RowVector4d(trg_point[0], trg_point[1], trg_point[2], 1.0);
                }
                auto trg_vector = trg_matrix * barycentric_coordinates;
                for (int c=0; c < 3; c++) {
                    dstPix[c] = trg_vector(c);
                }
                break;
            }

        }
    }

    // check if inside bounds
    // if so multiply source colours by inverse barycentric matrix.
    // if all positive then it's inside.
    // multply M * lambda (M is dst tetra points with extra 1)
    // That gives you the out RGB
}

void EstimateGradePlugin::changedParam(const InstanceChangedArgs &args, const std::string &paramName) {
    if (paramName == kParamEstimate) {
        estimate(args.time);
    } else if (paramName == kParamMapping) {
        auto mapping = _mapping->getValue();
        _blackPoint->setIsSecretAndDisabled(true);
        _whitePoint->setIsSecretAndDisabled(true);
        _centrePoint->setIsSecretAndDisabled(true);
        _slope->setIsSecretAndDisabled(true);
        _gamma->setIsSecretAndDisabled(true);
        _matrixRed->setIsSecretAndDisabled(true);
        _matrixGreen->setIsSecretAndDisabled(true);
        _matrixBlue->setIsSecretAndDisabled(true);
        _matrixAlpha->setIsSecretAndDisabled(true);
        _x1->setIsSecretAndDisabled(true);
        _y1->setIsSecretAndDisabled(true);
        _slope1->setIsSecretAndDisabled(true);
        _x2->setIsSecretAndDisabled(true);
        _y2->setIsSecretAndDisabled(true);
        _x3->setIsSecretAndDisabled(true);
        _y3->setIsSecretAndDisabled(true);
        _slope3->setIsSecretAndDisabled(true);
        _iterations->setIsSecretAndDisabled(false);
        switch (mapping) {
            case 0:
                _blackPoint->setIsSecretAndDisabled(false);
                _whitePoint->setIsSecretAndDisabled(false);
                _gamma->setIsSecretAndDisabled(false);
                break;
            case 1:
                _centrePoint->setIsSecretAndDisabled(false);
                _slope->setIsSecretAndDisabled(false);
                _gamma->setIsSecretAndDisabled(false);
                break;
            case 2:
                _x1->setIsSecretAndDisabled(false);
                _y1->setIsSecretAndDisabled(false);
                _slope1->setIsSecretAndDisabled(false);
                _x2->setIsSecretAndDisabled(false);
                _y2->setIsSecretAndDisabled(false);
                _x3->setIsSecretAndDisabled(false);
                _y3->setIsSecretAndDisabled(false);
                _slope3->setIsSecretAndDisabled(false);
                break;
            case 3:
                _matrixRed->setIsSecretAndDisabled(false);
                _matrixGreen->setIsSecretAndDisabled(false);
                _matrixBlue->setIsSecretAndDisabled(false);
                _matrixAlpha->setIsSecretAndDisabled(false);
            case 4:
                _iterations->setIsSecretAndDisabled(true);
                break;
        }
    }
}

void EstimateGradePlugin::estimate(double time) {
    progressStart("Estimating");
    progressUpdate(0);
    std::unique_ptr<Image> srcImg(_srcClip->fetchImage(time));
    progressUpdate(0.1);
    std::unique_ptr<Image> trgImg(_trgClip->fetchImage(time));
    progressUpdate(0.2);
    auto srcROD = srcImg->getRegionOfDefinition();
    auto trgROD = trgImg->getRegionOfDefinition();
    auto horizScale = trgImg->getPixelAspectRatio() / srcImg->getPixelAspectRatio();
    trgROD.x1 *= horizScale;
    trgROD.x2 *= horizScale;

    OfxRectI isect;
    Coords::rectIntersection(srcROD, trgROD, &isect);

    auto components = srcImg->getPixelComponentCount();

    auto mapping = _mapping->getValue();
    auto samples = _samples->getValue();
    auto iterations = _iterations->getValue();

    switch (mapping) {
        case 0:
        case 1:
        case 2:
            estimateCurve(time, mapping, samples, iterations, srcImg.get(), trgImg.get(), components, isect, horizScale);
            break;
        case 3:
            estimateMatrix(time, samples, srcImg.get(), trgImg.get(), components, isect, horizScale);
            break;
        case 4:
            estimateCube(time, samples, srcImg.get(), trgImg.get(), components, isect, horizScale);
    }

    progressEnd();
}

void EstimateGradePlugin::estimateCurve(
    double time, int mapping, int samples, int iterations, Image* srcImg, Image* trgImg, int components, OfxRectI isect, double horizScale
) {
    std::vector<std::vector<OfxPointD>> sums(components);
    std::vector<std::vector<int>> counts(components);

    for (int c=0; c < components; c++) {
        sums[c].resize(samples);
        counts[c].resize(samples);
        for (int i=0; i < samples; i++) {
            sums[c][i] = {0, 0};
            counts[c][i] = 0;
        }
    }

    for (auto y=isect.y1; y < isect.y2; y++) {        
        for (auto x=isect.x1; x < isect.x2; x++) {
            auto srcPix = (float*)srcImg->getPixelAddress(x, y);
            auto trgPix = (float*)trgImg->getPixelAddress(round(x / horizScale), y);
            if (!srcPix || !trgPix) {continue;}
            for (int c=0; c < components; c++, srcPix++, trgPix++) {
                if (*srcPix < 0 || *srcPix >= 1) {continue;}
                int i = floor(*srcPix * samples);
                sums[c][i].x += *srcPix;
                sums[c][i].y += *trgPix;
                counts[c][i]++;
            }
        }
    }

    std::vector<std::vector<OfxPointD>> srcAndTrgs(components);

    for (int c=0; c < components; c++) {
        auto srcAndTrgPtr = &srcAndTrgs[c];
        for (int i=0; i < samples; i++) {
            auto count = counts[c][i];
            if (!count) {continue;}
            auto sum = sums[c][i];
            srcAndTrgPtr->push_back({sum.x / count, sum.y / count});
        }
        printf("for %d: %ld points\n", c, srcAndTrgPtr->size());
    }


    const gsl_multifit_nlinear_type * T = gsl_multifit_nlinear_trust;

    double blackPoint[4];
    double whitePoint[4];
    double centrePoint[4];
    double slope[4];
    double gamma[4];
    double x1[4], y1[4], slope1[4], x2[4], y2[4], x3[4], y3[4], slope3[4];

    int nParams = 3;
    if (mapping == 2) {nParams = 8;}

    for (int c=0; c < components; c++) {
        auto srcAndTrgPtr = &(srcAndTrgs[c]);
        printf("count %d: %ld\n", c, srcAndTrgPtr->size());
        if (srcAndTrgPtr->size() < 3) {
            printf("TOO SMALL!\n");
            continue;
        }

        gsl_multifit_nlinear_parameters fdfParams = gsl_multifit_nlinear_default_parameters();
        gsl_multifit_nlinear_workspace * w = gsl_multifit_nlinear_alloc(T, &fdfParams, srcAndTrgPtr->size(), nParams);
        gsl_multifit_nlinear_fdf fdf;

        std::unique_ptr<double> start(new double[nParams]);
        switch (mapping) {
            case 0:
                fdf.f = &_gammaMappingFunction;
                fdf.df = &_gammaMappingDerivative;
                fdf.df = nullptr;
                start.get()[0] = 0.0;
                start.get()[1] = 1.0;
                start.get()[2] = 1.0;
                break;
            case 1:
                fdf.f = &_sCurveMappingFunction;
                fdf.df = &_sCurveMappingDerivative;
                start.get()[0] = 0.5;
                start.get()[1] = 1.0;
                start.get()[2] = 1.0;
                break;
            case 2:
                fdf.f = &_3PointCurveMappingFunction;
                fdf.df = nullptr;
                start.get()[0] = 0.0;
                start.get()[1] = 0.0;
                start.get()[2] = 1.0;
                start.get()[3] = 0.5;
                start.get()[4] = 0.5;
                start.get()[5] = 1.0;
                start.get()[6] = 1.0;
                start.get()[7] = 1.0;
                break;
        }

        fdf.n = srcAndTrgPtr->size();
        fdf.p = nParams;
        fdf.params = srcAndTrgPtr;
        
        gsl_vector_view startView = gsl_vector_view_array(start.get(), nParams);
        std::unique_ptr<double> weights(new double[srcAndTrgPtr->size()]);
        for (int i=0; i < srcAndTrgPtr->size(); i++) {weights.get()[i] = 1;}
        gsl_vector_view weightsView = gsl_vector_view_array(weights.get(), srcAndTrgPtr->size());

        gsl_multifit_nlinear_winit(&startView.vector, &weightsView.vector, &fdf, w);

        int info;
        auto status = gsl_multifit_nlinear_driver(iterations, 1e-8, 1e-8, 1e-8, NULL, NULL, &info, w);

        switch (mapping) {
            case 0:
                blackPoint[c] = gsl_vector_get(w->x, 0);
                whitePoint[c] = gsl_vector_get(w->x, 1);
                gamma[c] = gsl_vector_get(w->x, 2);
                break;
            case 1:
                centrePoint[c] = gsl_vector_get(w->x, 0);
                slope[c] = gsl_vector_get(w->x, 1);
                gamma[c] = gsl_vector_get(w->x, 2);
                break;
            case 2:
                x1[c] = gsl_vector_get(w->x, 0);
                y1[c] = gsl_vector_get(w->x, 1);
                slope1[c] = gsl_vector_get(w->x, 2);
                x2[c] = gsl_vector_get(w->x, 3);
                y2[c] = gsl_vector_get(w->x, 4);
                x3[c] = gsl_vector_get(w->x, 5);
                y3[c] = gsl_vector_get(w->x, 6);
                slope3[c] = gsl_vector_get(w->x, 7);
                break;
        }

        // printf("done %d: %d. %f %f %f\n", c, status, gsl_vector_get(w->x, 0), gsl_vector_get(w->x, 1), gsl_vector_get(w->x, 2));

        gsl_multifit_nlinear_free(w);
    }

    switch (mapping) {
        case 0:
            _blackPoint->setValue(blackPoint[0], blackPoint[1], blackPoint[2], blackPoint[3]);
            _whitePoint->setValue(whitePoint[0], whitePoint[1], whitePoint[2], whitePoint[3]);
            _gamma->setValue(gamma[0], gamma[1], gamma[2], gamma[3]);
            break;
        case 1:
            _centrePoint->setValue(centrePoint[0], centrePoint[1], centrePoint[2], centrePoint[3]);
            _slope->setValue(slope[0], slope[1], slope[2], slope[3]);
            _gamma->setValue(gamma[0], gamma[1], gamma[2], gamma[3]);
            break;
        case 2:
            _x1->setValue(x1[0], x1[1], x1[2], x1[3]);
            _y1->setValue(y1[0], y1[1], y1[2], y1[3]);
            _slope1->setValue(slope1[0], slope1[1], slope1[2], slope1[3]);
            _x2->setValue(x2[0], x2[1], x2[2], x2[3]);
            _y2->setValue(y2[0], y2[1], y2[2], y2[3]);
            _x3->setValue(x3[0], x3[1], x3[2], x3[3]);
            _y3->setValue(y3[0], y3[1], y3[2], y3[3]);
            _slope3->setValue(slope3[0], slope3[1], slope3[2], slope3[3]);
            break;
    }
}

void _readSourceAndTargetVals(
    int samples, Image* srcImg, Image* trgImg, int components, OfxRectI isect, double horizScale, std::vector<std::unique_ptr<double>>& srcVals, std::vector<std::unique_ptr<double>>& trgVals
) {
    auto samples_sqrt = sqrt(samples);
    auto x_step = std::max(1, int((isect.x2 - isect.x1) / samples_sqrt));
    auto y_step = std::max(1, int((isect.y2 - isect.y1) / samples_sqrt));
    for (auto y=isect.y1; y < isect.y2; y+=y_step) {
        for (auto x=isect.x1; x < isect.x2; x+=x_step) {
            auto srcPix = (float*)srcImg->getPixelAddress(x, y);
            auto trgPix = (float*)trgImg->getPixelAddress(round(x / horizScale), y);
            if (!srcPix || !trgPix) {continue;}
            double* srcVal = new double[4];
            double* trgVal = new double[4];
            bool skip = false;
            for (int c=0; c < 4; c++, srcPix++, trgPix++) {
                if (c < components) {
                    if (std::isnan(*srcPix) || std::isnan(*trgPix)) {
                        std::cout << "nan detected" << x << "," << y << ";" << c << std::endl;
                        skip = true;
                        break;
                    }
                    srcVal[c] = *srcPix;
                    trgVal[c] = *trgPix;
                } else {
                    srcVal[c] = 0;
                    trgVal[c] = 0;
                }
            }
            if (skip) {continue;}
            srcVals.push_back(std::unique_ptr<double>(srcVal));
            trgVals.push_back(std::unique_ptr<double>(trgVal));
            if (srcVals.size() == samples) {
                break;
            }
        }
        if (srcVals.size() == samples) {
            break;
        }
    }
}

void EstimateGradePlugin::estimateMatrix(
    double time, int samples, Image* srcImg, Image* trgImg, int components, OfxRectI isect, double horizScale
) {
    std::vector<std::unique_ptr<double>> srcVals;
    std::vector<std::unique_ptr<double>> trgVals;
    _readSourceAndTargetVals(samples, srcImg, trgImg, components, isect, horizScale, srcVals, trgVals);

    gsl_matrix *srcMat = gsl_matrix_alloc(3, srcVals.size());
    gsl_matrix *trgMat = gsl_matrix_alloc(3, srcVals.size());

    for (int c=0; c < 3; c++) {
        for (int i=0; i < srcVals.size(); i++) {
            gsl_matrix_set(srcMat, c, i, srcVals[i].get()[c]);
            gsl_matrix_set(trgMat, c, i, trgVals[i].get()[c]);
        }
    }

    progressUpdate(0.4);

    gsl_matrix *srcTransMat = gsl_matrix_alloc(srcMat->size2, srcMat->size1);
    gsl_matrix_transpose_memcpy(srcTransMat, srcMat);

    gsl_matrix *srcSqMat = gsl_matrix_alloc(srcMat->size1, srcMat->size1);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1, srcMat, srcTransMat, 0, srcSqMat);

    progressUpdate(0.5);

    int signum;
    gsl_permutation* perm = gsl_permutation_alloc(srcMat->size1);
    gsl_linalg_LU_decomp(srcSqMat, perm, &signum);

    progressUpdate(0.6);

    gsl_matrix *srcSqInvMat = gsl_matrix_alloc(srcSqMat->size1, srcSqMat->size1);
    gsl_linalg_LU_invert(srcSqMat, perm, srcSqInvMat);

    progressUpdate(0.7);

    gsl_matrix *trgSrcMat = gsl_matrix_alloc(srcMat->size1, srcMat->size1);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1, trgMat, srcTransMat, 0, trgSrcMat);

    progressUpdate(0.8);

    gsl_matrix *resMat = gsl_matrix_alloc(srcMat->size1, srcMat->size1);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1, trgSrcMat, srcSqInvMat, 0, resMat);

    progressUpdate(0.9);

    std::cout << "M = ";
    for (size_t i = 0; i < resMat->size1; ++i) {
        OfxRGBAColourD col;
        for (size_t j = 0; j < resMat->size2; ++j) {
            auto val = gsl_matrix_get(resMat, i, j);
            std::cout << val << " ";
            std::cout.flush();
            switch (j) {
                case 0: col.r = val; break;
                case 1: col.g = val; break;
                case 2: col.b = val; break;
                case 3: col.a = val; break;
            }
        }
        switch (i) {
            case 0:
                _matrixRed->setValue(col.r, col.g, col.b, col.a); break;
            case 1:
                _matrixGreen->setValue(col.r, col.g, col.b, col.a); break;
            case 2:
                _matrixBlue->setValue(col.r, col.g, col.b, col.a); break;
            case 3:
                _matrixAlpha->setValue(col.r, col.g, col.b, col.a); break;
        }
        std::cout << std::endl;
    }

    // Free memory
    gsl_matrix_free(srcMat);
    gsl_matrix_free(trgMat);
    gsl_matrix_free(srcTransMat);
    gsl_matrix_free(srcSqMat);
    gsl_permutation_free(perm);
    gsl_matrix_free(srcSqInvMat);
    gsl_matrix_free(trgSrcMat);
    gsl_matrix_free(resMat);
}

void _print_point(std::array<double, 3>& arr) {
    for (int i=0; i < 3; i++) {
        std::cout << arr[i] << ", ";
    }
    std::cout << std::endl;
}

void EstimateGradePlugin::estimateCube(
    double time, int samples, Image* srcImg, Image* trgImg, int components, OfxRectI isect, double horizScale
) {
    std::vector<std::unique_ptr<double>> srcVals, trgVals;
    _readSourceAndTargetVals(samples, srcImg, trgImg, components, isect, horizScale, srcVals, trgVals);
    
    std::set<point3d_t> taken_points;

    _cube_src_points.clear();
    _cube_trg_points.clear();
    for (int i=0; i < srcVals.size(); i++) {
        auto srcVal = srcVals[i].get();
        point3d_t src_point(srcVal[0], srcVal[1], srcVal[2]);
        if (taken_points.find(src_point) != taken_points.end()) {
            continue;
        }
        taken_points.insert(src_point);
        auto trgVal = trgVals[i].get();
        _cube_src_points.push_back(std::get<0>(src_point));
        _cube_src_points.push_back(std::get<1>(src_point));
        _cube_src_points.push_back(std::get<2>(src_point));
        _cube_trg_points.push_back(trgVal[0]);
        _cube_trg_points.push_back(trgVal[1]);
        _cube_trg_points.push_back(trgVal[2]);
    }

    for (double z=0; z < 2; z++) {
        for (double y=0; y < 2; y++) {
            for (double x=0; x < 2; x++) {
                point3d_t corner(x, y, z);
                if (taken_points.find(corner) != taken_points.end()) {
                    continue;
                }
                for (auto v : {x,y,z}) {
                    _cube_src_points.push_back(v);
                    _cube_trg_points.push_back(v);
                }
            }
        }
    }

    qhT qh_local;
    auto qh = &qh_local;
    qh_zero(qh, stderr);

    if (qh_new_qhull(qh, 3, _cube_src_points.size() / 3, _cube_src_points.data(), 0, "qhull d Qbb", stdout, stderr)) {
        std::cerr << "Qhull error: failed to compute Delaunay triangulation." << std::endl;
        return;
    }

    _cube_src_tetrahedra.clear();
    _cube_src_tetrahedron_inverse_barycentric_matrices.clear();
    facetT* facet;
    bool a = false;
    FORALLfacets {
        if (facet->toporient && facet->simplicial) {
            std::vector<int> tetra;
            vertexT* vertex, **vertexp;
            Eigen::Matrix4d barycentric_matrix;
            int r = 0;
            std::array<double, 3> bounds_minimum, bounds_maximum;
            FOREACHvertex_(facet->vertices) {
                tetra.push_back(qh_pointid(qh, vertex->point));
                if (!r) {
                    for (int c=0; c < 3; c++) {
                        bounds_minimum[c] = bounds_maximum[c] = vertex->point[c];
                    }
                } else {
                    for (int c=0; c < 3; c++) {
                        bounds_minimum[c] = std::min(bounds_minimum[c], vertex->point[c]);
                        bounds_maximum[c] = std::max(bounds_maximum[c], vertex->point[c]);
                    }
                }
                if (r < 4) {
                    barycentric_matrix.row(r++) = Eigen::RowVector4d(vertex->point[0], vertex->point[1], vertex->point[2], 1.0);
                }
            }
            if (tetra.size() != 4) {
                continue;
            }
            _cube_src_tetrahedra.push_back(tetra);
            _cube_src_tetrahedron_inverse_barycentric_matrices.push_back(barycentric_matrix.inverse());
            _cube_src_tetrahedron_bounds_minimum.push_back(bounds_minimum);
            _cube_src_tetrahedron_bounds_maximum.push_back(bounds_maximum);
            _print_point(bounds_minimum);
            _print_point(bounds_maximum);
        }
    }
}
