#include "PatchMatcher.h"
#include <ctime>

#define CACHE_SIZE 1000

time_t st_score;
double t_score = 0;
auto_ptr<SimpleImage> vectImageCache[CACHE_SIZE];
int nextVectImageCacheIdx = 0;
typedef std::pair<PatchMatchPlugin*, double> pluginFramePair;
std::map<pluginFramePair, int> pluginFrameToVectImageCacheIdx;
std::map<int, pluginFramePair> vectImageCacheIdxToPluginFrame;


SimpleImage* getCachedVectImage(PatchMatchPlugin* plugin, double frame) {
    pluginFramePair pluginFrame = {plugin, frame};
    auto resIdx = pluginFrameToVectImageCacheIdx.find(pluginFrame);
    if (resIdx == pluginFrameToVectImageCacheIdx.end()) {
        auto args = RenderArguments();
        args.time = frame;
        args.renderScale.x = 1;
        args.renderScale.y = 1;
        OfxRectD rect = plugin->dstClip->getRegionOfDefinition(frame);
        args.renderWindow.x1 = rect.x1;
        args.renderWindow.x2 = rect.x2;
        args.renderWindow.y1 = rect.y1;
        args.renderWindow.y2 = rect.y2;
        PatchMatcher(plugin, args).render();
        resIdx = pluginFrameToVectImageCacheIdx.find(pluginFrame);
    }
    return vectImageCache[resIdx->second].get();
}


void cacheVectImage(PatchMatchPlugin* plugin, double frame, SimpleImage* img) {
    pluginFramePair pluginFrame = {plugin, frame};
    auto res = vectImageCacheIdxToPluginFrame.find(nextVectImageCacheIdx);
    if (res != vectImageCacheIdxToPluginFrame.end()) {
        pluginFrameToVectImageCacheIdx.erase(res->second);
    }
    vectImageCache[nextVectImageCacheIdx].reset(img);
    pluginFrameToVectImageCacheIdx[pluginFrame] = nextVectImageCacheIdx;
    vectImageCacheIdxToPluginFrame[nextVectImageCacheIdx] = pluginFrame;
    nextVectImageCacheIdx = (nextVectImageCacheIdx + 1) % CACHE_SIZE;
}


inline float sq(float x) {
    return x*x;
}


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
    // full initial vects
    _srcInit.reset(
        _plugin->initClip->fetchImage(
            args.time, _plugin->initClip->getRegionOfDefinition(args.time)
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
    }

    _numLevels = _plugin->calculateNumLevelsAtTime(args.time);
    _endLevel = std::max(
        1, std::min(_numLevels, _plugin->endLevel->getValueAtTime(args.time))
    );
    auto iterateTemporally = _plugin->iterateTemporally->getValueAtTime(args.time);
    auto temporalIterRefFrame = _plugin->temporalIterationReferenceFrame->getValueAtTime(args.time);
    _initFromFrame = iterateTemporally && temporalIterRefFrame != args.time;
    if (_initFromFrame) {
        if (temporalIterRefFrame < args.time) {
            _initFrame = args.time - 1;
        } else {
            _initFrame = args.time + 1;
        }
    }
    if (_srcInit.get() || _initFromFrame) {
        _startLevel = _endLevel;
    }
    else {
        _startLevel = std::max(
            1, std::min(_endLevel, _plugin->startLevel->getValueAtTime(args.time))
        );
    }
    _iterations = _plugin->iterations->getValueAtTime(args.time);
    _acceptableScore = _plugin->acceptableScore->getValueAtTime(args.time);
    _spatialImpairmentFactor = _plugin->spatialImpairmentFactor->getValueAtTime(args.time);
    auto bA = _srcA->getRegionOfDefinition();
    auto bB = _srcB->getRegionOfDefinition();
    _maxDist = sqrt(sq(boundsWidth(bA)) + sq(boundsHeight(bB)));
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

    auto imgVect = _imgVect.release();
    cacheVectImage(_plugin, _renderArgs.time, imgVect);

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

    std::cout << "sct:" << t_score << std::endl;
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
    float* copyPix = NULL;
    int copyComps = 0;
    if (_initFromFrame) {
        auto tempImg = getCachedVectImage(_plugin, _initFrame);
        copyPix = tempImg->data;
        copyComps = tempImg->components;
    } else if (_srcInit.get()) {
        copyPix = (float*)_srcInit->getPixelData();
        copyComps = _srcInit->getPixelComponentCount();
    }
    for (int y=0, pY=0; y < img->height; y++) {
        if (_plugin->abort()) {return;}
        auto prevCell = prevRow;
        for (int x=0, pX=0; x < img->width; x++) {
            if (copyPix) {
                dataPix[0] = copyPix[0];
                dataPix[1] = copyPix[1];
            } else {
                dataPix[0] = rand() % _imgSrc->width - x;
                dataPix[1] = rand() % _imgSrc->height - y;
            }
            dataPix[2] = -1;
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
            if (copyPix) {
                copyPix += copyComps;
            }
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

            // current value
            auto cur = _imgVect->pix(x, y);
            if (cur[2] >= 0 && cur[2] <= _acceptableScore) {continue;}
            allAcceptable = false;

            // ideal circle
            float idealRadSq = -1, idealRad, idealX, idealY;
            auto havePrevX = dir > 0 && x > 0 || dir < 0 && x < _imgVect->width - 1;
            auto havePrevY = dir > 0 && y > 0 || dir < 0 && y < _imgVect->height - 1;
            float prevXX, prevXY, prevYX, prevYY;
            OfxPointI prevV;
            bool prevSame = true;
            if (havePrevX) {
                prevV = _imgVect->vect(x - dir, y);
                prevSame = prevV.x == cur[0] && prevV.y == cur[1];
                prevXX = x + prevV.x;
                prevXY = y + prevV.y;
            }
            if (havePrevY) {
                prevV = _imgVect->vect(x, y - dir);
                prevSame = prevSame && prevV.x == cur[0] && prevV.y == cur[1];
                prevYX = x + prevV.x;
                prevYY = y + prevV.y;
            }
            if (havePrevX && havePrevY) {
                auto radVX = (prevXX - prevYX) / 2;
                auto radVY = (prevXY - prevYY) / 2;
                idealRadSq = sq(radVX) + sq(radVY);
                idealRad = sqrt(idealRadSq);
                idealX = prevYX + radVX;
                idealY = prevYY + radVY;
            }

            if (!prevSame) {
                // propagate
                if (havePrevX) {
                    score(prevXX, prevXY, x, y, cur
                        ,idealRadSq, idealRad, idealX, idealY);
                }
                if (havePrevY) {
                    score(prevYX, prevYY, x, y, cur
                        ,idealRadSq, idealRad, idealX, idealY);
                }
            }

            // search
            double radW = _imgSrc->width / 2.0;
            double radH = _imgSrc->height / 2.0;
            int srchCentX = x + cur[0];
            int srchCentY = y + cur[1];
            for (; radW >= 1 && radH >= 1; radW /= 2, radH /= 2) {
                if (_plugin->abort()) {return allAcceptable;}
                int radWi = ceil(radW);
                int radHi = ceil(radH);
                auto l = std::max(0, srchCentX - radWi);
                auto b = std::max(0, srchCentY - radHi);
                auto w = std::min(_imgSrc->width, srchCentX + radWi + 1) - l;
                auto h = std::min(_imgSrc->height, srchCentY + radHi + 1) - b;
                auto sX = rand() % w + l;
                auto sY = rand() % h + b;
                score(
                    sX, sY, x, y, cur
                        ,idealRadSq, idealRad, idealX, idealY
                );
            }
        }
    }
    return allAcceptable;
}

