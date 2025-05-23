/*
 *  SPDX-FileCopyrightText: 2006, 2007, 2010 Cyrille Berger <cberger@cberger.bet
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef KOCOLORSPACEMATHS_H_
#define KOCOLORSPACEMATHS_H_

#include <cmath>
#include <limits>

#include "kritapigment_export.h"
#include <KoIntegerMaths.h>
#include "KoChannelInfo.h"
#include "KoLut.h"

#include <kis_global.h>

#undef _T

/**
 * This is an empty mainWindow that needs to be "specialized" for each possible
 * numerical type (quint8, quint16...).
 *
 * It needs to defines some static constant fields :
 * - zeroValue : the zero for this numerical type
 * - unitValue : the maximum value of the normal dynamic range
 * - max : the maximum value
 * - min : the minimum value
 * - epsilon : a value close to zero but different of zero
 * - bits : the bit depth
 *
 * And some types :
 * - compositetype the type used for composite operations (usually one with
 *   a higher bit depth)
 */
template<typename _T>
class KoColorSpaceMathsTraits
{
public:
};

template<>
class KRITAPIGMENT_EXPORT KoColorSpaceMathsTraits<quint8>
{
public:
    typedef qint32 compositetype;
    typedef qint64 mixtype;
    static constexpr quint8 zeroValue = 0;
    static constexpr quint8 unitValue = 0x00FF;
    static constexpr quint8 halfValue = 0x00FF / 2;
    static constexpr quint8 max = 0x00FF;
    static constexpr quint8 min = 0;
    static constexpr quint8 epsilon = 1;
    static constexpr qint8 bits = 8;
    static const KoChannelInfo::enumChannelValueType channelValueType;
};

template<>
class KRITAPIGMENT_EXPORT KoColorSpaceMathsTraits<quint16>
{
public:
    typedef qint64 compositetype;
    typedef qint64 mixtype;
    static constexpr quint16 zeroValue = 0;
    static constexpr quint16 unitValue = 0xFFFF;
    static constexpr quint16 halfValue = 0xFFFF / 2;
    static constexpr quint16 max = 0xFFFF;
    static constexpr quint16 min = 0;
    static constexpr quint16 epsilon = 1;
    static constexpr qint8 bits = 16;
    static const KoChannelInfo::enumChannelValueType channelValueType;
};

template<>
class KRITAPIGMENT_EXPORT KoColorSpaceMathsTraits<qint16>
{
public:
    typedef qint64 compositetype;
    typedef qint64 mixtype;
    static constexpr qint16 zeroValue = 0;
    static constexpr qint16 unitValue = 32767;
    static constexpr qint16 halfValue = 32767 / 2;
    static constexpr qint16 max = 32767;
    static constexpr qint16 min = -32768;
    static constexpr qint16 epsilon = 1;
    static constexpr qint8 bits = 16;
    static const KoChannelInfo::enumChannelValueType channelValueType;
};

template<>
class KRITAPIGMENT_EXPORT KoColorSpaceMathsTraits<quint32>
{
public:
    typedef qint64 compositetype;
    typedef qint64 mixtype;
    static constexpr quint32 zeroValue = 0;
    static constexpr quint32 unitValue = 0xFFFFFFFF;
    static constexpr quint32 halfValue = 0xFFFFFFFF / 2;
    static constexpr quint32 max = 0xFFFFFFFF;
    static constexpr quint32 min = 0;
    static constexpr quint32 epsilon = 1;
    static constexpr qint8 bits = 32;
    static const KoChannelInfo::enumChannelValueType channelValueType;
};

#include <KoConfig.h>
#ifdef HAVE_OPENEXR
#include <half.h>
#include <KisHalfTraits.h>

template<>
class KRITAPIGMENT_EXPORT KoColorSpaceMathsTraits<half>
{
public:
    typedef double compositetype;
    typedef double mixtype;
    static const half zeroValue;
    static const half unitValue;
    static const half halfValue;
    static const half max;
    static const half min;
    static const half epsilon;
    static constexpr qint8 bits = 16;
    static const KoChannelInfo::enumChannelValueType channelValueType;
};
#endif

template<>
class KRITAPIGMENT_EXPORT KoColorSpaceMathsTraits<float>
{
public:
    typedef float compositetype;
    typedef double mixtype;
    static const float zeroValue;
    static const float unitValue;
    static const float halfValue;
    static const float max;
    static const float min;
    static const float epsilon;
    static constexpr qint8 bits = 32;
    static const KoChannelInfo::enumChannelValueType channelValueType;
};

template<>
class KRITAPIGMENT_EXPORT KoColorSpaceMathsTraits<double>
{
public:
    typedef double compositetype;
    typedef double mixtype;
    static const double zeroValue;
    static const double unitValue;
    static const double halfValue;
    static const double max;
    static const double min;
    static const double epsilon;
    static constexpr qint8 bits = 64;
    static const KoChannelInfo::enumChannelValueType channelValueType;
};

