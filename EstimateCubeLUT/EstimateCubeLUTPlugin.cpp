#include "EstimateCubeLUTPlugin.h"
#include "ofxsCoords.h"
#include <iostream>
#include <fstream>
#include <regex>


EstimateCubeLUTPlugin::EstimateCubeLUTPlugin(OfxImageEffectHandle handle)
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
    _file = fetchStringParam(kParamFile);
    _writeCubeSize = fetchIntParam(kParamWriteCubeSize);
    _estimate = fetchPushButtonParam(kParamEstimate);
}

bool EstimateCubeLUTPlugin::isIdentity(const IsIdentityArguments &args, 
                                  Clip * &identityClip, double &/*identityTime*/
#ifdef OFX_EXTENSIONS_NUKE
    , int& view
    , std::string& plane
#endif
)
{
    if (!_file->getValue().empty()) {
        return false;
    }
    identityClip = _srcClip;
    return true;
}

inline bool insideBounds(const Eigen::Vector3f& p, const std::pair<Eigen::Vector3f, Eigen::Vector3f>& bounds) {
    for (int d = 0; d < 3; d++) {
        if (p(d) < bounds.first(d)) {return false;}
        if (p(d) > bounds.second(d)) {return false;}
    }
    return true;
}

inline Eigen::Vector4f computeBarycentricCoords(const Eigen::Vector3f& p, Eigen::Matrix4f& A) {
    Eigen::Vector4f b;
    b << p(0), p(1), p(2), 1;

    Eigen::Vector4f x = A.fullPivHouseholderQr().solve(b);
    return x;
}

inline Eigen::Vector3f barycentricToPoint(const Eigen::Vector4f& bary, const std::array<Eigen::Vector3f, 4>& tetra) {
    Eigen::Vector3f result(0, 0, 0);
    for (int t = 0; t < 4; t++) {
        result += bary(t) * tetra[t];
    }
    return result;
}

inline void interpolateFrom(
    OfxRGBColourF* srcRGB,
    std::set<int>* fromTetraGrid,
    std::vector<std::pair<Eigen::Vector3f, Eigen::Vector3f>>* fromTetraBounds,
    std::vector<Eigen::Matrix4f>* fromTetraMatrices,
    std::vector<std::array<Eigen::Vector3f, 4>>* toTetras,
    OfxRGBColourF* dstRGB
) {
    Eigen::Vector3f p, dstV;
    Eigen::Vector4f bary;

    p.x() = srcRGB->r;
    p.y() = srcRGB->g;
    p.z() = srcRGB->b;
    int gridIndices[3];
    for (int d=0; d < 3; d++) {
        gridIndices[d] = (std::min(float(1), std::max(float(0), p(d))) * (TETRA_GRID_SIZE - 1));
    }
    auto tetraIndices = fromTetraGrid[gridIndices[0] * TETRA_GRID_SIZE*TETRA_GRID_SIZE + gridIndices[1] * TETRA_GRID_SIZE + gridIndices[2]];
    for (auto i : tetraIndices) {
        if (!insideBounds(p, fromTetraBounds->at(i))) {
            continue;
        }
        bary = computeBarycentricCoords(p, fromTetraMatrices->at(i));
        dstV = barycentricToPoint(bary, toTetras->at(i));
        dstRGB->r = dstV.x();
        dstRGB->g = dstV.y();
        dstRGB->b = dstV.z();
        break;
    }
}

void EstimateCubeLUTPlugin::render(const RenderArguments &args)
{
    std::unique_ptr<Image> srcImg(_srcClip->fetchImage(args.time));
    std::unique_ptr<Image> dstImg(_dstClip->fetchImage(args.time));
    auto components = srcImg->getPixelComponentCount();

    for (auto y=args.renderWindow.y1; y < args.renderWindow.y2; y++) {
        for (auto x=args.renderWindow.x1; x < args.renderWindow.x2; x++) {
            auto srcPIX = (float*)srcImg->getPixelAddress(x, y);
            if (!srcPIX) {continue;}
            auto dstPIX = (float*)dstImg->getPixelAddress(x, y);
            if (!dstPIX) {continue;}
            OfxRGBColourF srcRGB;
            for (auto c=0; c < components; c++, srcPIX++) {
                if (c >= 3) {
                    dstPIX[c] = *srcPIX;
                    continue;
                }
                if (c == 0) {srcRGB.r = *srcPIX;}
                else if (c == 1) {srcRGB.g = *srcPIX;}
                else if (c == 2) {srcRGB.b = *srcPIX;}
            }
            OfxRGBColourF dstRGB{0, 0, 0};
            auto iter = _cubeLUT.find(srcRGB);
            if (iter != _cubeLUT.end()) {
                dstRGB.r = iter->second.r;
                dstRGB.g = iter->second.g;
                dstRGB.b = iter->second.b;
            } else if (_cubeLUT.size()) {
                interpolateFrom(&srcRGB, _cubeLUTFromTetraGrid, &_cubeLUTFromTetraBounds, &_cubeLUTFromTetraMatrices, &_cubeLUTToTetras, &dstRGB);
            }
            for (auto c=0; c < std::min(3, components); c++, dstPIX++) {
                if (c == 0) {*dstPIX = dstRGB.r;}
                else if (c == 1) {*dstPIX = dstRGB.g;}
                else if (c == 2) {*dstPIX = dstRGB.b;}
            }            
        }
    }
}

