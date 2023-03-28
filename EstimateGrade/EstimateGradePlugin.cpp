#include "EstimateGradePlugin.h"
#include "ofxsCoords.h"
#include <cmath>
#include <vector>
#include <gsl/gsl_math.h>
#include <gsl/gsl_multifit_nlinear.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_linalg.h>


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
            renderCurve(args, mapping, srcImg.get(), dstImg.get(), components);
            break;
        case 2:
            renderMatrix(args, srcImg.get(), dstImg.get(), components);
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
                _matrixRed->setIsSecretAndDisabled(false);
                _matrixGreen->setIsSecretAndDisabled(false);
                _matrixBlue->setIsSecretAndDisabled(false);
                _matrixAlpha->setIsSecretAndDisabled(false);
                break;
        }
    }
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

    auto mapping = _mapping->getValue();
    auto samples = _samples->getValue();
    auto iterations = _iterations->getValue();

    switch (mapping) {
        case 0:
        case 1:
            estimateCurve(time, mapping, samples, iterations, srcImg.get(), trgImg.get(), components, isect, horizScale);
            break;
        case 2:
            estimateMatrix(time, samples, iterations, srcImg.get(), trgImg.get(), components, isect, horizScale);
            break;
    }
    

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

    for (int c=0; c < components; c++) {
        auto srcAndTrgPtr = &(srcAndTrgs[c]);
        printf("count %d: %ld\n", c, srcAndTrgPtr->size());
        if (srcAndTrgPtr->size() < 3) {
            printf("TOO SMALL!\n");
            continue;
        }

        gsl_multifit_nlinear_parameters fdfParams = gsl_multifit_nlinear_default_parameters();
        gsl_multifit_nlinear_workspace * w = gsl_multifit_nlinear_alloc(T, &fdfParams, srcAndTrgPtr->size(), 3);
        gsl_multifit_nlinear_fdf fdf;

        double start[3];
        switch (mapping) {
            case 0:
                fdf.f = &_gammaMappingFunction;
                fdf.df = &_gammaMappingDerivative;
                fdf.df = nullptr;
                start[0] = 0;
                start[1] = 1.0;
                break;
            case 1:
                fdf.f = &_sCurveMappingFunction;
                fdf.df = &_sCurveMappingDerivative;
                start[0] = 0.5;
                start[1] = 1.0;
                break;
        }
        start[2] = 1.0;

        fdf.n = srcAndTrgPtr->size();
        fdf.p = 3;
        fdf.params = srcAndTrgPtr;
        
        gsl_vector_view startView = gsl_vector_view_array(start, 3);
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
                break;
            case 1:
                centrePoint[c] = gsl_vector_get(w->x, 0);
                slope[c] = gsl_vector_get(w->x, 1);
                break;
        }
        gamma[c] = gsl_vector_get(w->x, 2);

        printf("done %d: %d. %f %f %f\n", c, status, gsl_vector_get(w->x, 0), gsl_vector_get(w->x, 1), gsl_vector_get(w->x, 2));

        gsl_multifit_nlinear_free(w);
    }

    switch (mapping) {
        case 0:
            _blackPoint->setValue(blackPoint[0], blackPoint[1], blackPoint[2], blackPoint[3]);
            _whitePoint->setValue(whitePoint[0], whitePoint[1], whitePoint[2], whitePoint[3]);
            break;
        case 1:
            _centrePoint->setValue(centrePoint[0], centrePoint[1], centrePoint[2], centrePoint[3]);
            _slope->setValue(slope[0], slope[1], slope[2], slope[3]);
            break;
    }
    _gamma->setValue(gamma[0], gamma[1], gamma[2], gamma[3]);
}

void EstimateGradePlugin::estimateMatrix(
    double time, int samples, int iterations, Image* srcImg, Image* trgImg, int components, OfxRectI isect, double horizScale
) {
    std::vector<std::unique_ptr<double>> srcVals;
    std::vector<std::unique_ptr<double>> trgVals;
    for (auto y=isect.y1; y < isect.y2; y+=100) {        
        for (auto x=isect.x1; x < isect.x2; x+=100) {
            auto srcPix = (float*)srcImg->getPixelAddress(x, y);
            auto trgPix = (float*)trgImg->getPixelAddress(round(x / horizScale), y);
            if (!srcPix || !trgPix) {continue;}
            double* srcVal = new double[4];
            double* trgVal = new double[4];
            for (int c=0; c < 4; c++, srcPix++, trgPix++) {
                if (c < components) {
                    srcVal[c] = *srcPix;
                    trgVal[c] = *trgPix;
                } else {
                    srcVal[c] = 0;
                    trgVal[c] = 0;
                }
            }
            srcVals.push_back(std::unique_ptr<double>(srcVal));
            trgVals.push_back(std::unique_ptr<double>(trgVal));
        }
    }

    gsl_matrix *srcMat = gsl_matrix_alloc(3, srcVals.size());
    gsl_matrix *trgMat = gsl_matrix_alloc(3, srcVals.size());

    for (int c=0; c < 3; c++) {
        for (int i=0; i < srcVals.size(); i++) {
            gsl_matrix_set(srcMat, c, i, srcVals[i].get()[c]);
            gsl_matrix_set(trgMat, c, i, trgVals[i].get()[c]);
        }
    }

    gsl_matrix *srcTransMat = gsl_matrix_alloc(srcMat->size2, srcMat->size1);
    gsl_matrix_transpose_memcpy(srcTransMat, srcMat);

    gsl_matrix *srcSqMat = gsl_matrix_alloc(srcMat->size1, srcMat->size1);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1, srcMat, srcTransMat, 0, srcSqMat);

    int signum;
    gsl_permutation* perm = gsl_permutation_alloc(srcMat->size1);
    gsl_linalg_LU_decomp(srcSqMat, perm, &signum);

    gsl_matrix *srcSqInvMat = gsl_matrix_alloc(srcSqMat->size1, srcSqMat->size1);
    gsl_linalg_LU_invert(srcSqMat, perm, srcSqInvMat);

    gsl_matrix *trgSrcMat = gsl_matrix_alloc(srcMat->size1, srcMat->size1);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1, trgMat, srcTransMat, 0, trgSrcMat);

    gsl_matrix *resMat = gsl_matrix_alloc(srcMat->size1, srcMat->size1);
    gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1, trgSrcMat, srcSqInvMat, 0, resMat);

    std::cout << "M = ";
    for (size_t i = 0; i < resMat->size1; ++i) {
        OfxRGBAColourD col;
        for (size_t j = 0; j < resMat->size2; ++j) {
            auto val = gsl_matrix_get(resMat, i, j);
            std::cout << val << " ";
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