inline int float2int(float x)
{
    // NOTE: we don't use rint() here, because on Windows
    //       it falls back to x87 instructions on Intel CPUs,
    //       which are executed extremely slowly
    // NOTE2: we do always clamp value to 0...0xff range
    //        before calling this function, so `x` cannot be
    //        negative.
    return int(x + 0.5f);
}

inline int float2int(double x)
{
    // NOTE: we don't use rint() here, because on Windows
    //       it falls back to x87 instructions on Intel CPUs,
    //       which are executed extremely slowly
    // NOTE2: we do always clamp value to 0...0xff range
    //        before calling this function, so `x` cannot be
    //        negative.
    return int(x + 0.5);
}

template<typename _T_>
struct KoIntegerToFloat {
  inline float operator()(_T_ f) const
  {
    return f / float(KoColorSpaceMathsTraits<_T_>::max);
  }
};

struct KoLuts {

  static KRITAPIGMENT_EXPORT const Ko::FullLut< KoIntegerToFloat<quint16>, float, quint16> Uint16ToFloat;
  static KRITAPIGMENT_EXPORT const Ko::FullLut< KoIntegerToFloat<quint8>, float, quint8> Uint8ToFloat;
};

/**
 * This class defines some elementary operations used by various color
 * space. It's intended to be generic, but some specialization exists
 * either for optimization or just for being buildable.
 *
 * @param _T some numerical type with an existing trait
 * @param _Tdst some other numerical type with an existing trait, it is
 *              only needed if different of _T
 */
template < typename _T, typename _Tdst = _T >
class KoColorSpaceMaths
{
    typedef KoColorSpaceMathsTraits<_T> traits;
    typedef typename traits::compositetype src_compositetype;
    typedef typename KoColorSpaceMathsTraits<_Tdst>::compositetype dst_compositetype;
    
public:
    inline static _Tdst multiply(_T a, _Tdst b) {
        return (dst_compositetype(a)*b) /  KoColorSpaceMathsTraits<_Tdst>::unitValue;
    }
    
    inline static _Tdst multiply(_T a, _Tdst b, _Tdst c) {
        return (dst_compositetype(a)*b*c) / (dst_compositetype(KoColorSpaceMathsTraits<_Tdst>::unitValue) * KoColorSpaceMathsTraits<_T>::unitValue);
    }

    /**
     * Division : (a * MAX ) / b
     * @param a
     * @param b
     */
    inline static dst_compositetype divide(_T a, _Tdst b) {
        return (dst_compositetype(a) *  KoColorSpaceMathsTraits<_Tdst>::unitValue) / b;
    }
    
    inline static dst_compositetype modulus(_T a, _Tdst b) {
        return (dst_compositetype(a) - floor(dst_compositetype(a)/((b != (KoColorSpaceMathsTraits<_T>::zeroValue - traits::epsilon) ? b : KoColorSpaceMathsTraits<_T>::zeroValue)  + traits::epsilon))*(b + traits::epsilon));
    }

    inline static dst_compositetype xor(_T a, _Tdst b) {
        return (int (a *  std::numeric_limits<int>::max() - traits::epsilon) ^ int (b *  std::numeric_limits<int>::max() - traits::epsilon));
    }

    inline static dst_compositetype and(_T a, _Tdst b) {
        return (int (a *  std::numeric_limits<int>::max()  - traits::epsilon) & int (b *  std::numeric_limits<int>::max()  - traits::epsilon));
    }
    
    inline static dst_compositetype or(_T a, _Tdst b) {
        return (int (a *  std::numeric_limits<int>::max()  - traits::epsilon) | int (b *  std::numeric_limits<int>::max()  - traits::epsilon));
    }

    /**
     * Inversion : unitValue - a
     * @param a
     */
    inline static _T invert(_T a) {
        return traits::unitValue - a;
    }

    /**
     * Blending : (a * alpha) + b * (1 - alpha)
     * @param a
     * @param b
     * @param alpha
     */
    inline static _T blend(_T a, _T b, _T alpha) {
        src_compositetype c = ((src_compositetype(a) - b) * alpha) / traits::unitValue;
        return c + b;
    }

    /**
     * This function will scale a value of type _T to fit into a _Tdst.
     */
    inline static _Tdst scaleToA(_T a) {
        return _Tdst(dst_compositetype(a) * KoColorSpaceMathsTraits<_Tdst>::unitValue / KoColorSpaceMathsTraits<_T>::unitValue);
    }

    inline static dst_compositetype clamp(dst_compositetype val) {
        return kisBoundFast<dst_compositetype>(KoColorSpaceMathsTraits<_Tdst>::min, val, KoColorSpaceMathsTraits<_Tdst>::max);
    }