bool operator<(const OfxRGBColourF lhs, const OfxRGBColourF rhs) {
    if (lhs.r != rhs.r) {return lhs.r < rhs.r;}
    if (lhs.g != rhs.g) {return lhs.g < rhs.g;}
    return lhs.b < rhs.b;
}

void EstimateCubeLUTPlugin::changedParam(const InstanceChangedArgs &args, const std::string &paramName) {
    if (paramName == kParamFile) {
        readFile();
    } else if (paramName == kParamEstimate) {
        estimate(args.time);
    }
}

inline void cacheTetraSearch(
    tetgenio* tetgenOut,
    float* toPoints,
    std::vector<std::pair<Eigen::Vector3f, Eigen::Vector3f>>* fromTetraBounds,
    std::set<int>* fromTetraGrid,
    std::vector<Eigen::Matrix4f>* fromTetraMatrices,
    std::vector<std::array<Eigen::Vector3f, 4>>* toTetras
) {
    fromTetraBounds->resize(tetgenOut->numberoftetrahedra);
    fromTetraMatrices->resize(tetgenOut->numberoftetrahedra);
    toTetras->resize(tetgenOut->numberoftetrahedra);
    auto tetraPtr = tetgenOut->tetrahedronlist;
    for (int i=0; i < tetgenOut->numberoftetrahedra; i++) {
        auto tetraBounds = &(fromTetraBounds->at(i));
        int minGridIdx[3], maxGridIdx[3];
        for (int t=0; t < 4; t++, tetraPtr++) {
            auto tIdx = *tetraPtr * 3;
            auto fromPointsPtr = &(tetgenOut->pointlist[tIdx]);
            auto toPointsPtr = &(toPoints[tIdx]);
            for (int d=0; d < 3; d++, fromPointsPtr++, toPointsPtr++) {
                int gridIdx = (std::min(double(1), std::max(double(0), *fromPointsPtr)) * (TETRA_GRID_SIZE - 1));
                if (!t) {
                    tetraBounds->first(d) = *fromPointsPtr;
                    tetraBounds->second(d) = *fromPointsPtr;
                    minGridIdx[d] = gridIdx;
                    maxGridIdx[d] = gridIdx;
                } else {
                    tetraBounds->first(d) = std::min((float)*fromPointsPtr, tetraBounds->first(d));
                    tetraBounds->second(d) = std::max((float)*fromPointsPtr, tetraBounds->second(d));
                    minGridIdx[d] = std::min(gridIdx, minGridIdx[d]);
                    maxGridIdx[d] = std::max(gridIdx, maxGridIdx[d]);
                }
                fromTetraMatrices->at(i)(d, t) = *fromPointsPtr;
                toTetras->at(i)[t](d) = *toPointsPtr;
            }
            fromTetraMatrices->at(i).row(3) << 1, 1, 1, 1;
        }
        for (int x=minGridIdx[0]; x <= maxGridIdx[0]; x++) {
            for (int y=minGridIdx[1]; y <= maxGridIdx[1]; y++) {
                for (int z=minGridIdx[2]; z <= maxGridIdx[2]; z++) {
                    fromTetraGrid[x*TETRA_GRID_SIZE*TETRA_GRID_SIZE + y*TETRA_GRID_SIZE + z].insert(i);
                }
            }
        }
    }
}

