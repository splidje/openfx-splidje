#include "PatchMatcher.h"


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

    // initialise patch
    auto patchSize = _plugin->patchSize->getValueAtTime(args.time);
    _patch.count = patchSize * patchSize;
    _patch.offY = (patchSize-1) >> 1;
    _patch.offXs = new int[patchSize];
    _patch.endOffXs = _patch.offXs + patchSize;
    auto r = _patch.offY + 0.5;
    auto rSq = r*r;
    for (int y=_patch.offY, *p=_patch.offXs; p < _patch.endOffXs; y--, p++) {
        if (!y) {
            *p = _patch.offY;
        } else if (y > 0) {
            *p = floor(sqrt(rSq - y*y));
        } else {
            *p = *(p + (y<<1));
        }
        std::cout << *p << std::endl;
    }

    _numLevels = _plugin->calculateNumLevelsAtTime(args.time);
    _endLevel = std::max(
        1, std::min(_numLevels, _plugin->endLevel->getValueAtTime(args.time))
    );
    _startLevel = std::max(
        1, std::min(_endLevel, _plugin->startLevel->getValueAtTime(args.time))
    );
    _iterations = _plugin->iterations->getValueAtTime(args.time);
    _acceptableScore = _plugin->acceptableScore->getValueAtTime(args.time);
    _radicalImpairmentWeight = _plugin->radicalImpairmentWeight->getValueAtTime(args.time);
    auto bA = _srcA->getRegionOfDefinition();
    auto bB = _srcB->getRegionOfDefinition();
    auto widthA = boundsWidth(bA);
    auto heightA = boundsHeight(bA);
    _maxDist = sqrt(widthA * widthA + heightA * heightA);
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
        _imgTrg->width, _imgTrg->height, 3
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
                    ,dataPix
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

void PatchMatcher::averagerAdd(int x, int y) {
    _averager.xs[_averager.count] = x;
    _averager.ys[_averager.count] = y;
    _averager.sumX += x;
    _averager.sumY += y;
    _averager.count++;
}