    inline static _T clampChannelToSDR(_T val) {
        if constexpr (std::numeric_limits<_T>::is_integer) {
            return val;
        } else {
            return kisBoundFast<_T>(KoColorSpaceMathsTraits<_T>::zeroValue, val, KoColorSpaceMathsTraits<_T>::unitValue);
        }
    }

    inline static _T clampChannelToSDRBottom(_T val)
    {
        if constexpr (std::numeric_limits<_T>::is_integer) {
            return val;
        } else {
            return qMax<_T>(KoColorSpaceMathsTraits<_T>::zeroValue, val);
        }
    }

    inline static dst_compositetype clampToSDR(dst_compositetype val) {
        return kisBoundFast<dst_compositetype>(KoColorSpaceMathsTraits<_Tdst>::zeroValue, val, KoColorSpaceMathsTraits<_Tdst>::unitValue);
    }

    inline static dst_compositetype clampToSDRTop(dst_compositetype val) {
        return qMin<dst_compositetype>(val, KoColorSpaceMathsTraits<_Tdst>::unitValue);
    }

    inline static dst_compositetype clampToSDRBottom(dst_compositetype val) {
        return qMax<dst_compositetype>(KoColorSpaceMathsTraits<_Tdst>::zeroValue, val);
    }

    inline static dst_compositetype divideInCompositeSpace(dst_compositetype a, dst_compositetype b) {
        if constexpr (std::numeric_limits<_Tdst>::is_integer) {
            return a * KoColorSpaceMathsTraits<_Tdst>::unitValue / b;
        } else {
            return a / b;
        }
    }

    inline static dst_compositetype multiplyInCompositeSpace(dst_compositetype a, dst_compositetype b) {
        if constexpr (std::numeric_limits<_Tdst>::is_integer) {
            return a * b / KoColorSpaceMathsTraits<_Tdst>::unitValue;
        } else {
            return a * b;
        }
    }

    /**
     * Clamps the composite type on higher border only. That is a fast path
     * for scale-only transformations
     */
    inline static _Tdst clampAfterScale(dst_compositetype val) {
        return qMin<dst_compositetype>(val, KoColorSpaceMathsTraits<_Tdst>::max);
    }

    inline static _T isUnsafeAsDivisor(_T value) {
        static_assert(std::numeric_limits<_Tdst>::is_integer);
        return value == KoColorSpaceMathsTraits<_T>::zeroValue;
    }

    inline static bool isUnitValueFuzzy(_T value) {
        static_assert(std::numeric_limits<_Tdst>::is_integer);
        return value == KoColorSpaceMathsTraits<_T>::unitValue;
    }

    inline static bool isZeroValueFuzzy(_T value) {
        static_assert(std::numeric_limits<_Tdst>::is_integer);
        return value == KoColorSpaceMathsTraits<_T>::zeroValue;
    }

    inline static bool isZeroValueClampedFuzzy(_T v) {
        static_assert(std::numeric_limits<_Tdst>::is_integer);
        return v <= 0;
    }

    static inline bool isUnitValueClampedFuzzy(_T v) {
        static_assert(std::numeric_limits<_Tdst>::is_integer);
        return v >= KoColorSpaceMathsTraits<_T>::unitValue;
    }

    static inline bool isHalfValueFuzzy(_T v)
    {
        static_assert(std::numeric_limits<_Tdst>::is_integer);
        return v == KoColorSpaceMathsTraits<_T>::halfValue;
    }
};

//------------------------------ double specialization ------------------------------//
template<>
inline quint8 KoColorSpaceMaths<double, quint8>::scaleToA(double a)
{
    double v = a * 255;
    return float2int(CLAMP(v, 0, 255));
}

template<>
inline double KoColorSpaceMaths<quint8, double>::scaleToA(quint8 a)
{
    return KoLuts::Uint8ToFloat(a);
}

template<>
inline quint16 KoColorSpaceMaths<double, quint16>::scaleToA(double a)
{
    double v = a * 0xFFFF;
    return float2int(CLAMP(v, 0, 0xFFFF));
}

template<>
inline double KoColorSpaceMaths<quint16, double>::scaleToA(quint16 a)
{
    return KoLuts::Uint16ToFloat(a);
}

template<>
inline double KoColorSpaceMaths<double>::clamp(double a)
{
    return a;
}

template<>
inline double KoColorSpaceMaths<double>::isUnsafeAsDivisor(double value) {
    return value < 1e-6; // negative values are also unsafe!
}

template<>
inline bool KoColorSpaceMaths<double>::isUnitValueFuzzy(double value) {
    return qFuzzyCompare(value, KoColorSpaceMathsTraits<double>::unitValue);
}

template<>
inline bool KoColorSpaceMaths<double>::isZeroValueFuzzy(double value) {
    return qFuzzyIsNull(value);
}

template<>
inline bool KoColorSpaceMaths<double>::isZeroValueClampedFuzzy(double d)
{
    // constant is from qFuzzyIsNull()
    return d <= 0.000000000001;
}

