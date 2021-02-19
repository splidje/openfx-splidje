#include "PatchMatchPlugin.h"
#include "ofxsCoords.h"

PatchMatchPlugin::PatchMatchPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
{
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    assert(_dstClip && (_dstClip->getPixelComponents() == ePixelComponentRGB ||
    	    _dstClip->getPixelComponents() == ePixelComponentRGBA));
    _srcAClip = fetchClip(kSourceAClip);
    assert(_srcAClip && (_srcAClip->getPixelComponents() == ePixelComponentRGB ||
    	    _srcAClip->getPixelComponents() == ePixelComponentRGBA));
    _srcBClip = fetchClip(kSourceBClip);
    assert(_srcBClip && (_srcBClip->getPixelComponents() == ePixelComponentRGB ||
    	    _srcBClip->getPixelComponents() == ePixelComponentRGBA));
    _patchSize = fetchIntParam(kParamPatchSize);
    _startLevel = fetchIntParam(kParamStartLevel);
    _endLevel = fetchIntParam(kParamEndLevel);
    _iterations = fetchDoubleParam(kParamIterations);
    _acceptableScore = fetchDoubleParam(kParamAcceptableScore);
    _similarityThreshold = fetchDoubleParam(kParamSimilarityThreshold);
    _randomSeed = fetchIntParam(kParamRandomSeed);
    _logCoords = fetchInt2DParam(kParamLogCoords);
}

// the overridden render function
void PatchMatchPlugin::render(const RenderArguments &args)
{
    // full source image
    auto_ptr<Image> srcA(_srcAClip->fetchImage(args.time, _srcAClip->getRegionOfDefinition(args.time)));
    // full target image
    auto_ptr<Image> srcB(_srcBClip->fetchImage(args.time, _srcBClip->getRegionOfDefinition(args.time)));

    std::srand(_randomSeed->getValueAtTime(args.time));

    auto patchSize = _patchSize->getValueAtTime(args.time);

    auto numLevels = calculateNumLevelsAtTime(args.time);
    auto startLevel = std::max(
        1, std::min(numLevels, _startLevel->getValueAtTime(args.time))
    );
    auto endLevel = std::max(
        1, std::min(numLevels, _endLevel->getValueAtTime(args.time))
    );

    auto iterations = _iterations->getValueAtTime(args.time);
    auto similarityThreshold = _similarityThreshold->getValueAtTime(args.time);

    _curAcceptableScore = _acceptableScore->getValueAtTime(args.time);

    _curLogCoords = _logCoords->getValueAtTime(args.time);

    double scale = 1;
    for (int l=numLevels; l > startLevel; l--) {scale *= 0.5;}
    auto_ptr<SimpleImage> imgVect;
    for (int level=startLevel; level <= endLevel; level++, scale *= 2) {
        _finalLevel = level == endLevel;

        // resample input images
        auto_ptr<SimpleImage> imgA(resample(srcA.get(), scale));
        if (abort()) {return;}
        auto_ptr<SimpleImage> imgB(resample(srcB.get(), scale));
        if (abort()) {return;}

        // initialise        
        imgVect.reset(initialiseLevel(
            imgA.get(), imgB.get(), imgVect.get()
            ,patchSize, similarityThreshold
        ));
        if (abort()) {return;}

        // iterate propagate and search
        int iterationLength = (iterations - floor(iterations)) * imgB->width * imgB->height;
        for (int i=0; i < iterations; i++) {
            int len = 0;
            if (level == endLevel && i + 1 > iterations) {
                len = iterationLength;
            }
            if (propagateAndSearch(
                imgVect.get(), imgA.get(), imgB.get()
                ,patchSize, similarityThreshold, i, len
            )) {break;}
            if (abort()) {return;}
        }
    }
    auto bA = srcA->getRegionOfDefinition();
    auto bB = srcB->getRegionOfDefinition();
    auto offX = bA.x1 - bB.x1;
    auto offY = bA.y1 - bB.y1;
    srcA.reset();
    srcB.reset();

    // get a dst image
    auto_ptr<Image> dst( _dstClip->fetchImage(args.time) );

    auto dstRoD = dst->getRegionOfDefinition();
    auto dstComponents = dst->getPixelComponentCount();

    for (int y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        if (abort()) {return;}
        for (int x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            auto dstPix = (float*)dst->getPixelAddress(x, y);
            auto inX = x - dstRoD.x1;
            auto inY = y - dstRoD.y1;
            float* outPix = NULL;
            if (inX >= 0 && inX < imgVect->width
                    && inY >= 0 && inY < imgVect->height) {
                outPix = imgVect->data + ((inY * imgVect->width + inX) * imgVect->components);
            }
            for (int c=0; c < dstComponents; c++, dstPix++) {
                if (outPix && c < imgVect->components) {
                    *dstPix = *outPix;
                    outPix++;
                }
                else {
                    *dstPix = 0;
                }
                switch (c) {
                    case 0:
                        *dstPix += offX;
                        break;
                    case 1:
                        *dstPix += offY;
                        break;
                }
            }
        }
    }
}