bool PatchMatcher::propagateAndSearch(int iterNum, int iterLen)
{
    int count = 0;
    int dir = iterNum % 2 ? -1 : 1;
    int x, y;
    bool allAcceptable = true;
    OfxPointI prevXV;
    OfxPointI prevYV;
    OfxPointI neighV;
    int neighX, neighY;
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
            auto xPastStart = x > 0;
            auto xBeforeEnd = x < _imgVect->width - 1;
            auto yPastStart = y > 0;
            auto yBeforeEnd = y < _imgVect->height - 1;
            auto prevX = x - dir;
            auto prevY = y - dir;
            auto nextX = x + dir;
            auto nextY = y + dir;
            auto hasPrevX = dir > 0 && xPastStart || dir < 0 && xBeforeEnd;
            auto hasPrevY = dir > 0 && yPastStart || dir < 0 && yBeforeEnd;
            if (hasPrevX) {prevXV = _imgVect->vect(prevX, y);}
            if (hasPrevY) {prevYV = _imgVect->vect(x, prevY);}
            auto hasNextX = dir > 0 && xBeforeEnd || dir < 0 && xPastStart;
            auto hasNextY = dir > 0 && yBeforeEnd || dir < 0 && yPastStart;
            _averager.count = 0;
            _averager.sumX = 0;
            _averager.sumY = 0;
            // prev column
            if (hasPrevX) {
                averagerAdd(prevX + prevXV.x, prevY + prevXV.y);
                if (hasPrevY) {
                    neighY = prevY;
                    neighV = _imgVect->vect(prevX, neighY);
                    averagerAdd(prevX + neighV.x, neighY + neighV.y);
                }
                if (hasNextY) {
                    neighY = nextY;
                    neighV = _imgVect->vect(prevX, neighY);
                    averagerAdd(prevX + neighV.x, neighY + neighV.y);
                }
            }
            // prev row
            if (hasPrevY) {
                averagerAdd(x + prevYV.x, prevY + prevYV.y);
            }
            // next row
            if (hasNextY) {
                neighV = _imgVect->vect(x, nextY);
                averagerAdd(x + neighV.x, nextY + neighV.y);
            }
            // next column
            if (hasNextX) {
                neighV = _imgVect->vect(nextX, y);
                averagerAdd(nextX + neighV.x, y + neighV.y);
                if (hasPrevY) {
                    neighV = _imgVect->vect(nextX, prevY);
                    averagerAdd(nextX + neighV.x, prevY + neighV.y);
                }
                if (hasNextY) {
                    neighV = _imgVect->vect(nextX, nextY);
                    averagerAdd(nextX + neighV.x, nextY + neighV.y);
                }
            }
            // average
            auto avgX = _averager.sumX / double(_averager.count);
            auto avgY = _averager.sumY / double(_averager.count);
            double sumDistSq = 0;
            for (int i=0; i < _averager.count; i++) {
                auto dx = _averager.xs[i] - avgX;
                auto dy = _averager.ys[i] - avgY;
                sumDistSq += dx*dx + dy*dy;
            }
            double sd = sqrt(sumDistSq / _averager.count);
            if (_level == _endLevel && x == _logCoords.x && y == _logCoords.y) {
                std::cout << prevXV.x << "," << prevXV.y
                    << " " << prevYV.x << "," << prevYV.y
                    << " " << dir
                    << " " << avgX << "," << avgY
                    << " (" << sd << ")"
                    << std::endl;
            }

            // current value
            auto cur = _imgVect->pix(x, y);
            if (cur[2] >= 0 && cur[2] <= _acceptableScore) {continue;}
            allAcceptable = false;
            auto left = _imgVect->vect(x - dir, y);
            auto down = _imgVect->vect(x, y - dir);

            // propagate
            if (dir > 0 && x > 0 || dir < 0 && x < _imgVect->width - 1) {
                score(
                    x + prevXV.x, y + prevXV.y, x, y
                    ,cur, true, avgX, avgY, sd
                );
            }
            if (dir > 0 && y > 0 || dir < 0 && y < _imgVect->height - 1) {
                score(
                    x + prevYV.x, y + prevYV.y, x, y
                    ,cur, true, avgX, avgY, sd
                );
            }

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
                    ,cur, true, avgX, avgY, sd
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
                            ,bool haveIdeal, double idealX, double idealY, double idealRad)
{
    if (!_imgSrc->valid(xSrc, ySrc)) {return;}
    auto components = std::min(_imgSrc->components, _imgTrg->components);
    auto extCompsSrc = _imgSrc->components - components;
    auto extCompsTrg = _imgTrg->components - components;
    float total = 0;
    int count = 0;

    double impairment = 0;
    auto bestTotal = best[2];
    double bestImpairment = 0;
    auto haveBest = bestTotal >= 0;
    if (!haveBest) {
        bestTotal = MAXFLOAT;
    } else if (haveIdeal) {
        auto bestDX = xTrg + best[0] - idealX;
        auto bestDY = yTrg + best[1] - idealY;
        auto bestD = sqrt(bestDX*bestDX + bestDY*bestDY);
        if (bestD > idealRad) {
            bestImpairment = _radicalImpairmentWeight * (bestD - idealRad) / _maxDist;
            bestTotal += bestImpairment;
        }
        auto dX = xSrc - idealX;
        auto dY = ySrc - idealY;
        auto d = sqrt(dX*dX + dY*dY);
        if (d > idealRad) {
            impairment = _radicalImpairmentWeight * (d - idealRad) / _maxDist;
            total = impairment;
        }
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

    initScan(xSrc, ySrc, xTrg, yTrg);
    for (int y=0; y < _scan.numRows; y++) {
        for (int x=0; x < _scan.numCols; x++) {
            for (int c=0; c < components; c++, _scan.pixSrc++, _scan.pixTrg++) {
                auto diff = *(_scan.pixTrg) - *(_scan.pixSrc);
                if (diff < 0) {total -= diff;}
                else {total += diff;}
                if (total >= bestTotal) {
                    if (logIt) {std::cout << "lose " << total << std::endl;}
                    return;
                }
            }
            _scan.pixSrc += extCompsSrc;
            _scan.pixTrg += extCompsTrg;
            count++;
        }
        nextScanRow();
    }
    if (count < _patch.count) {
        total *= _patch.count / double(count);
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

void PatchMatcher::initScan(int xSrc, int ySrc, int xTrg, int yTrg) {
    _scan._offXMax = -std::min(_patch.offY, std::min(xSrc, xTrg));
    _scan._offXMin = std::min(
        _patch.offY + 1
        ,std::min(_imgSrc->width - xSrc, _imgTrg->width - xTrg)
    );
    auto startOffY = -std::min(_patch.offY, std::min(ySrc, yTrg));
    _scan.numRows = std::min(
        _patch.offY + 1
        ,std::min(_imgSrc->height - ySrc, _imgTrg->height - yTrg)
    ) - startOffY;
    _scan._offX = _patch.offXs + (startOffY + _patch.offY);
    _scan._curOffX = std::max(-*(_scan._offX), _scan._offXMax);
    _scan.numCols = std::min(*(_scan._offX) + 1, _scan._offXMin) - _scan._curOffX;
    _scan.pixSrc = _imgSrc->pix(xSrc + _scan._curOffX, ySrc + startOffY);
    _scan.pixTrg = _imgTrg->pix(xTrg + _scan._curOffX, yTrg + startOffY);
}

void PatchMatcher::nextScanRow() {
    _scan._offX++;
    if (_scan._offX >= _patch.endOffXs) {return;}
    _scan._curOffX += _scan.numCols;
    auto diffX = _scan._curOffX - std::max(-*(_scan._offX), _scan._offXMax);
    _scan.pixSrc += (_imgSrc->width - diffX) * _imgSrc->components;
    _scan.pixTrg += (_imgTrg->width - diffX) * _imgTrg->components;
    _scan._curOffX -= diffX;
    _scan.numCols = std::min(*(_scan._offX) + 1, _scan._offXMin) - _scan._curOffX;
}