template<>
inline bool KoColorSpaceMaths<double>::isUnitValueClampedFuzzy(double d)
{
    // constant is from qFuzzyIsNull()
    return d > 1.0 - 0.000000000001;
}

template<>
inline bool KoColorSpaceMaths<double>::isHalfValueFuzzy(double value) {
    return qFuzzyCompare(value, KoColorSpaceMathsTraits<double>::halfValue);
}

//------------------------------ float specialization ------------------------------//

template<>
inline float KoColorSpaceMaths<double, float>::scaleToA(double a)
{
    return (float)a;
}

template<>
inline double KoColorSpaceMaths<float, double>::scaleToA(float a)
{
    return a;
}

template<>
inline quint16 KoColorSpaceMaths<float, quint16>::scaleToA(float a)
{
    float v = a * 0xFFFF;
    return (quint16)float2int(CLAMP(v, 0, 0xFFFF));
}

template<>
inline float KoColorSpaceMaths<quint16, float>::scaleToA(quint16 a)
{
    return KoLuts::Uint16ToFloat(a);
}

template<>
inline quint8 KoColorSpaceMaths<float, quint8>::scaleToA(float a)
{
    float v = a * 255;
    return (quint8)float2int(CLAMP(v, 0, 255));
}

template<>
inline float KoColorSpaceMaths<quint8, float>::scaleToA(quint8 a)
{
    return KoLuts::Uint8ToFloat(a);
}

template<>
inline float KoColorSpaceMaths<float>::blend(float a, float b, float alpha)
{
    return (a - b) * alpha + b;
}

template<>
inline float KoColorSpaceMaths<float>::clamp(float a)
{
    return a;
}

template<>
inline float KoColorSpaceMaths<float>::isUnsafeAsDivisor(float value) {
    return value < 1e-6; // negative values are also unsafe!
}

template<>
inline bool KoColorSpaceMaths<float>::isUnitValueFuzzy(float value) {
    return qFuzzyCompare(value, KoColorSpaceMathsTraits<float>::unitValue);
}

template<>
inline bool KoColorSpaceMaths<float>::isZeroValueFuzzy(float value) {
    return qFuzzyIsNull(value);
}

template<>
inline bool KoColorSpaceMaths<float>::isZeroValueClampedFuzzy(float f)
{
    // constant is from qFuzzyIsNull()
    return f <= 0.00001f;
}

template<>
inline bool KoColorSpaceMaths<float>::isUnitValueClampedFuzzy(float f)
{
    // constant is from qFuzzyIsNull()
    return f > 1.0 - 0.00001f;
}

template<>
inline bool KoColorSpaceMaths<float>::isHalfValueFuzzy(float value) {
    return qFuzzyCompare(value, KoColorSpaceMathsTraits<float>::halfValue);
}

//------------------------------ half specialization ------------------------------//

#ifdef HAVE_OPENEXR

template<>
inline half KoColorSpaceMaths<double, half>::scaleToA(double a)
{
    return (half)a;
}

template<>
inline double KoColorSpaceMaths<half, double>::scaleToA(half a)
{
    return a;
}

template<>
inline float KoColorSpaceMaths<half, float>::scaleToA(half a)
{
    return a;
}

template<>
inline half KoColorSpaceMaths<float, half>::scaleToA(float a)
{
    return (half) a;
}

template<>
inline quint8 KoColorSpaceMaths<half, quint8>::scaleToA(half a)
{
    half v = a * 255;
    return (quint8)(CLAMP(v, 0, 255));
}

template<>
inline half KoColorSpaceMaths<quint8, half>::scaleToA(quint8 a)
{
    return a *(1.0 / 255.0);
}
template<>
inline quint16 KoColorSpaceMaths<half, quint16>::scaleToA(half a)
{
    double v = a * 0xFFFF;
    return (quint16)(CLAMP(v, 0, 0xFFFF));
}

template<>
inline half KoColorSpaceMaths<quint16, half>::scaleToA(quint16 a)
{
    return a *(1.0 / 0xFFFF);
}

template<>
inline half KoColorSpaceMaths<half, half>::scaleToA(half a)
{
    return a;
}

template<>
inline half KoColorSpaceMaths<half>::blend(half a, half b, half alpha)
{
    return (a - b) * alpha + b;
}

template<>
inline double KoColorSpaceMaths<half>::clamp(double a)
{
    return a;
}

template<>
inline half KoColorSpaceMaths<half>::isUnsafeAsDivisor(half value) {
    return value < 1e-6; // negative values are also unsafe!
}

template<>
inline bool KoColorSpaceMaths<half>::isUnitValueFuzzy(half value) {
    // ~ 2 * HALF_ESPILON
    return qAbs(value - KoColorSpaceMathsTraits<half>::unitValue) < 0.002f;
}

