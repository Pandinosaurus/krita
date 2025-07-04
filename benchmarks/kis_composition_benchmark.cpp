/*
 *  SPDX-FileCopyrightText: 2012 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2015 Thorsten Zachmann <zachmann@kde.org>
 *  SPDX-FileCopyrightText: 2020 Mathias Wein <lynx.mw+kde@gmail.com>
 *  SPDX-FileCopyrightText: 2022 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// for calculation of the needed alignment
#include <xsimd_extensions/xsimd.hpp>
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
#include <KoOptimizedCompositeOpOver32.h>
#include <KoOptimizedCompositeOpOver128.h>
#include <KoOptimizedCompositeOpCopy128.h>
#include <KoOptimizedCompositeOpAlphaDarken32.h>
#endif

#include "kis_composition_benchmark.h"
#include <simpletest.h>
#include <QElapsedTimer>

#include <KoColorSpace.h>
#include <KoCompositeOp.h>
#include <KoColorSpaceRegistry.h>

#include <KoColorSpaceTraits.h>
#include <KoCompositeOpAlphaDarken.h>
#include <KoCompositeOpOver.h>
#include <KoCompositeOpCopy2.h>
#include <KoOptimizedCompositeOpFactory.h>
#include <KoAlphaDarkenParamsWrapper.h>

// for posix_memalign()
#include <stdlib.h>

#include <kis_debug.h>

#if defined Q_OS_WIN
#define MEMALIGN_ALLOC(p, a, s) ((*(p)) = _aligned_malloc((s), (a)), *(p) ? 0 : errno)
#define MEMALIGN_FREE(p) _aligned_free((p))
#else
#define MEMALIGN_ALLOC(p, a, s) posix_memalign((p), (a), (s))
#define MEMALIGN_FREE(p) free((p))
#endif

#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
using float_v = xsimd::batch<float, xsimd::current_arch>;
#endif

enum AlphaRange {
    ALPHA_ZERO,
    ALPHA_UNIT,
    ALPHA_RANDOM
};


template <typename channel_type, class RandomGenerator>
inline channel_type generateAlphaValue(AlphaRange range, RandomGenerator &rnd) {
    channel_type value = 0;

    switch (range) {
    case ALPHA_ZERO:
        break;
    case ALPHA_UNIT:
        value = rnd.unit();
        break;
    case ALPHA_RANDOM:
        value = rnd();
        break;
    }

    return value;
}

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_smallint.hpp>
#include <boost/random/uniform_real.hpp>

template <typename channel_type>
struct RandomGenerator {
    channel_type operator() () {
        qFatal("Wrong template instantiation");
        return channel_type(0);
    }

    channel_type unit() {
        qFatal("Wrong template instantiation");
        return channel_type(0);
    }
};

template <>
struct RandomGenerator<quint8>
{
    RandomGenerator(int seed)
        : m_smallint(0,255),
          m_rnd(seed)
    {
    }

    quint8 operator() () {
        return m_smallint(m_rnd);
    }

    quint8 unit() {
        return KoColorSpaceMathsTraits<quint8>::unitValue;
    }

    boost::uniform_smallint<int> m_smallint;
    boost::mt11213b m_rnd;
};

template <>
struct RandomGenerator<quint16>
{
    RandomGenerator(int seed)
        : m_smallint(0,65535),
          m_rnd(seed)
    {
    }

    quint16 operator() () {
        return m_smallint(m_rnd);
    }

    quint16 unit() {
        return KoColorSpaceMathsTraits<quint16>::unitValue;
    }

    boost::uniform_smallint<int> m_smallint;
    boost::mt11213b m_rnd;
};

template <>
struct RandomGenerator<float>
{
    RandomGenerator(int seed)
        : m_rnd(seed)
    {
    }

    float operator() () {
        //return float(m_rnd()) / float(m_rnd.max());
        return m_smallfloat(m_rnd);
    }

    float unit() {
        return KoColorSpaceMathsTraits<float>::unitValue;
    }

    boost::uniform_real<float> m_smallfloat;
    boost::mt11213b m_rnd;
};

template <>
struct RandomGenerator<double> : RandomGenerator<float>
{
    RandomGenerator(int seed)
        : RandomGenerator<float>(seed)
    {
    }
};


template <typename channel_type>
void generateDataLine(uint seed, int numPixels, quint8 *srcPixels, quint8 *dstPixels, quint8 *mask, AlphaRange srcAlphaRange, AlphaRange dstAlphaRange)
{
    Q_ASSERT(numPixels >= 4);

    RandomGenerator<channel_type> rnd(seed);
    RandomGenerator<quint8> maskRnd(seed + 1);

    channel_type *srcArray = reinterpret_cast<channel_type*>(srcPixels);
    channel_type *dstArray = reinterpret_cast<channel_type*>(dstPixels);

    for (int i = 0; i < numPixels; i++) {
        for (int j = 0; j < 3; j++) {
            channel_type s = rnd();
            channel_type d = rnd();
            *(srcArray++) = s;
            *(dstArray++) = d;
        }

        channel_type sa = generateAlphaValue<channel_type>(srcAlphaRange, rnd);
        channel_type da = generateAlphaValue<channel_type>(dstAlphaRange, rnd);
        *(srcArray++) = sa;
        *(dstArray++) = da;

        *(mask++) = maskRnd();
    }
}

void printData(int numPixels, quint8 *srcPixels, quint8 *dstPixels, quint8 *mask)
{
    for (int i = 0; i < numPixels; i++) {
        qDebug() << "Src: "
                 << srcPixels[i*4] << "\t"
                 << srcPixels[i*4+1] << "\t"
                 << srcPixels[i*4+2] << "\t"
                 << srcPixels[i*4+3] << "\t"
                 << "Msk:" << mask[i];

        qDebug() << "Dst: "
                 << dstPixels[i*4] << "\t"
                 << dstPixels[i*4+1] << "\t"
                 << dstPixels[i*4+2] << "\t"
                 << dstPixels[i*4+3];
    }
}

const int rowStride = 64;
const int totalRows = 64;
const QRect processRect(0,0,64,64);
const int numPixels = rowStride * totalRows;
const int numTiles = 1024;


struct Tile {
    quint8 *src;
    quint8 *dst;
    quint8 *mask;
};
#include <stdint.h>
QVector<Tile> generateTiles(int size,
                            const int srcAlignmentShift,
                            const int dstAlignmentShift,
                            AlphaRange srcAlphaRange,
                            AlphaRange dstAlphaRange,
                            const quint32 pixelSize)
{
    QVector<Tile> tiles(size);

#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    const int vecSize = float_v::size;
#else
    const int vecSize = 1;
#endif

    // the 256 are used to make sure that we have a good alignment no matter what build options are used.
    const size_t pixelAlignment = qMax(size_t(vecSize * sizeof(float)), size_t(256));
    const size_t maskAlignment = qMax(size_t(vecSize), size_t(256));
    for (int i = 0; i < size; i++) {
        void *ptr = 0;
        int error = MEMALIGN_ALLOC(&ptr, pixelAlignment, numPixels * pixelSize + srcAlignmentShift);
        if (error) {
            qFatal("posix_memalign failed: %d", error);
        }
        tiles[i].src = (quint8*)ptr + srcAlignmentShift;
        error = MEMALIGN_ALLOC(&ptr, pixelAlignment, numPixels * pixelSize + dstAlignmentShift);
        if (error) {
            qFatal("posix_memalign failed: %d", error);
        }
        tiles[i].dst = (quint8*)ptr + dstAlignmentShift;
        error = MEMALIGN_ALLOC(&ptr, maskAlignment, numPixels);
        if (error) {
            qFatal("posix_memalign failed: %d", error);
        }
        tiles[i].mask = (quint8*)ptr;

        if (pixelSize == 4) {
            generateDataLine<quint8>(1, numPixels, tiles[i].src, tiles[i].dst, tiles[i].mask, srcAlphaRange, dstAlphaRange);
        } else if (pixelSize == 8) {
            generateDataLine<quint16>(1, numPixels, tiles[i].src, tiles[i].dst, tiles[i].mask, srcAlphaRange, dstAlphaRange);
        } else if (pixelSize == 16) {
            generateDataLine<float>(1, numPixels, tiles[i].src, tiles[i].dst, tiles[i].mask, srcAlphaRange, dstAlphaRange);
        } else {
            qFatal("Pixel size %i is not implemented", pixelSize);
        }
    }

    return tiles;
}

void freeTiles(QVector<Tile> tiles,
               const int srcAlignmentShift,
               const int dstAlignmentShift)
{
    Q_FOREACH (const Tile &tile, tiles) {
        MEMALIGN_FREE(tile.src - srcAlignmentShift);
        MEMALIGN_FREE(tile.dst - dstAlignmentShift);
        MEMALIGN_FREE(tile.mask);
    }
}

template <typename channel_type>
inline bool fuzzyCompare(channel_type a, channel_type b, channel_type prec) {
    return qAbs(a - b) <= prec;
}

template<typename channel_type>
struct PixelEqualDirect
{
    bool operator() (channel_type c1, channel_type a1,
                     channel_type c2, channel_type a2,
                     channel_type prec) {

        Q_UNUSED(a1);
        Q_UNUSED(a2);

        return fuzzyCompare(c1, c2, prec);
    }
};

template<typename channel_type>
struct PixelEqualPremultiplied
{
    bool operator() (channel_type c1, channel_type a1,
                     channel_type c2, channel_type a2,
                     channel_type prec) {

        c1 = KoColorSpaceMaths<channel_type>::multiply(c1, a1);
        c2 = KoColorSpaceMaths<channel_type>::multiply(c2, a2);

        return fuzzyCompare(c1, c2, prec);
    }
};

template <typename channel_type, template<typename> class Compare = PixelEqualDirect>
inline bool comparePixels(channel_type *p1, channel_type *p2, channel_type prec) {
    Compare<channel_type> comp;

    return (p1[3] == p2[3] && p1[3] == 0) ||
        (comp(p1[0], p1[3], p2[0], p2[3], prec) &&
         comp(p1[1], p1[3], p2[1], p2[3], prec) &&
         comp(p1[2], p1[3], p2[2], p2[3], prec) &&
         fuzzyCompare(p1[3], p2[3], prec));
}

template <typename channel_type, template<typename> class Compare>
bool compareTwoOpsPixels(QVector<Tile> &tiles, channel_type prec) {
    channel_type *dst1 = reinterpret_cast<channel_type*>(tiles[0].dst);
    channel_type *dst2 = reinterpret_cast<channel_type*>(tiles[1].dst);

    channel_type *src1 = reinterpret_cast<channel_type*>(tiles[0].src);
    channel_type *src2 = reinterpret_cast<channel_type*>(tiles[1].src);

    for (int i = 0; i < numPixels; i++) {
        if (!comparePixels<channel_type, Compare>(dst1, dst2, prec)) {
            qDebug() << "Wrong result:" << i;
            qDebug() << "Act: " << dst1[0] << dst1[1] << dst1[2] << dst1[3];
            qDebug() << "Exp: " << dst2[0] << dst2[1] << dst2[2] << dst2[3];
            qDebug() << "Dif: " << dst1[0] - dst2[0] << dst1[1] - dst2[1] << dst1[2] - dst2[2] << dst1[3] - dst2[3];

            channel_type *s1 = src1 + 4 * i;
            channel_type *s2 = src2 + 4 * i;

            qDebug() << "SrcA:" << s1[0] << s1[1] << s1[2] << s1[3];
            qDebug() << "SrcE:" << s2[0] << s2[1] << s2[2] << s2[3];

            qDebug() << "MskA:" << tiles[0].mask[i];
            qDebug() << "MskE:" << tiles[1].mask[i];

            return false;
        }
        dst1 += 4;
        dst2 += 4;
    }
    return true;
}

template<template<typename> class Compare = PixelEqualDirect>
bool compareTwoOps(bool haveMask, const KoCompositeOp *op1, const KoCompositeOp *op2)
{
    Q_ASSERT(op1->colorSpace()->pixelSize() == op2->colorSpace()->pixelSize());
    const quint32 pixelSize = op1->colorSpace()->pixelSize();
    const int alignment = 16;
    QVector<Tile> tiles = generateTiles(2, alignment, alignment, ALPHA_RANDOM, ALPHA_RANDOM, op1->colorSpace()->pixelSize());

    KoCompositeOp::ParameterInfo params;
    params.dstRowStride  = 4 * rowStride;
    params.srcRowStride  = 4 * rowStride;
    params.maskRowStride = rowStride;
    params.rows          = processRect.height();
    params.cols          = processRect.width();
    // This is a hack as in the old version we get a rounding of opacity to this value
    params.opacity       = float(Arithmetic::scale<quint8>(0.5*1.0f))/255.0;
    params.flow          = 0.3*1.0f;
    params.channelFlags  = QBitArray();

    params.dstRowStart   = tiles[0].dst;
    params.srcRowStart   = tiles[0].src;
    params.maskRowStart  = haveMask ? tiles[0].mask : 0;
    op1->composite(params);

    params.dstRowStart   = tiles[1].dst;
    params.srcRowStart   = tiles[1].src;
    params.maskRowStart  = haveMask ? tiles[1].mask : 0;
    op2->composite(params);

    bool compareResult = true;
    if (pixelSize == 4) {
        compareResult = compareTwoOpsPixels<quint8, Compare>(tiles, 10);
    }
    else if (pixelSize == 8) {
        compareResult = compareTwoOpsPixels<quint16, Compare>(tiles, 90);
    }
    else if (pixelSize == 16) {
        compareResult = compareTwoOpsPixels<float, Compare>(tiles, 2e-6);
    }
    else {
        qFatal("Pixel size %i is not implemented", pixelSize);
    }

    freeTiles(tiles, alignment, alignment);

    return compareResult;
}

QString getTestName(bool haveMask,
                    const int srcAlignmentShift,
                    const int dstAlignmentShift,
                    AlphaRange srcAlphaRange,
                    AlphaRange dstAlphaRange)
{

    QString testName;
    testName +=
        !srcAlignmentShift && !dstAlignmentShift ? "Aligned    " :
        !srcAlignmentShift &&  dstAlignmentShift ? "SrcUnalign " :
         srcAlignmentShift && !dstAlignmentShift ? "DstUnalign " :
         srcAlignmentShift &&  dstAlignmentShift ? "Unaligned  " : "###";

    testName += haveMask ? "Mask   " : "NoMask ";

    testName +=
        srcAlphaRange == ALPHA_RANDOM ? "SrcRand " :
        srcAlphaRange == ALPHA_ZERO   ? "SrcZero " :
        srcAlphaRange == ALPHA_UNIT   ? "SrcUnit " : "###";

    testName +=
        dstAlphaRange == ALPHA_RANDOM ? "DstRand" :
        dstAlphaRange == ALPHA_ZERO   ? "DstZero" :
        dstAlphaRange == ALPHA_UNIT   ? "DstUnit" : "###";

    return testName;
}

void benchmarkCompositeOp(const KoCompositeOp *op,
                          bool haveMask,
                          qreal opacity,
                          qreal flow,
                          const int srcAlignmentShift,
                          const int dstAlignmentShift,
                          AlphaRange srcAlphaRange,
                          AlphaRange dstAlphaRange)
{
    QString testName = getTestName(haveMask, srcAlignmentShift, dstAlignmentShift, srcAlphaRange, dstAlphaRange);

    QVector<Tile> tiles =
        generateTiles(numTiles, srcAlignmentShift, dstAlignmentShift, srcAlphaRange, dstAlphaRange, op->colorSpace()->pixelSize());

    const int tileOffset = 4 * (processRect.y() * rowStride + processRect.x());

    KoCompositeOp::ParameterInfo params;
    params.dstRowStride  = 4 * rowStride;
    params.srcRowStride  = 4 * rowStride;
    params.maskRowStride = rowStride;
    params.rows          = processRect.height();
    params.cols          = processRect.width();
    params.opacity       = opacity;
    params.flow          = flow;
    params.channelFlags  = QBitArray();

    QElapsedTimer timer;
    timer.start();

    Q_FOREACH (const Tile &tile, tiles) {
        params.dstRowStart   = tile.dst + tileOffset;
        params.srcRowStart   = tile.src + tileOffset;
        params.maskRowStart  = haveMask ? tile.mask : 0;
        op->composite(params);
    }

    qDebug() << testName << "RESULT:" << timer.elapsed() << "msec";

    freeTiles(tiles, srcAlignmentShift, dstAlignmentShift);
}

void benchmarkCompositeOp(const KoCompositeOp *op, const QString &postfix)
{
    qDebug() << "Testing Composite Op:" << op->id() << "(" << postfix << ")";

    benchmarkCompositeOp(op, true, 0.5, 0.3, 0, 0, ALPHA_RANDOM, ALPHA_RANDOM);
    benchmarkCompositeOp(op, true, 0.5, 0.3, 8, 0, ALPHA_RANDOM, ALPHA_RANDOM);
    benchmarkCompositeOp(op, true, 0.5, 0.3, 0, 8, ALPHA_RANDOM, ALPHA_RANDOM);
    benchmarkCompositeOp(op, true, 0.5, 0.3, 4, 8, ALPHA_RANDOM, ALPHA_RANDOM);

/// --- Vary the content of the source and destination

    benchmarkCompositeOp(op, false, 1.0, 1.0, 0, 0, ALPHA_RANDOM, ALPHA_RANDOM);
    benchmarkCompositeOp(op, false, 1.0, 1.0, 0, 0, ALPHA_ZERO, ALPHA_RANDOM);
    benchmarkCompositeOp(op, false, 1.0, 1.0, 0, 0, ALPHA_UNIT, ALPHA_RANDOM);

/// ---

    benchmarkCompositeOp(op, false, 1.0, 1.0, 0, 0, ALPHA_RANDOM, ALPHA_ZERO);
    benchmarkCompositeOp(op, false, 1.0, 1.0, 0, 0, ALPHA_ZERO, ALPHA_ZERO);
    benchmarkCompositeOp(op, false, 1.0, 1.0, 0, 0, ALPHA_UNIT, ALPHA_ZERO);

/// ---

    benchmarkCompositeOp(op, false, 1.0, 1.0, 0, 0, ALPHA_RANDOM, ALPHA_UNIT);
    benchmarkCompositeOp(op, false, 1.0, 1.0, 0, 0, ALPHA_ZERO, ALPHA_UNIT);
    benchmarkCompositeOp(op, false, 1.0, 1.0, 0, 0, ALPHA_UNIT, ALPHA_UNIT);
}

#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS

template <typename channels_type>
void printError(quint8 *s, quint8 *d1, quint8 *d2, quint8 *msk1, int pos)
{
    const channels_type *src1 = reinterpret_cast<const channels_type*>(s);
    const channels_type *dst1 = reinterpret_cast<const channels_type*>(d1);
    const channels_type *dst2 = reinterpret_cast<const channels_type*>(d2);

    qDebug() << "Wrong rounding in pixel:" << pos;
    qDebug() << "Vector version: " << dst1[0] << dst1[1] << dst1[2] << dst1[3];
    qDebug() << "Scalar version: " << dst2[0] << dst2[1] << dst2[2] << dst2[3];
    qDebug() << "Dif: " << dst1[0] - dst2[0] << dst1[1] - dst2[1] << dst1[2] - dst2[2] << dst1[3] - dst2[3];

    qDebug() << "src:" << src1[0] << src1[1] << src1[2] << src1[3];
    qDebug() << "msk:" << msk1[0];
}

template<class Compositor>
void checkRounding(qreal opacity, qreal flow, qreal averageOpacity = -1, quint32 pixelSize = 4)
{
    QVector<Tile> tiles =
        generateTiles(2, 0, 0, ALPHA_RANDOM, ALPHA_RANDOM, pixelSize);

    const int vecSize = float_v::size;

    const int numBlocks = numPixels / vecSize;

    quint8 *src1 = tiles[0].src;
    quint8 *dst1 = tiles[0].dst;
    quint8 *msk1 = tiles[0].mask;

    quint8 *src2 = tiles[1].src;
    quint8 *dst2 = tiles[1].dst;
    quint8 *msk2 = tiles[1].mask;

    KoCompositeOp::ParameterInfo params;
    params.opacity = opacity;
    params.flow = flow;

    if (averageOpacity >= 0.0) {
        params._lastOpacityData = averageOpacity;
        params.lastOpacity = &params._lastOpacityData;
    }

    params.channelFlags = QBitArray();
    typename Compositor::ParamsWrapper paramsWrapper(params);

    // The error count is needed as 38.5 gets rounded to 38 instead of 39 in the vc version.
    int errorcount = 0;
    for (int i = 0; i < numBlocks; i++) {
        Compositor::template compositeVector<true,true, xsimd::current_arch>(src1, dst1, msk1, params.opacity, paramsWrapper);
        for (int j = 0; j < vecSize; j++) {

            //if (8 * i + j == 7080) {
            //    qDebug() << "src: " << src2[0] << src2[1] << src2[2] << src2[3];
            //    qDebug() << "dst: " << dst2[0] << dst2[1] << dst2[2] << dst2[3];
            //    qDebug() << "msk:" << msk2[0];
            //}

            Compositor::template compositeOnePixelScalar<true, xsimd::current_arch>(src2, dst2, msk2, params.opacity, paramsWrapper);

            bool compareResult = true;
            if (pixelSize == 4) {
                compareResult = comparePixels<quint8>(dst1, dst2, 0);
                if (!compareResult) {
                    ++errorcount;
                    compareResult = comparePixels<quint8>(dst1, dst2, 1);
                    if (!compareResult) {
                        ++errorcount;
                    }
                }
            }
            else if (pixelSize == 8) {
                compareResult = comparePixels<quint16>(reinterpret_cast<quint16*>(dst1), reinterpret_cast<quint16*>(dst2), 0);
            }
            else if (pixelSize == 16) {
                compareResult = comparePixels<float>(reinterpret_cast<float*>(dst1), reinterpret_cast<float*>(dst2), 0);
            }
            else {
                qFatal("Pixel size %i is not implemented", pixelSize);
            }

            if(!compareResult || errorcount > 1) {
                if (pixelSize == 4) {
                    printError<quint8>(src1, dst1, dst2, msk1, 8 * i + j);
                } else if (pixelSize == 8) {
                    printError<quint16>(src1, dst1, dst2, msk1, 8 * i + j);
                } else if (pixelSize == 16) {
                    printError<float>(src1, dst1, dst2, msk1, 8 * i + j);
                } else {
                    qFatal("Pixel size %i is not implemented", pixelSize);
                }

                QFAIL("Wrong rounding");
            }

            src1 += pixelSize;
            dst1 += pixelSize;
            src2 += pixelSize;
            dst2 += pixelSize;
            msk1++;
            msk2++;
        }
    }

    freeTiles(tiles, 0, 0);
}

#endif


void KisCompositionBenchmark::detectBuildArchitecture()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    using namespace xsimd;

    qDebug() << "built for" << ppVar(current_arch().name());
    qDebug() << "built for" << ppVar(default_arch().name());

    qDebug() << ppVar(supported_architectures().contains<sse2>());
    qDebug() << ppVar(supported_architectures().contains<sse3>());
    qDebug() << ppVar(supported_architectures().contains<ssse3>());
    qDebug() << ppVar(supported_architectures().contains<sse4_1>());
    qDebug() << ppVar(supported_architectures().contains<sse4_2>());
    qDebug() << ppVar(supported_architectures().contains<fma3<sse4_2>>());

    qDebug() << ppVar(supported_architectures().contains<avx>());
    qDebug() << ppVar(supported_architectures().contains<avx2>());
    qDebug() << ppVar(supported_architectures().contains<fma3<avx2>>());
    qDebug() << ppVar(supported_architectures().contains<fma4>());
    qDebug() << ppVar(supported_architectures().contains<avx512f>());
    qDebug() << ppVar(supported_architectures().contains<avx512bw>());
    qDebug() << ppVar(supported_architectures().contains<avx512dq>());
    qDebug() << ppVar(supported_architectures().contains<avx512cd>());
    //    qDebug().nospace() << "running on " << Qt::hex << "0x" << xsimd::available_architectures().best;
#endif
}

void KisCompositionBenchmark::checkRoundingAlphaDarken_05_03()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    checkRounding<AlphaDarkenCompositor32<quint8, quint32, KoAlphaDarkenParamsWrapperCreamy> >(0.5,0.3);
#endif
}

void KisCompositionBenchmark::checkRoundingAlphaDarken_05_05()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    checkRounding<AlphaDarkenCompositor32<quint8, quint32, KoAlphaDarkenParamsWrapperCreamy> >(0.5,0.5);
#endif
}

void KisCompositionBenchmark::checkRoundingAlphaDarken_05_07()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    checkRounding<AlphaDarkenCompositor32<quint8, quint32, KoAlphaDarkenParamsWrapperCreamy> >(0.5,0.7);
#endif
}

void KisCompositionBenchmark::checkRoundingAlphaDarken_05_10()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    checkRounding<AlphaDarkenCompositor32<quint8, quint32, KoAlphaDarkenParamsWrapperCreamy> >(0.5,1.0);
#endif
}

void KisCompositionBenchmark::checkRoundingAlphaDarken_05_10_08()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    checkRounding<AlphaDarkenCompositor32<quint8, quint32, KoAlphaDarkenParamsWrapperCreamy> >(0.5,1.0,0.8);
#endif
}

void KisCompositionBenchmark::checkRoundingAlphaDarkenF32_05_03()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    checkRounding<OverCompositor128<float, false, true> >(0.5, 0.3, -1, 16);
#endif
}

void KisCompositionBenchmark::checkRoundingAlphaDarkenF32_05_05()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    checkRounding<OverCompositor128<float, false, true> >(0.5, 0.5, -1, 16);
#endif
}

void KisCompositionBenchmark::checkRoundingAlphaDarkenF32_05_07()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    checkRounding<OverCompositor128<float, false, true> >(0.5, 0.7, -1, 16);
#endif
}

void KisCompositionBenchmark::checkRoundingAlphaDarkenF32_05_10()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    checkRounding<OverCompositor128<float, false, true> >(0.5, 1.0, -1, 16);
#endif
}

void KisCompositionBenchmark::checkRoundingAlphaDarkenF32_05_10_08()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    checkRounding<OverCompositor128<float, false, true> >(0.5, 1.0, 0.8, 16);
#endif
}

void KisCompositionBenchmark::checkRoundingOver()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    checkRounding<OverCompositor32<quint8, quint32, false, true> >(0.5, 0.3);
#endif
}

void KisCompositionBenchmark::checkRoundingOverRgbaU16()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    checkRounding<OverCompositor128<quint16, false, true> >(0.5, 1.0, -1, 8);
#endif
}

void KisCompositionBenchmark::checkRoundingOverRgbaF32()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    checkRounding<OverCompositor128<float, false, true> >(0.5, 1.0, -1, 16);
#endif
}
#include <cfenv>
void KisCompositionBenchmark::checkRoundingCopyRgbaU16()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    checkRounding<CopyCompositor128<quint16, false, true> >(0.5, 1.0, -1, 8);
#endif
}

void KisCompositionBenchmark::checkRoundingCopyRgbaF32()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    checkRounding<CopyCompositor128<float, false, true> >(0.5, 1.0, -1, 16);
#endif
}

void KisCompositionBenchmark::compareAlphaDarkenOps()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KoCompositeOp *opAct = KoOptimizedCompositeOpFactory::createAlphaDarkenOpCreamy32(cs);
    KoCompositeOp *opExp = new KoCompositeOpAlphaDarken<KoBgrU8Traits, KoAlphaDarkenParamsWrapperCreamy>(cs);

    QVERIFY(compareTwoOps(true, opAct, opExp));

    delete opExp;
    delete opAct;
}

void KisCompositionBenchmark::compareRgbF32AlphaDarkenOps()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->colorSpace("RGBA", "F32", "");
    KoCompositeOp *opAct = KoOptimizedCompositeOpFactory::createAlphaDarkenOpCreamy128(cs);
    KoCompositeOp *opExp = new KoCompositeOpAlphaDarken<KoRgbF32Traits, KoAlphaDarkenParamsWrapperCreamy>(cs);

    QVERIFY(compareTwoOps(true, opAct, opExp));

    delete opExp;
    delete opAct;
}

void KisCompositionBenchmark::compareAlphaDarkenOpsNoMask()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KoCompositeOp *opAct = KoOptimizedCompositeOpFactory::createAlphaDarkenOpCreamy32(cs);
    KoCompositeOp *opExp = new KoCompositeOpAlphaDarken<KoBgrU8Traits, KoAlphaDarkenParamsWrapperCreamy>(cs);

    QVERIFY(compareTwoOps(false, opAct, opExp));

    delete opExp;
    delete opAct;
}

void KisCompositionBenchmark::compareRgbU16AlphaDarkenOps()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb16();
    KoCompositeOp *opAct = KoOptimizedCompositeOpFactory::createAlphaDarkenOpCreamyU64(cs);
    KoCompositeOp *opExp = new KoCompositeOpAlphaDarken<KoRgbU16Traits, KoAlphaDarkenParamsWrapperCreamy>(cs);

    QVERIFY(compareTwoOps(true, opAct, opExp));

    delete opExp;
    delete opAct;
}

void KisCompositionBenchmark::compareOverOps()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KoCompositeOp *opAct = KoOptimizedCompositeOpFactory::createOverOp32(cs);
    KoCompositeOp *opExp = new KoCompositeOpOver<KoBgrU8Traits>(cs);

    QVERIFY(compareTwoOps(true, opAct, opExp));

    delete opExp;
    delete opAct;
}

void KisCompositionBenchmark::compareOverOpsNoMask()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KoCompositeOp *opAct = KoOptimizedCompositeOpFactory::createOverOp32(cs);
    KoCompositeOp *opExp = new KoCompositeOpOver<KoBgrU8Traits>(cs);

    QVERIFY(compareTwoOps(false, opAct, opExp));

    delete opExp;
    delete opAct;
}

void KisCompositionBenchmark::compareRgbU16OverOps()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb16();
    KoCompositeOp *opAct = KoOptimizedCompositeOpFactory::createOverOpU64(cs);
    KoCompositeOp *opExp = new KoCompositeOpOver<KoRgbU16Traits>(cs);

    QVERIFY(compareTwoOps(false, opAct, opExp));

    delete opExp;
    delete opAct;
}

void KisCompositionBenchmark::compareRgbF32OverOps()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->colorSpace("RGBA", "F32", "");
    KoCompositeOp *opAct = KoOptimizedCompositeOpFactory::createOverOp128(cs);
    KoCompositeOp *opExp = new KoCompositeOpOver<KoRgbF32Traits>(cs);

    QVERIFY(compareTwoOps(false, opAct, opExp));

    delete opExp;
    delete opAct;
}

void KisCompositionBenchmark::compareRgbU8CopyOps()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KoCompositeOp *opAct = KoOptimizedCompositeOpFactory::createCopyOp32(cs);
    KoCompositeOp *opExp = new KoCompositeOpCopy2<KoRgbU8Traits>(cs);

    // Since composite copy involves a channel division operation,
    // there might be significant rounding difference with purely
    // integer implementation. So we should compare in premultiplied
    // form.
    QVERIFY(compareTwoOps<PixelEqualPremultiplied>(false, opAct, opExp));

    delete opExp;
    delete opAct;
}

void KisCompositionBenchmark::compareRgbU16CopyOps()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb16();
    KoCompositeOp *opAct = KoOptimizedCompositeOpFactory::createCopyOpU64(cs);
    KoCompositeOp *opExp = new KoCompositeOpCopy2<KoRgbU16Traits>(cs);

    QVERIFY(compareTwoOps(false, opAct, opExp));

    delete opExp;
    delete opAct;
}

void KisCompositionBenchmark::compareRgbF32CopyOps()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->colorSpace("RGBA", "F32", "");
    KoCompositeOp *opAct = KoOptimizedCompositeOpFactory::createCopyOp128(cs);
    KoCompositeOp *opExp = new KoCompositeOpCopy2<KoRgbF32Traits>(cs);

    QVERIFY(compareTwoOps(false, opAct, opExp));

    delete opExp;
    delete opAct;
}

void KisCompositionBenchmark::testRgb8CompositeAlphaDarkenLegacy()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KoCompositeOp *op = new KoCompositeOpAlphaDarken<KoBgrU8Traits, KoAlphaDarkenParamsWrapperCreamy>(cs);
    benchmarkCompositeOp(op, "Legacy");
    delete op;
}

void KisCompositionBenchmark::testRgb8CompositeAlphaDarkenOptimized()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KoCompositeOp *op = KoOptimizedCompositeOpFactory::createAlphaDarkenOpCreamy32(cs);
    benchmarkCompositeOp(op, "Optimized");
    delete op;
}

void KisCompositionBenchmark::testRgb8CompositeOverLegacy()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KoCompositeOp *op = new KoCompositeOpOver<KoBgrU8Traits>(cs);
    benchmarkCompositeOp(op, "Legacy");
    delete op;
}

void KisCompositionBenchmark::testRgb8CompositeOverOptimized()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KoCompositeOp *op = KoOptimizedCompositeOpFactory::createOverOp32(cs);
    benchmarkCompositeOp(op, "Optimized");
    delete op;
}

void KisCompositionBenchmark::testRgb16CompositeAlphaDarkenLegacy()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb16();
    KoCompositeOp *op = new KoCompositeOpAlphaDarken<KoBgrU16Traits, KoAlphaDarkenParamsWrapperCreamy>(cs);
    benchmarkCompositeOp(op, "Legacy");
    delete op;
}

void KisCompositionBenchmark::testRgb16CompositeAlphaDarkenOptimized()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb16();
    KoCompositeOp *op = KoOptimizedCompositeOpFactory::createAlphaDarkenOpCreamyU64(cs);
    benchmarkCompositeOp(op, "Optimized");
    delete op;
}

void KisCompositionBenchmark::testRgb16CompositeOverLegacy()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb16();
    KoCompositeOp *op = new KoCompositeOpOver<KoBgrU16Traits>(cs);
    benchmarkCompositeOp(op, "Legacy");
    delete op;
}

void KisCompositionBenchmark::testRgb16CompositeOverOptimized()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb16();
    KoCompositeOp *op = KoOptimizedCompositeOpFactory::createOverOpU64(cs);
    benchmarkCompositeOp(op, "Optimized");
    delete op;
}


void KisCompositionBenchmark::testRgb16CompositeCopyLegacy()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb16();
    KoCompositeOp *op = new KoCompositeOpCopy2<KoBgrU16Traits>(cs);
    benchmarkCompositeOp(op, "Legacy");
    delete op;
}

void KisCompositionBenchmark::testRgb16CompositeCopyOptimized()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb16();
    KoCompositeOp *op = KoOptimizedCompositeOpFactory::createCopyOpU64(cs);
    benchmarkCompositeOp(op, "Optimized");
    delete op;
}
void KisCompositionBenchmark::testRgbF32CompositeAlphaDarkenLegacy()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->colorSpace("RGBA", "F32", "");
    KoCompositeOp *op = new KoCompositeOpAlphaDarken<KoRgbF32Traits, KoAlphaDarkenParamsWrapperCreamy>(cs);
    benchmarkCompositeOp(op, "Legacy");
    delete op;
}

void KisCompositionBenchmark::testRgbF32CompositeAlphaDarkenOptimized()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->colorSpace("RGBA", "F32", "");
    KoCompositeOp *op = KoOptimizedCompositeOpFactory::createAlphaDarkenOpCreamy128(cs);
    benchmarkCompositeOp(op, "Optimized");
    delete op;
}

void KisCompositionBenchmark::testRgbF32CompositeOverLegacy()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->colorSpace("RGBA", "F32", "");
    KoCompositeOp *op = new KoCompositeOpOver<KoRgbF32Traits>(cs);
    benchmarkCompositeOp(op, "RGBF32 Legacy");
    delete op;
}

void KisCompositionBenchmark::testRgbF32CompositeOverOptimized()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->colorSpace("RGBA", "F32", "");
    KoCompositeOp *op = KoOptimizedCompositeOpFactory::createOverOp128(cs);
    benchmarkCompositeOp(op, "RGBF32 Optimized");
    delete op;
}

void KisCompositionBenchmark::testRgbF32CompositeCopyLegacy()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->colorSpace("RGBA", "F32", "");
    KoCompositeOp *op = new KoCompositeOpCopy2<KoRgbF32Traits>(cs);
    benchmarkCompositeOp(op, "RGBF32 Legacy");
    delete op;
}

void KisCompositionBenchmark::testRgbF32CompositeCopyOptimized()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->colorSpace("RGBA", "F32", "");
    KoCompositeOp *op = KoOptimizedCompositeOpFactory::createCopyOp128(cs);
    benchmarkCompositeOp(op, "RGBF32 Optimized");
    delete op;
}

void KisCompositionBenchmark::testRgb8CompositeAlphaDarkenReal_Aligned()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    const KoCompositeOp *op = cs->compositeOp(COMPOSITE_ALPHA_DARKEN);
    benchmarkCompositeOp(op, true, 0.5, 0.3, 0, 0, ALPHA_RANDOM, ALPHA_RANDOM);
}

void KisCompositionBenchmark::testRgb8CompositeOverReal_Aligned()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    const KoCompositeOp *op = cs->compositeOp(COMPOSITE_OVER);
    benchmarkCompositeOp(op, true, 0.5, 0.3, 0, 0, ALPHA_RANDOM, ALPHA_RANDOM);
}

void KisCompositionBenchmark::testRgb8CompositeCopyLegacy()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KoCompositeOp *op = new KoCompositeOpCopy2<KoBgrU8Traits>(cs);
    benchmarkCompositeOp(op, "Copy");
    delete op;
}

void KisCompositionBenchmark::testRgb8CompositeCopyOptimized()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KoCompositeOp *op = KoOptimizedCompositeOpFactory::createCopyOp32(cs);
    benchmarkCompositeOp(op, "Optimized");
    delete op;
}

void KisCompositionBenchmark::benchmarkMemcpy()
{
    QVector<Tile> tiles =
        generateTiles(numTiles, 0, 0, ALPHA_UNIT, ALPHA_UNIT, 4);

    QBENCHMARK_ONCE {
        Q_FOREACH (const Tile &tile, tiles) {
            memcpy(tile.dst, tile.src, 4 * numPixels);
        }
    }

    freeTiles(tiles, 0, 0);
}

#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
const int vecSize = float_v::size;
const size_t uint8VecAlignment = qMax(vecSize * sizeof(quint8), sizeof(void *));
const size_t uint32VecAlignment = qMax(vecSize * sizeof(quint32), sizeof(void *));
const size_t floatVecAlignment = qMax(vecSize * sizeof(float), sizeof(void *));
#endif

void KisCompositionBenchmark::benchmarkUintFloat()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    using uint_v = xsimd::batch<unsigned int, xsimd::current_arch>;

    const int dataSize = 4096;
    void *ptr = 0;
    int error = MEMALIGN_ALLOC(&ptr, uint8VecAlignment, dataSize);
    if (error) {
        qFatal("posix_memalign failed: %d", error);
    }
    quint8 *iData = (quint8*)ptr;
    error = MEMALIGN_ALLOC(&ptr, floatVecAlignment, dataSize * sizeof(float));
    if (error) {
        qFatal("posix_memalign failed: %d", error);
    }
    float *fData = (float*)ptr;

    QBENCHMARK {
        for (int i = 0; i < dataSize; i += float_v::size) {
            // convert uint -> float directly, this causes
            // static_cast helper be called
            const auto b = xsimd::batch_cast<typename float_v::value_type>(
                xsimd::load_and_extend<uint_v>(iData + i)
            );
            b.store_aligned(fData + i);
        }
    }

    MEMALIGN_FREE(iData);
    MEMALIGN_FREE(fData);
#endif
}

void KisCompositionBenchmark::benchmarkUintIntFloat()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    using uint_v = xsimd::batch<unsigned int, xsimd::current_arch>;

    const int dataSize = 4096;
    void *ptr = 0;
    int error = MEMALIGN_ALLOC(&ptr, uint8VecAlignment, dataSize);
    if (error) {
        qFatal("posix_memalign failed: %d", error);
    }
    quint8 *iData = (quint8*)ptr;
    error = MEMALIGN_ALLOC(&ptr, floatVecAlignment, dataSize * sizeof(float));
    if (error) {
        qFatal("posix_memalign failed: %d", error);
    }
    float *fData = (float*)ptr;

    QBENCHMARK {
        for (int i = 0; i < dataSize; i += float_v::size) {
            // convert uint->int->float, that avoids special sign
            // treating, and gives 2.6 times speedup
            const auto b = xsimd::batch_cast<typename float_v::value_type>(xsimd::load_and_extend<uint_v>(iData + i));
            b.store_aligned(fData + i);
        }
    }

    MEMALIGN_FREE(iData);
    MEMALIGN_FREE(fData);
#endif
}

void KisCompositionBenchmark::benchmarkFloatUint()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    using uint_v = xsimd::batch<unsigned int, xsimd::current_arch>;

    const int dataSize = 4096;
    void *ptr = 0;
    int error = MEMALIGN_ALLOC(&ptr, uint32VecAlignment, dataSize * sizeof(quint32));
    if (error) {
        qFatal("posix_memalign failed: %d", error);
    }
    quint32 *iData = (quint32*)ptr;
    error = MEMALIGN_ALLOC(&ptr, floatVecAlignment, dataSize * sizeof(float));
    if (error) {
        qFatal("posix_memalign failed: %d", error);
    }
    float *fData = (float*)ptr;

    QBENCHMARK {
        for (int i = 0; i < dataSize; i += float_v::size) {
            // conversion float -> uint
            // this being a direct conversion, load_and_extend does not apply
            const auto b = xsimd::batch_cast<typename uint_v::value_type>(float_v::load_aligned(fData + i));

            b.store_aligned(iData + i);
        }
    }

    MEMALIGN_FREE(iData);
    MEMALIGN_FREE(fData);
#endif
}

void KisCompositionBenchmark::benchmarkFloatIntUint()
{
#if !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE) && XSIMD_UNIVERSAL_BUILD_PASS
    using uint_v = xsimd::batch<unsigned int, xsimd::current_arch>;
    const int dataSize = 4096;
    void *ptr = 0;
    int error = MEMALIGN_ALLOC(&ptr, uint32VecAlignment, dataSize * sizeof(quint32));
    if (error) {
        qFatal("posix_memalign failed: %d", error);
    }
    quint32 *iData = (quint32*)ptr;
    error = MEMALIGN_ALLOC(&ptr, floatVecAlignment, dataSize * sizeof(float));
    if (error) {
        qFatal("posix_memalign failed: %d", error);
    }
    float *fData = (float*)ptr;

    QBENCHMARK {
        for (int i = 0; i < dataSize; i += float_v::size) {
            // conversion float -> int -> uint
            const auto b = xsimd::batch_cast<typename uint_v::value_type>(float_v::load_aligned(fData + i));

            b.store_aligned(iData + i);
        }
    }

    MEMALIGN_FREE(iData);
    MEMALIGN_FREE(fData);
#endif
}

SIMPLE_TEST_MAIN(KisCompositionBenchmark)