void PatchMatcher::score(int xSrc, int ySrc, int xTrg, int yTrg, float* best
                        ,float idealRadSq, float idealRad, float idealX, float idealY)
{
    time(&st_score);
    if (!_imgSrc->valid(xSrc, ySrc)) {
        t_score += difftime(time(NULL), st_score);
        return;
    }
    auto components = std::min(_imgSrc->components, _imgTrg->components);
    auto extCompsSrc = _imgSrc->components - components;
    auto extCompsTrg = _imgTrg->components - components;
    float total = 0;
    float impairment = 0;
    int count = 0;
    auto bestTotal = best[2];
    auto haveBest = bestTotal >= 0;
    if (!haveBest) {
        bestTotal = MAXFLOAT;
    } else if (idealRadSq >= 0 && _spatialImpairmentFactor) {
        auto distSq = sq(xSrc - idealX) + sq(ySrc - idealY);
        if (distSq > idealRadSq) {
            impairment = _spatialImpairmentFactor * (sqrt(distSq) - idealRad) / _maxDist;
        }
        auto bestDistSq = sq(xTrg + best[0] - idealX) + sq(yTrg + best[1] - idealY);
        if (bestDistSq > idealRadSq) {
            bestTotal += _spatialImpairmentFactor * (sqrt(bestDistSq) - idealRad) / _maxDist;
        }
    }
    auto logIt = _level == _endLevel && _logCoords.x == xTrg && _logCoords.y == yTrg;
    if (logIt) {
        std::cout << "t:" << xTrg << "," << yTrg
            << " s:" << xSrc << "," << ySrc
            << " t:" << total
            << " bt:" << bestTotal;
        if (haveBest) {
            std::cout << " b:"
                << best[2] << " ["
                << (xTrg + best[0]) << " (" << best[0] << "),"
                << (yTrg + best[1]) << " (" << best[1] << ")"
                << "]";
        }
        std::cout << std::endl;
    }

    initScan(xSrc, ySrc, xTrg, yTrg);
    for (int y=0; y < _scan.numRows; y++) {
        if (_plugin->abort()) {return;}
        for (int x=0; x < _scan.numCols; x++) {
            for (int c=0; c < components; c++, _scan.pixSrc++, _scan.pixTrg++) {
                auto diff = *(_scan.pixTrg) - *(_scan.pixSrc);
                if (diff < 0) {total -= diff;}
                else {total += diff;}
                if (total + impairment >= bestTotal) {
                    if (logIt) {std::cout << "lose " << total << std::endl;}
                    t_score += difftime(time(NULL), st_score);
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
    if (total + impairment >= bestTotal) {
        if (logIt) {std::cout << "lose " << total << std::endl;}
        t_score += difftime(time(NULL), st_score);
        return;
    }
    best[0] = xSrc - xTrg;
    best[1] = ySrc - yTrg;
    best[2] = total;
    if (logIt) {
        std::cout << "win! " << best[2]
            << " " << best[0] << "," << best[1]
            << std::endl;
    }
    t_score += difftime(time(NULL), st_score);
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