template<>
inline bool KoColorSpaceMaths<half>::isZeroValueFuzzy(half value) {
    // ~ 2 * HALF_ESPILON
    return qAbs(value) < 0.002f;
}

template<>
inline bool KoColorSpaceMaths<half>::isZeroValueClampedFuzzy(half f)
{
    // ~ 2 * HALF_ESPILON
    return f <= 0.002f;
}

template<>
inline bool KoColorSpaceMaths<half>::isUnitValueClampedFuzzy(half f)
{
    // ~ 2 * HALF_ESPILON
    return f > 1.0 - 0.002f;
}

template<>
inline bool KoColorSpaceMaths<half>::isHalfValueFuzzy(half value) {
    // TODO: check actual constant
    return qAbs(value - 0.5f) < 0.001f;
}

#endif

//------------------------------ quint8 specialization ------------------------------//

template<>
inline quint8 KoColorSpaceMaths<quint8>::multiply(quint8 a, quint8 b)
{
    return (quint8)UINT8_MULT(a, b);
}


template<>
inline quint8 KoColorSpaceMaths<quint8>::multiply(quint8 a, quint8 b, quint8 c)
{
    return (quint8)UINT8_MULT3(a, b, c);
}

template<>
inline KoColorSpaceMathsTraits<quint8>::compositetype
KoColorSpaceMaths<quint8>::divide(quint8 a, quint8 b)
{
    return UINT8_DIVIDE(a, b);
}

template<>
inline quint8 KoColorSpaceMaths<quint8>::invert(quint8 a)
{
    return ~a;
}

template<>
inline quint8 KoColorSpaceMaths<quint8>::blend(quint8 a, quint8 b, quint8 c)
{
    return UINT8_BLEND(a, b, c);
}

//------------------------------ quint16 specialization ------------------------------//

template<>
inline quint16 KoColorSpaceMaths<quint16>::multiply(quint16 a, quint16 b)
{
    return (quint16)UINT16_MULT(a, b);
}

template<>
inline KoColorSpaceMathsTraits<quint16>::compositetype
KoColorSpaceMaths<quint16>::divide(quint16 a, quint16 b)
{
    return UINT16_DIVIDE(a, b);
}

template<>
inline quint16 KoColorSpaceMaths<quint16>::invert(quint16 a)
{
    return ~a;
}

//------------------------------ various specialization ------------------------------//


// TODO: use more functions from KoIntegersMaths to do the computation

/// This specialization is needed because the default implementation won't work when scaling up
template<>
inline quint16 KoColorSpaceMaths<quint8, quint16>::scaleToA(quint8 a)
{
    return UINT8_TO_UINT16(a);
}

template<>
inline quint8 KoColorSpaceMaths<quint16, quint8>::scaleToA(quint16 a)
{
    return UINT16_TO_UINT8(a);
}


// Due to once again a bug in gcc, there is the need for those specialized functions:

template<>
inline quint8 KoColorSpaceMaths<quint8, quint8>::scaleToA(quint8 a)
{
    return a;
}

template<>
inline quint16 KoColorSpaceMaths<quint16, quint16>::scaleToA(quint16 a)
{
    return a;
}

template<>
inline float KoColorSpaceMaths<float, float>::scaleToA(float a)
{
    return a;
}

namespace Arithmetic
{
    const static qreal pi = 3.14159265358979323846;
    
    template<class T>
    inline T mul(T a, T b) { return KoColorSpaceMaths<T>::multiply(a, b); }
    
    template<class T>
    inline T mul(T a, T b, T c) { return KoColorSpaceMaths<T>::multiply(a, b, c); }
    
//     template<class T>
//     inline T mul(T a, T b) {
//         typedef typename KoColorSpaceMathsTraits<T>::compositetype composite_type;
//         return T(composite_type(a) * b / KoColorSpaceMathsTraits<T>::unitValue);
//     }
//     
//     template<class T>
//     inline T mul(T a, T b, T c) {
//         typedef typename KoColorSpaceMathsTraits<T>::compositetype composite_type;
//         return T((composite_type(a) * b * c) / (composite_type(KoColorSpaceMathsTraits<T>::unitValue) * KoColorSpaceMathsTraits<T>::unitValue));
//     }
    
    template<class T>
    inline T inv(T a) { return KoColorSpaceMaths<T>::invert(a); }
    
    template<class T>
    inline T lerp(T a, T b, T alpha) { return KoColorSpaceMaths<T>::blend(b, a, alpha); }
    
    template<class TRet, class T>
    inline TRet scale(T a) { return KoColorSpaceMaths<T,TRet>::scaleToA(a); }
    
    template<class T>
    inline typename KoColorSpaceMathsTraits<T>::compositetype
    div(T a, T b) { return KoColorSpaceMaths<T>::divide(a, b); }

