/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_POINTER_UTILS_H
#define KIS_POINTER_UTILS_H

#include <QSharedPointer>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QForeach>
#endif

/**
 * Convert a raw pointer into a shared pointer
 */
template <class T>
inline QSharedPointer<T> toQShared(T* ptr) {
    return QSharedPointer<T>(ptr);
}

/**
 * Convert a list of raw pointers into a list of shared pointers
 */
template <class A, template <class C> class List>
List<QSharedPointer<A>> listToQShared(const List<A*> list) {
    List<QSharedPointer<A>> newList;
    Q_FOREACH(A* value, list) {
        newList.append(toQShared(value));
    }
    return newList;
}


/**
 * Convert a list of strong pointers into a list of weak pointers
 */
template <template <class> class Container, class T>
Container<QWeakPointer<T>> listStrongToWeak(const Container<QSharedPointer<T>> &container)
{
    Container<QWeakPointer<T> > result;
    Q_FOREACH (QSharedPointer<T> v, container) {
        result << v;
    }
    return result;
}

/**
 * Convert a list of weak pointers into a list of strong pointers
 *
 * WARNING: By default, uses "all or nothing" rule. If at least one of
 *          the weak pointers is invalid, returns an *empty* list!
 *          Even though some other pointer can still be converted
 *          correctly.
 */
template <template <class> class Container, class T>
    Container<QSharedPointer<T> > listWeakToStrong(const Container<QWeakPointer<T>> &container,
                                                   bool allOrNothing = true)
{
    Container<QSharedPointer<T> > result;
    Q_FOREACH (QWeakPointer<T> v, container) {
        QSharedPointer<T> strong(v);
        if (!strong && allOrNothing) {
            result.clear();
            return result;
        }

        if (strong) {
            result << strong;
        }
    }
    return result;
}

/**
 * Converts a list of objects with type T into a list of objects of type R.
 * The conversion is done implicitly, therefore the c-tor of type R should
 * support it. The main usage case is conversion of pointers in "descendant-
 * to-parent" way.
 */
template <typename R, typename T, template <typename U> class Container>
inline Container<R> implicitCastList(const Container<T> &list)
{
    Container<R> result;

    Q_FOREACH(const T &item, list) {
        result.append(item);
    }
    return result;
}


template<class T>
class KisWeakSharedPtr;
template<class T>
class KisSharedPtr;
template<class T>
class KisPinnedSharedPtr;

/**
 * \fn removeSharedPointer
 *
 * A template function for converting any kind of a shared pointer into a
 * raw pointer.
 */

template <typename T>
T* removeSharedPointer(T* value)
{
    return value;
}

template <typename T>
T* removeSharedPointer(KisPinnedSharedPtr<T> value)
{
    return value.data();
}

template <typename T>
T* removeSharedPointer(KisSharedPtr<T> value)
{
    return value.data();
}

template <typename T>
T* removeSharedPointer(QSharedPointer<T> value)
{
    return value.data();
}


template <typename T>
struct KisSharedPointerTraits
{
};

template <typename T>
struct KisSharedPointerTraits<QSharedPointer<T>>
{
    template <typename U>
    using SharedPointerType = QSharedPointer<U>;
    using ValueType = T;

    template <typename D, typename S>
    static inline QSharedPointer<D> dynamicCastSP(QSharedPointer<S> src) {
        return src.template dynamicCast<D>();
    }
};

template <typename T>
struct KisSharedPointerTraits<KisSharedPtr<T>>
{
    template <typename U>
    using SharedPointerType = KisSharedPtr<U>;
    using ValueType = T;

    template <typename D, typename S>
    static inline KisSharedPtr<D> dynamicCastSP(KisSharedPtr<S> src) {
        return KisSharedPtr<D>(dynamic_cast<D*>(src.data()));
    }
};

template <typename T>
struct KisSharedPointerTraits<KisPinnedSharedPtr<T>>
{
    template <typename U>
    using SharedPointerType = KisPinnedSharedPtr<U>;
    using ValueType = T;

    template <typename D, typename S>
    static inline KisPinnedSharedPtr<D> dynamicCastSP(KisPinnedSharedPtr<S> src) {
        return KisPinnedSharedPtr<D>(dynamic_cast<D*>(src.data()));
    }
};

#endif // KIS_POINTER_UTILS_H