bool PatchMatchPlugin::isIdentity(const IsIdentityArguments &args, 
                                  Clip * &identityClip, double &/*identityTime*/
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
)
{
    return false;
}

bool PatchMatchPlugin::getRegionOfDefinition(const RegionOfDefinitionArguments &args, OfxRectD &rod)
{
    auto numLevels = calculateNumLevelsAtTime(args.time);
    auto endLevel = std::max(
        1, std::min(numLevels, _endLevel->getValueAtTime(args.time))
    );
    rod = _srcBClip->getRegionOfDefinition(args.time);
    double scale = 1;
    for (int l=numLevels; l > endLevel; l--) {scale *= 0.5;}
    rod.x1 *= scale;
    rod.x2 *= scale;
    rod.y1 *= scale;
    rod.y2 *= scale;
    return true;
}

int PatchMatchPlugin::calculateNumLevelsAtTime(double time)
{
    auto boundsA = _srcAClip->getRegionOfDefinition(time);
    auto boundsB = _srcBClip->getRegionOfDefinition(time);
    auto minDim = std::min(
        std::min(boundsWidth(boundsA), boundsWidth(boundsB))
        ,std::min(boundsHeight(boundsA), boundsHeight(boundsB))
    );
    auto patchSize = _patchSize->getValueAtTime(time);
    if (minDim <= patchSize) {
        return 1;
    }
    int numLevels = int(std::log2(double(minDim) / patchSize)) + 1;
    _startLevel->setDisplayRange(1, numLevels);
    _endLevel->setDisplayRange(1, numLevels);
    return numLevels;
}

SimpleImage* PatchMatchPlugin::resample(const Image* image, double scale)
{
    auto bounds = image->getRegionOfDefinition();
    auto width = boundsWidth(bounds);
    auto height = boundsHeight(bounds);
    auto components = image->getPixelComponentCount();
    auto data = (float*)image->getPixelData();
    if (scale == 1) {
        return new SimpleImage(
            width, height, components, data
        );
    }
    auto simg = new SimpleImage(
        std::max(1, int(round(width * scale)))
        ,std::max(1, int(round(height * scale)))
        ,components
    );
    auto pix = simg->data;
    auto sampleSize = 1 / scale;
    auto totals = new double[components];
    float* inPix;
    double sampleX1f, sampleX2f, sampleY1f, sampleY2f;
    int sampleX1i, sampleX2i, sampleY1i, sampleY2i;
    double fracX1, fracX2, fracY1, fracY2;
    double rowWeight, weight;
    int sampWidthWholes, sampHeightWholes;
    double totalWeight;
    for (int y=0; y < simg->height; y++) {
        if (abort()) {break;}
        for (int x=0; x < simg->width; x++) {
            sampleX1f = x * sampleSize;
            sampleX2f = std::min(double(width), (x + 1) * sampleSize);
            sampleY1f = y * sampleSize;
            sampleY2f = std::min(double(height), (y + 1) * sampleSize);
            sampleX1i = floor(sampleX1f);
            sampleX2i = ceil(sampleX2f);
            sampleY1i = floor(sampleY1f);
            sampleY2i = ceil(sampleY2f);
            fracX1 = sampleX1f - sampleX1i;
            fracX2 = sampleX2i - sampleX2f;
            fracY1 = sampleY1f - sampleY1i;
            fracY2 = sampleY2i - sampleY2f;
            for (int c=0; c < components; c++) {totals[c] = 0;}
            for (int sy=sampleY1i; sy < sampleY2i; sy++) {
                rowWeight = 1;
                if (fracY1 && sy == sampleY1i) {rowWeight *= fracY1;}
                else if (fracY2 && sy == sampleY2i - 1) {rowWeight *= fracY2;}
                inPix = data + (sy * width + sampleX1i) * components;
                for (int sx=sampleX1i; sx < sampleX2i; sx++) {
                    weight = rowWeight;
                    if (fracX1 && sx == sampleX1i) {weight *= fracX1;}
                    else if (fracX2 && sx == sampleX2i - 1) {weight *= fracX2;}
                    for (int c=0; c < components; c++, inPix++) {
                        totals[c] += (*inPix) * weight;
                    }
                }
            }
            sampWidthWholes = floor(sampleX2f) - ceil(sampleX1f);
            sampHeightWholes = floor(sampleY2f) - ceil(sampleY1f);
            totalWeight = (
                sampWidthWholes * sampHeightWholes
                + sampHeightWholes * (fracX1 + fracX2)
                + sampWidthWholes * (fracY1 + fracY2)
                + fracX1 * fracY1
                + fracX1 * fracY2
                + fracX2 * fracY1
                + fracX2 * fracY2
            );
            if (totalWeight == 0) {std::cout << x << " " << y << std::endl;}
            for (int c=0; c < simg->components; c++, pix++) {
                *pix = totals[c] / totalWeight;
            }
        }
    }
    delete[] totals;
    if (abort()) {
        delete simg;
        return NULL;
    }
    return simg;
}