    template<class T>
    inline typename KoColorSpaceMathsTraits<T>::compositetype
    xor(T a, T b) { return KoColorSpaceMaths<T>::xor(a, b); }

    template<class T>
    inline typename KoColorSpaceMathsTraits<T>::compositetype
    and(T a, T b) { return KoColorSpaceMaths<T>::and(a, b); }

    template<class T>
    inline typename KoColorSpaceMathsTraits<T>::compositetype
    or(T a, T b) { return KoColorSpaceMaths<T>::or(a, b); }

    template<class T>
    inline T clamp(typename KoColorSpaceMathsTraits<T>::compositetype a) {
        return KoColorSpaceMaths<T>::clamp(a);
    }

    template<class T>
    inline T clampChannelToSDR(T a) {
        return KoColorSpaceMaths<T>::clampChannelToSDR(a);
    }

    template<class T>
    inline T clampChannelToSDRBottom(T a) {
        return KoColorSpaceMaths<T>::clampChannelToSDRBottom(a);
    }

    template<class T>
    inline T clampToSDR(typename KoColorSpaceMathsTraits<T>::compositetype a) {
        return KoColorSpaceMaths<T>::clampToSDR(a);
    }

    template<class T>
    inline T clampToSDRTop(typename KoColorSpaceMathsTraits<T>::compositetype a) {
        return KoColorSpaceMaths<T>::clampToSDRTop(a);
    }

    template<class T>
    inline T clampToSDRBottom(typename KoColorSpaceMathsTraits<T>::compositetype a) {
        return KoColorSpaceMaths<T>::clampToSDRBottom(a);
    }

    template<class T, typename composite_type = typename KoColorSpaceMathsTraits<T>::compositetype>
    inline composite_type divideInCompositeSpace(composite_type a, composite_type b) {
        return KoColorSpaceMaths<T>::divideInCompositeSpace(a, b);
    }

    template<class T, typename composite_type = typename KoColorSpaceMathsTraits<T>::compositetype>
    inline composite_type multiplyInCompositeSpace(composite_type a, composite_type b) {
        return KoColorSpaceMaths<T>::multiplyInCompositeSpace(a, b);
    }
    
    template <typename T>
    static inline bool isZeroValueFuzzy(T v)
    {
        return KoColorSpaceMaths<T>::isZeroValueFuzzy(v);
    }

    template <typename T>
    static inline bool isUnitValueFuzzy(T v)
    {
        return KoColorSpaceMaths<T>::isUnitValueFuzzy(v);
    }

    template <typename T>
    static inline bool isZeroValueClampedFuzzy(T v)
    {
        return KoColorSpaceMaths<T>::isZeroValueClampedFuzzy(v);
    }

    template <typename T>
    static inline bool isUnitValueClampedFuzzy(T v)
    {
        return KoColorSpaceMaths<T>::isUnitValueClampedFuzzy(v);
    }

    template <typename T>
    static inline bool isHalfValueFuzzy(T v)
    {
        return KoColorSpaceMaths<T>::isHalfValueFuzzy(v);
    }

    template <typename T>
    inline bool isUnitValueStrict(T value) {
        return value == KoColorSpaceMathsTraits<T>::unitValue;
    }
    template <typename T>
    inline bool isZeroValueStrict(T value) {
        return value == KoColorSpaceMathsTraits<T>::zeroValue;
    }

    template <typename T>
    inline bool isUnitValueClampedStrict(T value) {
        if constexpr (std::numeric_limits<T>::is_integer) {
            return value == KoColorSpaceMathsTraits<T>::unitValue;
        } else {
            return value >= KoColorSpaceMathsTraits<T>::unitValue;
        }
    }
    template <typename T>
    inline bool isZeroValueClampedStrict(T value) {
        if constexpr (std::numeric_limits<T>::is_integer) {
            return value == KoColorSpaceMathsTraits<T>::zeroValue;
        } else {
            return value <= KoColorSpaceMathsTraits<T>::zeroValue;
        }
    }


    template<class T>
    inline T min(T a, T b, T c) {
        b = (a < b) ? a : b;
        return (b < c) ? b : c;
    }
    
    template<class T>
    inline T max(T a, T b, T c) {
        b = (a > b) ? a : b;
        return (b > c) ? b : c;
    }
    
    template<class T>
    inline T zeroValue() { return KoColorSpaceMathsTraits<T>::zeroValue; }
    
    template<class T>
    inline T halfValue() { return KoColorSpaceMathsTraits<T>::halfValue; }
    
    template<class T>
    inline T unitValue() { return KoColorSpaceMathsTraits<T>::unitValue; }
    
    template<class T>
    inline T unionShapeOpacity(T a, T b) {
        typedef typename KoColorSpaceMathsTraits<T>::compositetype composite_type;
        return T(composite_type(a) + b - mul(a,b));
    }
    