void EstimateCubeLUTPlugin::readFile() {
    _cubeSize = 0;
    _cubeLUT.clear();
    _cubeLUTFromTetraBounds.clear();
    _cubeLUTFromTetraMatrices.clear();
    _cubeLUTToTetras.clear();

    auto filePath = _file->getValue();
    if (filePath.empty()) {
        return;
    }
    std::ifstream cubeStream(filePath);
    if (!cubeStream.good()) {
        return;
    }
    std::string line;

    std::smatch matchResults;

    std::regex lutSizePattern("LUT_3D_SIZE\\s+(\\d+)");
    while (!cubeStream.eof()) {
        std::getline(cubeStream, line);
        if (!std::regex_match(line, matchResults, lutSizePattern)) {
            continue;
        }
        _cubeSize = stoi(matchResults[1]);
        break;
    }
    
    float maxIndex = _cubeSize - 1;
    auto cubeSizeSq = _cubeSize * _cubeSize;
    auto cubeSizeCb = cubeSizeSq * _cubeSize;
    int i = 0;
    std::regex mapToPattern("([\\d\\.\\+-]+)\\s+([\\d\\.\\+-]+)\\s+([\\d\\.\\+-]+)");
    OfxRGBColourF fromRGB, toRGB;
    tetgenio tetgenIn;
    tetgenIn.numberofpoints = cubeSizeCb;
    tetgenIn.pointlist = new double[cubeSizeCb * 3];
    std::unique_ptr<float> toPoints(new float[cubeSizeCb * 3]);
    auto fromPointsPtr = tetgenIn.pointlist;
    auto toPointsPtr = toPoints.get();
    while (!cubeStream.eof()) {
        std::getline(cubeStream, line);
        if (!std::regex_match(line, matchResults, mapToPattern)) {
            continue;
        }
        fromRGB.r = (i % _cubeSize) / maxIndex;
        fromRGB.g = ((i / _cubeSize) % _cubeSize) / maxIndex;
        fromRGB.b = (i / cubeSizeSq) / maxIndex;
        toRGB.r = std::stod(matchResults[1]);
        toRGB.g = std::stod(matchResults[2]);
        toRGB.b = std::stod(matchResults[3]);
        _cubeLUT[fromRGB] = toRGB;

        *(fromPointsPtr++) = fromRGB.r;
        *(fromPointsPtr++) = fromRGB.g;
        *(fromPointsPtr++) = fromRGB.b;
        *(toPointsPtr++) = toRGB.r;
        *(toPointsPtr++) = toRGB.g;
        *(toPointsPtr++) = toRGB.b;
        i++;
    }

    tetgenio tetgenOut;
    tetgenbehavior tetgenBehaviour;
    tetrahedralize(&tetgenBehaviour, &tetgenIn, &tetgenOut);

    cacheTetraSearch(
        &tetgenOut, toPoints.get(),
        &_cubeLUTFromTetraBounds, _cubeLUTFromTetraGrid, &_cubeLUTFromTetraMatrices, &_cubeLUTToTetras
    );

    // std::ofstream objFile("/home/jonathan/deleteme.obj");
    // objFile << "o Test" << std::endl;
    // toPointsPtr = _cubeLUTToPoints.get();
    // for (int i=0; i < cubeSizeCb; i++) {
    //     objFile << "v " << *(toPointsPtr++) << " " << *(toPointsPtr++) << " " << *(toPointsPtr++) << std::endl;
    // }
    // auto triFacePtr = _cubeLUTTetgenOut->trifacelist;
    // for (int i=0; i < _cubeLUTTetgenOut->numberoftrifaces; i++) {
    //     objFile << "f " << (*(triFacePtr++) + 1) << " " << (*(triFacePtr++) + 1) << " " << (*(triFacePtr++) + 1) << std::endl;
    // }
    // objFile.close();
}

