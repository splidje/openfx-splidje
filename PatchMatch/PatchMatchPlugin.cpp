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
    _radicalImpairmentWeight = fetchDoubleParam(kParamRadicalImpairmentWeight);
    _randomSeed = fetchIntParam(kParamRandomSeed);
}

// the overridden render function
void PatchMatchPlugin::render(const RenderArguments &args)
{
    // full source image
    auto_ptr<Image> srcA(_srcAClip->fetchImage(args.time, _srcAClip->getRegionOfDefinition(args.time)));
    // full target image
    auto_ptr<Image> srcB(_srcBClip->fetchImage(args.time, _srcBClip->getRegionOfDefinition(args.time)));

    std::srand(_randomSeed->getValueAtTime(args.time));

    _curPatchSize = _patchSize->getValueAtTime(args.time);

    auto numLevels = calculateNumLevelsAtTime(args.time);
    auto startLevel = std::max(
        1, std::min(numLevels, _startLevel->getValueAtTime(args.time))
    );
    auto endLevel = std::max(
        1, std::min(numLevels, _endLevel->getValueAtTime(args.time))
    );

    auto iterations = _iterations->getValueAtTime(args.time);

    _curAcceptableScore = _acceptableScore->getValueAtTime(args.time);

    _curRIW = _radicalImpairmentWeight->getValueAtTime(args.time);

    auto bA = srcA->getRegionOfDefinition();
    auto bB = srcB->getRegionOfDefinition();

    auto widthA = boundsWidth(bA);
    auto heightA = boundsHeight(bA);
    _maxDistSq = widthA * widthA + heightA * heightA;

    double scale = 1;
    for (int l=numLevels; l > startLevel; l--) {scale *= 0.5;}
    auto_ptr<SimpleImage> imgVect;
    for (int level=startLevel; level <= endLevel; level++, scale *= 2) {
        // resample input images
        auto_ptr<SimpleImage> imgA(resample(srcA.get(), scale));
        if (abort()) {return;}
        auto_ptr<SimpleImage> imgB(resample(srcB.get(), scale));
        if (abort()) {return;}

        _curImgSrc = imgA.get();
        _curImgTrg = imgB.get();

        // initialise
        imgVect.reset(initialiseLevel(imgVect.get()));
        if (abort()) {return;}

        _curImgVect = imgVect.get();

        // iterate propagate and search
        int iterationLength = (iterations - floor(iterations)) * imgB->width * imgB->height;
        for (int i=0; i < iterations; i++) {
            _curIterationNum = i;
            _curLength = 0;
            if (level == endLevel && i + 1 > iterations) {
                _curLength = iterationLength;
            }
            if (propagateAndSearch()) {break;}
            if (abort()) {return;}
        }
    }
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

SimpleImage* PatchMatchPlugin::initialiseLevel(SimpleImage* imgPrev)
{
    auto img = new SimpleImage(
        _curImgTrg->width, _curImgTrg->height, 4
    );
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
            dataPix[0] = rand() % _curImgSrc->width - x;
            dataPix[1] = rand() % _curImgSrc->height - y;
            dataPix[2] = -1;
            dataPix[3] = -1;
            score(x + dataPix[0], y + dataPix[1], x, y, dataPix);
            if (imgPrev) {
                score(
                    x + prevCell[0], y + prevCell[1]
                    ,x, y, dataPix
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

bool PatchMatchPlugin::propagateAndSearch()
{
    int count = 0;
    int dir = _curIterationNum % 2 ? -1 : 1;
    int x, y;
    bool allAcceptable = true;
    for (int yi=0; yi < _curImgVect->height; yi++) {
        if (abort()) {return allAcceptable;}
        for (int xi=0; xi < _curImgVect->width; xi++, count++) {
            if (_curLength && count == _curLength) {return allAcceptable;}

            if (dir < 0) {
                x = _curImgVect->width - 1 - xi;
                y = _curImgVect->height - 1 - yi;
            }
            else {
                x = xi;
                y = yi;
            }

            // get neighbours

            auto cur = _curImgVect->pix(x, y);
            if (cur[2] >= 0 && cur[2] <= _curAcceptableScore) {continue;}
            allAcceptable = false;
            auto left = _curImgVect->vect(x - dir, y);
            auto down = _curImgVect->vect(x, y - dir);

            auto leftRX = left.x - dir;
            auto downRY = down.y - dir;
            float hypVX_2 = (down.x - leftRX) / 2;
            float hypVY_2 = (downRY - left.y) / 2;
            int idealX = round(leftRX + hypVX_2 - hypVY_2);
            int idealY = round(left.y + hypVY_2 + hypVX_2);
            // std::cout << left.x << "," << left.y
            //     << " " << down.x << "," << down.y << " " << dir
            //     << " " << idealX << "," << idealY << std::endl;

            // propagate
            score(
                x + left.x, y + left.y, x, y
                ,cur, true, idealX, idealY
            );
            score(
                x + down.x, y + down.y, x, y
                ,cur, true, idealX, idealY
            );

            // search
            double radW = _curImgSrc->width / 2.0;
            double radH = _curImgSrc->height / 2.0;
            int srchCentX = x + cur[0];
            int srchCentY = y + cur[1];
            for (; radW >= 1 && radH >= 1; radW /= 2, radH /= 2) {
                int radWi = ceil(radW);
                int radHi = ceil(radH);
                auto l = std::max(0, srchCentX - radWi);
                auto b = std::max(0, srchCentY - radHi);
                auto w = std::min(_curImgSrc->width, srchCentX + radWi + 1) - l;
                auto h = std::min(_curImgSrc->height, srchCentY + radHi + 1) - b;
                auto sX = rand() % w + l;
                auto sY = rand() % h + b;
                score(
                    sX, sY, x, y
                    ,cur, true, idealX, idealY
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

void PatchMatchPlugin::score(int xSrc, int ySrc, int xTrg, int yTrg
                            ,float* best
                            ,bool haveIdeal, int idealX, int idealY)
{
    if (!_curImgSrc->valid(xSrc, ySrc)) {return;}
    int components = std::min(_curImgSrc->components, _curImgTrg->components);
    float total = 0;
    int count = 0;
    auto pOff = (_curPatchSize-1) >> 1;

    double impairment = 0;
    auto bestTotal = best[2];
    auto haveBest = bestTotal >= 0;
    if (!haveBest) {
        bestTotal = MAXFLOAT;
    } else if (haveIdeal) {
        auto bestRadX = xTrg + best[0] - idealX;
        auto bestRadY = yTrg + best[1] - idealY;
        bestTotal += _curRIW * (bestRadX*bestRadX + bestRadY*bestRadY) / _maxDistSq;
        auto radX = xSrc - idealX;
        auto radY = ySrc - idealY;
        impairment = _curRIW * (radX*radX + radY*radY) / _maxDistSq;
        total = impairment;
        best[3] = impairment;
    }

    for (int yOff=-pOff; yOff <= pOff; yOff++) {
        for (int xOff=-pOff; xOff <= pOff; xOff++) {
            auto xxTrg = xTrg + xOff;
            auto yyTrg = yTrg + yOff;
            auto xxSrc = xSrc + xOff;
            auto yySrc = ySrc + yOff;
            if (
                !_curImgSrc->valid(xxSrc, yySrc)
                || !_curImgTrg->valid(xxTrg, yyTrg)
            ) {continue;}
            auto pixSrc = _curImgSrc->pix(xxSrc, yySrc);
            auto pixTrg = _curImgTrg->pix(xxTrg, yyTrg);
            for (int c=0; c < components; c++, pixSrc++, pixTrg++) {
                auto diff = *pixTrg - *pixSrc;
                if (diff < 0) {total -= diff;}
                else {total += diff;}
                if (total >= bestTotal) {return;}
            }
            count++;
        }
    }
    auto maxCount = _curPatchSize * _curPatchSize;
    if (count < maxCount) {
        total *= maxCount / double(count);
    }
    if (total >= bestTotal) {return;}
    best[0] = xSrc - xTrg;
    best[1] = ySrc - yTrg;
    best[2] = total - impairment;
}