    template<class T>
    inline T blend(T src, T srcAlpha, T dst, T dstAlpha, T cfValue) {
        return mul(inv(srcAlpha), dstAlpha, dst) + mul(inv(dstAlpha), srcAlpha, src) + mul(dstAlpha, srcAlpha, cfValue);
    }

    template<class T>
    inline T epsilon() { return KoColorSpaceMathsTraits<T>::epsilon; }
    
    template<class T>
    inline typename KoColorSpaceMathsTraits<T>::compositetype
    mod(T a, T b) { return KoColorSpaceMaths<T>::modulus(a, b); }    

    template<typename T>
    inline T isUnsafeAsDivisor(T value) {
        return KoColorSpaceMaths<T>::isUnsafeAsDivisor(value);
    }
}

struct HSYType
{
    template<class TReal>
    inline static TReal getLightness(TReal r, TReal g, TReal b) {
        return TReal(0.299)*r + TReal(0.587)*g + TReal(0.114)*b;
    }
    
    template<class TReal>
    inline static TReal getSaturation(TReal r, TReal g, TReal b) {
        return Arithmetic::max(r,g,b) - Arithmetic::min(r,g,b);
    }

    static constexpr bool lightnessIsAverage = true;
};

struct HSIType
{
    template<class TReal>
    inline static TReal getLightness(TReal r, TReal g, TReal b) {
        return (r + g + b) * TReal(0.33333333333333333333); // (r + g + b) / 3.0
    }
    
    template<class TReal>
    inline static TReal getSaturation(TReal r, TReal g, TReal b) {
        TReal max    = Arithmetic::max(r, g, b);
        TReal min    = Arithmetic::min(r, g, b);
        TReal chroma = max - min;
        
        return (chroma > std::numeric_limits<TReal>::epsilon()) ?
            (TReal(1.0) - min / getLightness(r, g, b)) : TReal(0.0);
    }

    static constexpr bool lightnessIsAverage = true;
};

struct HSLType
{
    template<class TReal>
    inline static TReal getLightness(TReal r, TReal g, TReal b) {
        TReal max = Arithmetic::max(r, g, b);
        TReal min = Arithmetic::min(r, g, b);
        return (max + min) * TReal(0.5);
    }
    
    template<class TReal>
    inline static TReal getSaturation(TReal r, TReal g, TReal b) {
        TReal max    = Arithmetic::max(r, g, b);
        TReal min    = Arithmetic::min(r, g, b);
        TReal chroma = max - min;
        TReal light  = (max + min) * TReal(0.5);
        TReal div    = TReal(1.0) - std::abs(TReal(2.0)*light - TReal(1.0));
        
        if(div > std::numeric_limits<TReal>::epsilon())
            return chroma / div;
        
        return TReal(0.0);
    }

    static constexpr bool lightnessIsAverage = true;
};

struct HSVType
{
    template<class TReal>
    inline static TReal getLightness(TReal r, TReal g, TReal b) {
        return Arithmetic::max(r,g,b);
    }
    
    template<class TReal>
    inline static TReal getSaturation(TReal r, TReal g, TReal b) {
        TReal max = Arithmetic::max(r, g, b);
        TReal min = Arithmetic::min(r, g, b);
        return (max > std::numeric_limits<TReal>::epsilon()) ? (max - min) / max : TReal(0.0);
    }

    static constexpr bool lightnessIsAverage = false;
};

template<class TReal>
TReal getHue(TReal r, TReal g, TReal b) {
    TReal min    = Arithmetic::min(r, g, b);
    TReal max    = Arithmetic::max(r, g, b);
    TReal chroma = max - min;
    
    TReal hue = TReal(-1.0);
    
    if(chroma > std::numeric_limits<TReal>::epsilon()) {
        
//         return atan2(TReal(2.0)*r - g - b, TReal(1.73205080756887729353)*(g - b));
        
        if(max == r) // between yellow and magenta
            hue = (g - b) / chroma;
        else if(max == g) // between cyan and yellow
            hue = TReal(2.0) + (b - r) / chroma;
        else if(max == b) // between magenta and cyan
            hue = TReal(4.0) + (r - g) / chroma;
        
        if(hue < -std::numeric_limits<TReal>::epsilon())
            hue += TReal(6.0);
        
        hue /= TReal(6.0);
    }
    
//     hue = (r == max) ? (b-g) : (g == max) ? TReal(2.0)+(r-b) : TReal(4.0)+(g-r);
    
    return hue;
}