void EstimateCubeLUTPlugin::estimate(double time) {
    auto filePath = _file->getValue();
    if (filePath.empty()) {
        return;
    }
    std::ofstream cubeStream(filePath);
    if (!cubeStream.good()) {
        return;
    }

    auto writeCubeSize = _writeCubeSize->getValue();
    cubeStream << "LUT_3D_SIZE " << writeCubeSize << std::endl;

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

    std::map<OfxRGBColourF, std::pair<int, OfxRGBColourF>> fromToCountSum;
    for (auto y=isect.y1; y < isect.y2; y++) {        
        for (auto x=isect.x1; x < isect.x2; x++) {
            auto srcPix = (float*)srcImg->getPixelAddress(x, y);
            if (!srcPix) {continue;}
            auto trgPix = (float*)trgImg->getPixelAddress(round(x / horizScale), y);
            if (!trgPix) {continue;}

            OfxRGBColourF srcRGB, trgRGB;
            for (int d=0; d < 3; d++, srcPix++, trgPix++) {
                float srcVal, trgVal;
                if (d < components) {
                    srcVal = *srcPix;
                    trgVal = *trgPix;
                } else {
                    srcVal = 0;
                    trgVal = 0;
                }
                if (d == 0) {
                    srcRGB.r = srcVal;
                    trgRGB.r = trgVal;
                } else if (d == 1) {
                    srcRGB.g = srcVal;
                    trgRGB.g = trgVal;
                } else if (d == 2) {
                    srcRGB.b = srcVal;
                    trgRGB.b = trgVal;
                }
            }
            if (!fromToCountSum.count(srcRGB)) {
                fromToCountSum[srcRGB] = {1, trgRGB};
            } else {
                auto pair = &fromToCountSum[srcRGB];
                pair->first++;
                pair->second.r += trgRGB.r;
                pair->second.g += trgRGB.g;
                pair->second.b += trgRGB.b;
            }
        }
    }

    OfxRGBColourF corner;
    for (corner.r = 0; corner.r <= 1; corner.r++) {
        for (corner.g = 0; corner.g <= 1; corner.g++) {
            for (corner.b = 0; corner.b <= 1; corner.b++) {
                if (fromToCountSum.count(corner)) {
                    continue;
                }
                auto pair = &fromToCountSum[corner];
                pair->first = 1;
                pair->second = corner;
            }
        }
    }

    tetgenio tetgenIn;
    tetgenIn.numberofpoints = fromToCountSum.size();
    tetgenIn.pointlist = new double[tetgenIn.numberofpoints * 3];
    std::unique_ptr<float> toPoints(new float[tetgenIn.numberofpoints * 3]);

    auto fromPointsPtr = tetgenIn.pointlist;
    auto toPointsPtr = toPoints.get();

    for (auto mapping : fromToCountSum) {
        *(fromPointsPtr++) = mapping.first.r;
        *(fromPointsPtr++) = mapping.first.g;
        *(fromPointsPtr++) = mapping.first.b;
        *(toPointsPtr++) = mapping.second.second.r / mapping.second.first;
        *(toPointsPtr++) = mapping.second.second.g / mapping.second.first;
        *(toPointsPtr++) = mapping.second.second.b / mapping.second.first;
    }

    progressUpdate(0.3);

    tetgenio tetgenOut;
    tetgenbehavior tetgenBehaviour;
    tetrahedralize(&tetgenBehaviour, &tetgenIn, &tetgenOut);

    // std::ofstream objFileFrom("/home/jonathan/deleteme-from.obj");
    // objFileFrom << "o From" << std::endl;
    // fromPointsPtr = tetgenOut.pointlist;
    // for (int i=0; i < tetgenOut.numberofpoints; i++) {
    //     objFileFrom << "v " << *(fromPointsPtr++) << " " << *(fromPointsPtr++) << " " << *(fromPointsPtr++) << std::endl;
    // }
    // auto triFacePtr = tetgenOut.trifacelist;
    // for (int i=0; i < tetgenOut.numberoftrifaces; i++) {
    //     objFileFrom << "f " << (*(triFacePtr++) + 1) << " " << (*(triFacePtr++) + 1) << " " << (*(triFacePtr++) + 1) << std::endl;
    // }
    // objFileFrom.close();

    // std::ofstream objFileTo("/home/jonathan/deleteme-to.obj");
    // objFileTo << "o To" << std::endl;
    // toPointsPtr = toPoints.get();
    // for (int i=0; i < tetgenOut.numberofpoints; i++) {
    //     objFileTo << "v " << *(toPointsPtr++) << " " << *(toPointsPtr++) << " " << *(toPointsPtr++) << std::endl;
    // }
    // triFacePtr = tetgenOut.trifacelist;
    // for (int i=0; i < tetgenOut.numberoftrifaces; i++) {
    //     objFileTo << "f " << (*(triFacePtr++) + 1) << " " << (*(triFacePtr++) + 1) << " " << (*(triFacePtr++) + 1) << std::endl;
    // }
    // objFileTo.close();

    progressUpdate(0.6);

    std::set<int> fromTetraGrid[TETRA_GRID_SIZE * TETRA_GRID_SIZE * TETRA_GRID_SIZE];
    std::vector<std::pair<Eigen::Vector3f, Eigen::Vector3f>> fromTetraBounds;
    std::vector<Eigen::Matrix4f> fromTetraMatrices;
    std::vector<std::array<Eigen::Vector3f, 4>> toTetras;

    cacheTetraSearch(
        &tetgenOut, toPoints.get(),
        &fromTetraBounds, fromTetraGrid, &fromTetraMatrices, &toTetras
    );

    progressUpdate(0.7);

    OfxRGBColourF rowRGB, dstRGB;
    auto maxIndex = writeCubeSize - 1;
    for (float b = 0; b < writeCubeSize; b++) {
        for (float g = 0; g < writeCubeSize; g++) {
            for (float r = 0; r < writeCubeSize; r++) {
                rowRGB.r = r / maxIndex;
                rowRGB.g = g / maxIndex;
                rowRGB.b = b / maxIndex;
                interpolateFrom(
                    &rowRGB, fromTetraGrid, &fromTetraBounds, &fromTetraMatrices, &toTetras, &dstRGB
                );
                cubeStream << dstRGB.r << " " << dstRGB.g << " " << dstRGB.b << std::endl;
            }
        }
    }
    cubeStream.close();

    progressUpdate(0.8);

    readFile();

    progressUpdate(1);

    progressEnd();
}
