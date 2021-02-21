#include "PatchMatcher.h"


// TODO: struct representing different patch shapes (e.g. circle)


PatchMatcher::PatchMatcher(PatchMatchPlugin* plugin, const RenderArguments &args)
    : _plugin(plugin)
    , _renderArgs(args) {
    // full source image
    _srcA.reset(
        _plugin->srcClip->fetchImage(
            args.time, _plugin->srcClip->getRegionOfDefinition(args.time)
        )
    );
    // full target image
    _srcB.reset(
        _plugin->trgClip->fetchImage(
            args.time, _plugin->trgClip->getRegionOfDefinition(args.time)
        )
    );
    std::srand(_plugin->randomSeed->getValueAtTime(args.time));
    _logCoords = _plugin->logCoords->getValueAtTime(args.time);
    _logCoords.x /= args.renderScale.x;
    _logCoords.y /= args.renderScale.y;
    // std::cout << args.renderScale.x << "," << args.renderScale.y << std::endl;
    _patchSize = _plugin->patchSize->getValueAtTime(args.time);
    _numLevels = _plugin->calculateNumLevelsAtTime(args.time);
    _startLevel = std::max(
        1, std::min(_numLevels, _plugin->startLevel->getValueAtTime(args.time))
    );
    _endLevel = std::max(
        1, std::min(_numLevels, _plugin->endLevel->getValueAtTime(args.time))
    );
    _iterations = _plugin->iterations->getValueAtTime(args.time);
    _acceptableScore = _plugin->acceptableScore->getValueAtTime(args.time);
    _radicalImpairmentWeight = _plugin->radicalImpairmentWeight->getValueAtTime(args.time);
    _radicalImpairmentSquared = _plugin->radicalImpairmentSquared->getValueAtTime(args.time);
    auto bA = _srcA->getRegionOfDefinition();
    auto bB = _srcB->getRegionOfDefinition();
    auto widthA = boundsWidth(bA);
    auto heightA = boundsHeight(bA);
    _maxDistSq = widthA * widthA + heightA * heightA;
    _offX = bA.x1 - bB.x1;
    _offY = bA.y1 - bB.y1;
}

void PatchMatcher::render() {
    double scale = 1;
    for (int l=_numLevels; l > _startLevel; l--) {scale *= 0.5;}
    for (_level=_startLevel; _level <= _endLevel; _level++, scale *= 2) {
        // resample input images
        _imgSrc.reset(resample(_srcA.get(), scale));
        if (_plugin->abort()) {return;}
        _imgTrg.reset(resample(_srcB.get(), scale));
        if (_plugin->abort()) {return;}

        // initialise
        initialiseLevel();
        if (_plugin->abort()) {return;}

        // iterate propagate and search
        int lastIterationLength = (_iterations - floor(_iterations)) * _imgTrg->width * _imgTrg->height;
        for (int i=0; i < _iterations; i++) {
            int len = 0;
            if (_level == _endLevel && (i + 1) > _iterations) {
                len = lastIterationLength;
            }
            if (propagateAndSearch(i, len)) {break;}
            if (_plugin->abort()) {return;}
        }
    }
    _imgSrc.reset();
    _imgTrg.reset();
    _srcA.reset();
    _srcB.reset();

    // get a dst image
    auto_ptr<Image> dst(_plugin->dstClip->fetchImage(_renderArgs.time));

    auto dstRoD = dst->getRegionOfDefinition();
    auto dstComponents = dst->getPixelComponentCount();

    for (int y=_renderArgs.renderWindow.y1; y < _renderArgs.renderWindow.y2; y++) {
        if (_plugin->abort()) {return;}
        for (int x=_renderArgs.renderWindow.x1; x < _renderArgs.renderWindow.x2; x++) {
            auto dstPix = (float*)dst->getPixelAddress(x, y);
            auto inX = x - dstRoD.x1;
            auto inY = y - dstRoD.y1;
            float* outPix = NULL;
            if (inX >= 0 && inX < _imgVect->width
                    && inY >= 0 && inY < _imgVect->height) {
                outPix = _imgVect->data + ((inY * _imgVect->width + inX) * _imgVect->components);
            }
            for (int c=0; c < dstComponents; c++, dstPix++) {
                if (outPix && c < _imgVect->components) {
                    *dstPix = *outPix;
                    outPix++;
                }
                else {
                    *dstPix = 0;
                }
                switch (c) {
                    case 0:
                        *dstPix += _offX;
                        *dstPix /= _renderArgs.renderScale.x;
                        break;
                    case 1:
                        *dstPix += _offY;
                        *dstPix /= _renderArgs.renderScale.y;
                        break;
                }
            }
        }
    }
}