SimpleImage* PatchMatchPlugin::initialiseLevel(SimpleImage* imgSrc, SimpleImage* imgTrg
                                              ,SimpleImage* imgPrev
                                              ,int patchSize, float threshold)
{
    auto img = new SimpleImage(imgTrg->width, imgTrg->height, 3);
    auto dataPix = img->data;
    int prevStepX, prevStepY;
    float* prevRow;
    if (imgPrev) {
        prevRow = imgPrev->data;
        prevStepX = round(img->width / double(imgPrev->width));
        prevStepY = round(img->height / double(imgPrev->height));
    }
    for (int y=0, pY=0; y < img->height; y++) {
        if (abort()) {
            delete img;
            return NULL;
        }
        auto prevCell = prevRow;
        for (int x=0, pX=0; x < img->width; x++) {
            dataPix[0] = rand() % imgSrc->width - x;
            dataPix[1] = rand() % imgSrc->height - y;
            dataPix[2] = -1;
            score(x + dataPix[0], y + dataPix[1], x, y, imgSrc, imgTrg
                ,patchSize, threshold, dataPix);
            if (imgPrev) {
                score(
                    x + prevCell[0], y + prevCell[1]
                    ,x, y
                    ,imgSrc, imgTrg
                    ,patchSize, threshold
                    ,dataPix
                );
                if (x % prevStepX && pX < imgPrev->width) {
                    pX++;
                    prevCell += imgPrev->components;
                }
            }
            dataPix += img->components;
        }
        if (imgPrev && y % prevStepY && pY < imgPrev->height) {
            pY++;
            prevRow += imgPrev->width * imgPrev->components;
        }
    }
    return img;
}

bool PatchMatchPlugin::propagateAndSearch(SimpleImage* imgVect, SimpleImage* imgSrc
                                         ,SimpleImage* imgTrg
                                         ,int patchSize, float threshold
                                         ,int iterationNum, int length)
{
    int count = 0;
    int dir = iterationNum % 2 ? -1 : 1;
    int x, y;
    bool allAcceptable = true;
    for (int yi=0; yi < imgVect->height; yi++) {
        if (abort()) {return allAcceptable;}
        for (int xi=0; xi < imgVect->width; xi++, count++) {
            if (length && count == length) {return allAcceptable;}

            if (dir < 0) {
                x = imgVect->width - 1 - xi;
                y = imgVect->height - 1 - yi;
            }
            else {
                x = xi;
                y = yi;
            }

            // propagate
            auto cur = imgVect->pix(x, y);
            if (cur[2] <= _curAcceptableScore) {continue;}
            allAcceptable = false;
            auto left = imgVect->vect(x - dir, y);
            score(
                x + left.x, y + left.y, x, y, imgSrc, imgTrg
                ,patchSize, threshold, cur
            );
            auto up = imgVect->vect(x, y - dir);
            score(
                x + up.x, y + up.y, x, y, imgSrc, imgTrg
                ,patchSize, threshold, cur
            );

            // search
            double radW = imgSrc->width / 2.0;
            double radH = imgSrc->height / 2.0;
            int srchCentX = x + cur[0];
            int srchCentY = y + cur[1];
            for (; radW >= 1 && radH >= 1; radW /= 2, radH /= 2) {
                int radWi = ceil(radW);
                int radHi = ceil(radH);
                auto l = std::max(0, srchCentX - radWi);
                auto b = std::max(0, srchCentY - radHi);
                auto w = std::min(imgSrc->width, srchCentX + radWi + 1) - l;
                auto h = std::min(imgSrc->height, srchCentY + radHi + 1) - b;
                auto sX = rand() % w + l;
                auto sY = rand() % h + b;
                score(
                    sX, sY, x, y, imgSrc, imgTrg
                    ,patchSize, threshold, cur
                );
            }
        }
    }
    return allAcceptable;
}