template<class TReal>
void getRGB(TReal& r, TReal& g, TReal& b, TReal hue) {
    // 0 red    -> (1,0,0)
    // 1 yellow -> (1,1,0)
    // 2 green  -> (0,1,0)
    // 3 cyan   -> (0,1,1)
    // 4 blue   -> (0,0,1)
    // 5 magenta -> (1,0,1)
    // 6 red    -> (1,0,0)
    
    if(hue < -std::numeric_limits<TReal>::epsilon()) {
        r = g = b = TReal(0.0);
        return;
    }
    
    int   i = int(hue * TReal(6.0));
    TReal x = hue * TReal(6.0) - i;
    TReal y = TReal(1.0) - x;
    
    switch(i % 6){
        case 0: { r=TReal(1.0), g=x         , b=TReal(0.0); } break;
        case 1: { r=y         , g=TReal(1.0), b=TReal(0.0); } break;
        case 2: { r=TReal(0.0), g=TReal(1.0), b=x         ; } break;
        case 3: { r=TReal(0.0), g=y         , b=TReal(1.0); } break;
        case 4: { r=x         , g=TReal(0.0), b=TReal(1.0); } break;
        case 5: { r=TReal(1.0), g=TReal(0.0), b=y         ; } break;
    }
}

template<class HSXType, class TReal>
inline static TReal getLightness(TReal r, TReal g, TReal b) {
    return HSXType::getLightness(r, g, b);
}

template<class HSXType, class TReal>
inline void ToneMapping(TReal& r, TReal& g, TReal& b)
{
    using namespace Arithmetic;


    TReal l = HSXType::getLightness(r, g, b);
    TReal n = min(r, g, b);
    TReal x = max(r, g, b);

    if(n < TReal(0.0)) {
        if (isZeroValueClampedFuzzy<float>(l)) {
            /**
             * The tonemapping method we use does **not** support lightness
             * values below 0.0, so we just clamp the value.
             *
             * TODO: use proper HSV/HSI shape-tracking clamping methods we use
             * in kis_hsv_adjustment.cpp
             */
            r = g = b = TReal(0.0);
        } else {
            const TReal stretch = l - n;

            if (stretch < std::numeric_limits<TReal>::epsilon()) {
                r = g = b = TReal(0.0);
            } else {
                TReal iln = TReal(1.0) / stretch;
                r = l + ((r-l) * l) * iln;
                g = l + ((g-l) * l) * iln;
                b = l + ((b-l) * l) * iln;
            }
        }
    }

    if(x > TReal(1.0)) {
        auto setFallbackValues = [&] () {
            if constexpr (HSXType::lightnessIsAverage) {
                r = g = b = TReal(1.0);
            } else {
                r = qMin(r, TReal(1.0));
                g = qMin(g, TReal(1.0));
                b = qMin(b, TReal(1.0));
            }
        };

        if (l > TReal(1.0)) {
            /**
             * The tonemapping method we use does **not** support lightness
             * values above 1.0, so we just clamp the value.
             *
             * TODO: use proper HSV/HSI shape-tracking clamping methods we use
             * in kis_hsv_adjustment.cpp
             */
            setFallbackValues();
        } else {
            const TReal stretch = x - l;

            if (stretch < std::numeric_limits<TReal>::epsilon()) {
                setFallbackValues();
            } else {
                TReal il  = TReal(1.0) - l;
                TReal ixl = TReal(1.0) / stretch;

                r = l + ((r-l) * il) * ixl;
                g = l + ((g-l) * il) * ixl;
                b = l + ((b-l) * il) * ixl;
            }
        }
    }
}

template<class HSXType, class TReal>
inline void addLightness(TReal& r, TReal& g, TReal& b, TReal light)
{
    using namespace Arithmetic;

    r += light;
    g += light;
    b += light;

    ToneMapping<HSXType, TReal>(r, g, b);
}

template<class HSXType, class TReal>
inline void setLightness(TReal& r, TReal& g, TReal& b, TReal light)
{
    addLightness<HSXType>(r,g,b, light - HSXType::getLightness(r,g,b));
}

template<class HSXType, class TReal>
inline static TReal getSaturation(TReal r, TReal g, TReal b) {
    return HSXType::getSaturation(r, g, b);
}

template<class HSXType, class TReal>
inline void setSaturation(TReal& r, TReal& g, TReal& b, TReal sat)
{
    int   min    = 0;
    int   mid    = 1;
    int   max    = 2;
    TReal rgb[3] = {r, g, b};
    
    if(rgb[mid] < rgb[min]) {
        int tmp = min;
        min = mid;
        mid = tmp;
    }
    
    if(rgb[max] < rgb[mid]) {
        int tmp = mid;
        mid = max;
        max = tmp;
    }
    
    if(rgb[mid] < rgb[min]) {
        int tmp = min;
        min = mid;
        mid = tmp;
    }
    
    if((rgb[max] - rgb[min]) > std::numeric_limits<TReal>::epsilon()) {
        rgb[mid] = ((rgb[mid]-rgb[min]) * sat) / (rgb[max]-rgb[min]);
        rgb[max] = sat;
        rgb[min] = TReal(0.0);
        
        r = rgb[0];
        g = rgb[1];
        b = rgb[2];
    }
    else r = g = b = TReal(0.0);
}

#endif