SimpleImage* PatchMatcher::resample(const Image* image, double scale)
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
        if (_plugin->abort()) {break;}
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
    if (_plugin->abort()) {
        delete simg;
        return NULL;
    }
    return simg;
}

void PatchMatcher::initialiseLevel()
{
    auto_ptr<SimpleImage> img(new SimpleImage(
        _imgTrg->width, _imgTrg->height, 4
    ));
    auto dataPix = img->data;
    double prevScaleX, prevScaleY;
    int prevStepX, prevStepY;
    float* prevRow = NULL;
    if (_imgVect.get()) {
        prevRow = _imgVect->data;
        prevScaleX = img->width / double(_imgVect->width);
        prevScaleY = img->height / double(_imgVect->height);
        prevStepX = round(prevScaleX);
        prevStepY = round(prevScaleY);
    }
    for (int y=0, pY=0; y < img->height; y++) {
        if (_plugin->abort()) {return;}
        auto prevCell = prevRow;
        for (int x=0, pX=0; x < img->width; x++) {
            dataPix[0] = rand() % _imgSrc->width - x;
            dataPix[1] = rand() % _imgSrc->height - y;
            dataPix[2] = -1;
            dataPix[3] = -1;
            score(x + dataPix[0], y + dataPix[1], x, y, dataPix);
            if (prevRow) {
                score(
                    x + prevCell[0] * prevScaleX
                    ,y + prevCell[1] * prevScaleY
                    ,x, y
                    , dataPix
                );
                if (x % prevStepX && pX < _imgVect->width) {
                    pX++;
                    prevCell += _imgVect->components;
                }
            }
            dataPix += img->components;
        }
        if (prevRow && y % prevStepY && pY < _imgVect->height) {
            pY++;
            prevRow += _imgVect->width * _imgVect->components;
        }
    }
    _imgVect.reset(img.get());
    img.release();
}