inline void distSq(int dX, int dY, int &dSq)
{
    if (dSq >= 0) {return;}
    dSq = dX*dX + dY*dY;
}

inline bool scoreCarryOn(float total, float lowLim, float upLim
                        ,int vX, int vY, int bestVX, int bestVY
                        ,bool &reachedThreshold, int &dSq, int &dSqBest)
{
    if (total > upLim) {return false;}
    if (!reachedThreshold && total >= lowLim) {
        reachedThreshold = true;
        distSq(vX, vY, dSq);
        distSq(bestVX, bestVY, dSqBest);
        if (dSqBest <= dSq) {return false;}
    }
    return true;
}

void PatchMatchPlugin::score(int xSrc, int ySrc, int xTrg, int yTrg
                              ,SimpleImage* imgSrc, SimpleImage* imgTrg
                              ,int patchSize, float threshold, float* best)
{
    if (!imgSrc->valid(xSrc, ySrc)) {
        if (_finalLevel
            && _curLogCoords.x == xTrg
            && _curLogCoords.y == yTrg
        ) {
            std::cout << xTrg << "," << yTrg << "-" << xSrc << "," << ySrc
                << ": src invalid" << std::endl;
        }
        return;
    }
    int components = std::min(imgSrc->components, imgTrg->components);
    float total = 0;
    int count = 0;
    auto pOff = (patchSize-1) >> 1;
    int vX = xSrc - xTrg;
    int vY = ySrc - yTrg;
    int bestVX;
    int bestVY;
    auto bestTotal = best[2];
    bool haveBest = bestTotal >= 0;
    float upLim = -1, lowLim = -1;
    int dSq = -1;
    int dSqBest = -1;
    bool reachedThreshold = false;
    if (haveBest) {
        bestVX = best[0];
        bestVY = best[1];
        upLim = bestTotal + threshold;
        lowLim = bestTotal - threshold;
    }
    for (int yOff=-pOff; yOff <= pOff; yOff++) {
        for (int xOff=-pOff; xOff <= pOff; xOff++) {
            auto xxTrg = xTrg + xOff;
            auto yyTrg = yTrg + yOff;
            auto xxSrc = xSrc + xOff;
            auto yySrc = ySrc + yOff;
            if (
                !imgSrc->valid(xxSrc, yySrc)
                || !imgTrg->valid(xxTrg, yyTrg)
            ) {continue;}
            auto pixSrc = imgSrc->pix(xxSrc, yySrc);
            auto pixTrg = imgTrg->pix(xxTrg, yyTrg);
            for (int c=0; c < components; c++, pixSrc++, pixTrg++) {
                auto diff = *pixTrg - *pixSrc;
                if (diff < 0) {total -= diff;}
                else {total += diff;}
                if (
                    haveBest
                    && !scoreCarryOn(total, lowLim, upLim
                                    ,vX, vY, bestVX, bestVY
                                    ,reachedThreshold
                                    ,dSq, dSqBest)
                ) {
                    if (_finalLevel
                        && _curLogCoords.x == xTrg
                        && _curLogCoords.y == yTrg
                    ) {
                        std::cout << xTrg << "," << yTrg << "-" << xSrc << "," << ySrc
                            << ": give up at " << total
                            << " (" << lowLim << "," << upLim << ") "
                            << " (" << dSq << " " << dSqBest << ")" << std::endl;
                    }
                    return;
                }
            }
            count++;
        }
    }
    auto maxCount = patchSize * patchSize;
    if (count < maxCount) {
        total *= maxCount / double(count);
    }
    if (
        haveBest
        && !scoreCarryOn(total, lowLim, upLim
                        ,vX, vY, bestVX, bestVY
                        ,reachedThreshold
                        ,dSq, dSqBest)
    ) {
        if (_finalLevel
            && _curLogCoords.x == xTrg
            && _curLogCoords.y == yTrg
        ) {
            std::cout << xTrg << "," << yTrg << "-" << xSrc << "," << ySrc
                << ": lose " << total
                << " (" << lowLim << "," << upLim << ")"
                << " (" << dSq << " " << dSqBest << ")" << std::endl;
        }
        return;
    }
    best[0] = vX;
    best[1] = vY;
    best[2] = total;
    if (_finalLevel
        && _curLogCoords.x == xTrg
        && _curLogCoords.y == yTrg
    ) {
        std::cout << xTrg << "," << yTrg << "-" << xSrc << "," << ySrc
            << ": win! " << total
            << " (" << lowLim << "," << upLim << ")" << std::endl;
    }
}