bool PatchMatcher::propagateAndSearch(int iterNum, int iterLen)
{
    int count = 0;
    int dir = iterNum % 2 ? -1 : 1;
    int x, y;
    bool allAcceptable = true;
    for (int yi=0; yi < _imgVect->height; yi++) {
        if (_plugin->abort()) {return allAcceptable;}
        for (int xi=0; xi < _imgVect->width; xi++, count++) {
            if (iterLen && count == iterLen) {return allAcceptable;}

            if (dir < 0) {
                x = _imgVect->width - 1 - xi;
                y = _imgVect->height - 1 - yi;
            }
            else {
                x = xi;
                y = yi;
            }

            // get neighbours

            auto cur = _imgVect->pix(x, y);
            if (cur[2] >= 0 && cur[2] <= _acceptableScore) {continue;}
            allAcceptable = false;
            auto left = _imgVect->vect(x - dir, y);
            auto down = _imgVect->vect(x, y - dir);

            auto leftRX = left.x - dir;
            auto downRY = down.y - dir;
            float hypVX_2 = (down.x - leftRX) / 2;
            float hypVY_2 = (downRY - left.y) / 2;
            int idealX = round(leftRX + hypVX_2 - hypVY_2) + x;
            int idealY = round(left.y + hypVY_2 + hypVX_2) + y;
            if (_level == _endLevel && x == _logCoords.x && y == _logCoords.y) {
                std::cout << left.x << "," << left.y
                    << " " << down.x << "," << down.y
                    << " " << dir
                    << " " << idealX << "," << idealY
                    << std::endl;
            }

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
            double radW = _imgSrc->width / 2.0;
            double radH = _imgSrc->height / 2.0;
            int srchCentX = x + cur[0];
            int srchCentY = y + cur[1];
            for (; radW >= 1 && radH >= 1; radW /= 2, radH /= 2) {
                int radWi = ceil(radW);
                int radHi = ceil(radH);
                auto l = std::max(0, srchCentX - radWi);
                auto b = std::max(0, srchCentY - radHi);
                auto w = std::min(_imgSrc->width, srchCentX + radWi + 1) - l;
                auto h = std::min(_imgSrc->height, srchCentY + radHi + 1) - b;
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

void PatchMatcher::score(int xSrc, int ySrc, int xTrg, int yTrg
                            ,float* best
                            ,bool haveIdeal, int idealX, int idealY)
{
    if (!_imgSrc->valid(xSrc, ySrc)) {return;}
    int components = std::min(_imgSrc->components, _imgTrg->components);
    float total = 0;
    int count = 0;
    auto pOff = (_patchSize-1) >> 1;

    double impairment = 0;
    auto bestTotal = best[2];
    double bestImpairment = 0;
    auto haveBest = bestTotal >= 0;
    if (!haveBest) {
        bestTotal = MAXFLOAT;
    } else if (haveIdeal) {
        auto bestRadX = xTrg + best[0] - idealX;
        auto bestRadY = yTrg + best[1] - idealY;
        auto bestImpUnweigh = (bestRadX*bestRadX + bestRadY*bestRadY) / _maxDistSq;
        if (!_radicalImpairmentSquared) {
            bestImpUnweigh = sqrt(bestImpUnweigh);
        }
        bestImpairment = _radicalImpairmentWeight * bestImpUnweigh;
        bestTotal += bestImpairment;
        auto radX = xSrc - idealX;
        auto radY = ySrc - idealY;
        auto impUnweigh = (radX*radX + radY*radY) / _maxDistSq;
        if (!_radicalImpairmentSquared) {
            impUnweigh = sqrt(impUnweigh);
        }
        impairment = _radicalImpairmentWeight * impUnweigh;
        total = impairment;
        best[3] = impairment;
    }

    auto logIt = _level == _endLevel && _logCoords.x == xTrg && _logCoords.y == yTrg;
    if (logIt) {
        std::cout << "t:" << xTrg << "," << yTrg
            << " s:" << xSrc << "," << ySrc
            << " t:" << total << " imp:" << impairment
            << " bt:" << bestTotal;
        if (haveBest) {
            std::cout << " b:"
                << best[2] << " ["
                << (xTrg + best[0]) << " (" << best[0] << "),"
                << (yTrg + best[1]) << " (" << best[1] << ")"
                << "]";
            if (haveIdeal) {
                std::cout << " idl:" << idealX << "," << idealY
                    << " bimp:" << bestImpairment;
            }
        }
        std::cout << std::endl;
    }

    for (int yOff=-pOff; yOff <= pOff; yOff++) {
        for (int xOff=-pOff; xOff <= pOff; xOff++) {
            auto xxTrg = xTrg + xOff;
            auto yyTrg = yTrg + yOff;
            auto xxSrc = xSrc + xOff;
            auto yySrc = ySrc + yOff;
            if (
                !_imgSrc->valid(xxSrc, yySrc)
                || !_imgTrg->valid(xxTrg, yyTrg)
            ) {continue;}
            auto pixSrc = _imgSrc->pix(xxSrc, yySrc);
            auto pixTrg = _imgTrg->pix(xxTrg, yyTrg);
            for (int c=0; c < components; c++, pixSrc++, pixTrg++) {
                auto diff = *pixTrg - *pixSrc;
                if (diff < 0) {total -= diff;}
                else {total += diff;}
                if (total >= bestTotal) {
                    if (logIt) {std::cout << "lose " << total << std::endl;}
                    return;
                }
            }
            count++;
        }
    }
    auto maxCount = _patchSize * _patchSize;
    if (count < maxCount) {
        total *= maxCount / double(count);
    }
    if (total >= bestTotal) {
        if (logIt) {std::cout << "lose " << total << std::endl;}
        return;
    }
    best[0] = xSrc - xTrg;
    best[1] = ySrc - yTrg;
    best[2] = total - impairment;
    if (logIt) {
        std::cout << "win! " << best[2]
            << " " << best[0] << "," << best[1]
            << std::endl;
    }
}
