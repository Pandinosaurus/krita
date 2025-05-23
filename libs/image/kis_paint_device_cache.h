/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_PAINT_DEVICE_CACHE_H
#define __KIS_PAINT_DEVICE_CACHE_H

#include "kis_lock_free_cache.h"
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>

class KisPaintDeviceCache
{
public:
    KisPaintDeviceCache(KisPaintDevice *paintDevice)
        : m_paintDevice(paintDevice),
          m_exactBoundsCache(paintDevice),
          m_nonDefaultPixelAreaCache(paintDevice),
          m_regionCache(paintDevice),
          m_sequenceNumber(0)
    {
    }

    KisPaintDeviceCache(const KisPaintDeviceCache &rhs)
        : m_paintDevice(rhs.m_paintDevice),
          m_exactBoundsCache(rhs.m_paintDevice),
          m_nonDefaultPixelAreaCache(rhs.m_paintDevice),
          m_regionCache(rhs.m_paintDevice),
          m_sequenceNumber(0)
    {
    }

    void setupCache() {
        invalidate();
    }

    void invalidate() {
        m_thumbnailsValid = false;
        m_exactBoundsCache.invalidate();
        m_nonDefaultPixelAreaCache.invalidate();
        m_regionCache.invalidate();
        m_sequenceNumber++;
    }

    QRect exactBounds() {
        return m_exactBoundsCache.getValue(m_paintDevice->defaultBounds()->wrapAroundMode());
    }

    QRect exactBoundsAmortized() {
        QRect bounds;
        bool result = m_exactBoundsCache.tryGetValue(bounds, m_paintDevice->defaultBounds()->wrapAroundMode());

        if (!result) {
            /**
             * The calculation of the exact bounds might be too slow
             * in some special cases, e.g. for an empty canvas of 7k
             * by 6k.  So we just always return extent, when the exact
             * bounds is not available.
             */
            bounds = m_paintDevice->extent();
        }

        return bounds;
    }

    QRect nonDefaultPixelArea() {
        return m_nonDefaultPixelAreaCache.getValue(m_paintDevice->defaultBounds()->wrapAroundMode());
    }

    KisRegion region() {
        return m_regionCache.getValue(m_paintDevice->defaultBounds()->wrapAroundMode());
    }

    QImage createThumbnail(qint32 w, qint32 h, qreal oversample, KoColorConversionTransformation::Intent renderingIntent, KoColorConversionTransformation::ConversionFlags conversionFlags) {
        QImage thumbnail;

        if (h == 0 || w == 0) {
            return thumbnail;
        }

        {
            QReadLocker readLocker(&m_thumbnailsLock);
            if (m_thumbnailsValid) {
                if (m_thumbnails.contains(w) && m_thumbnails[w].contains(h) && m_thumbnails[w][h].contains(oversample)) {
                    thumbnail = m_thumbnails[w][h][oversample];
                }
            }
            else {
                readLocker.unlock();
                QWriteLocker writeLocker(&m_thumbnailsLock);
                m_thumbnails.clear();
                m_thumbnailsValid = true;
            }
        }

        if (thumbnail.isNull()) {
            // the thumbnails in the cache are always generated from exact bounds
            thumbnail = m_paintDevice->createThumbnail(w, h, m_paintDevice->exactBounds(), oversample, renderingIntent, conversionFlags);

            QWriteLocker writeLocker(&m_thumbnailsLock);
            m_thumbnails[w][h][oversample] = thumbnail;
            m_thumbnailsValid = true;
        }

        return thumbnail;
    }

    int sequenceNumber() const {
        return m_sequenceNumber;
    }

private:
    KisPaintDevice *m_paintDevice {nullptr};

    struct ExactBoundsCache : KisLockFreeCacheWithModeConsistency<QRect, bool> {
        ExactBoundsCache(KisPaintDevice *paintDevice) : m_paintDevice(paintDevice) {}

        QRect calculateNewValue() const override {
            return m_paintDevice->calculateExactBounds(false);
        }
    private:
        KisPaintDevice *m_paintDevice;
    };

    struct NonDefaultPixelCache : KisLockFreeCacheWithModeConsistency<QRect, bool> {
        NonDefaultPixelCache(KisPaintDevice *paintDevice) : m_paintDevice(paintDevice) {}

        QRect calculateNewValue() const override {
            return m_paintDevice->calculateExactBounds(true);
        }
    private:
        KisPaintDevice *m_paintDevice;
    };

    struct RegionCache : KisLockFreeCacheWithModeConsistency<KisRegion, bool> {
        RegionCache(KisPaintDevice *paintDevice) : m_paintDevice(paintDevice) {}

        KisRegion calculateNewValue() const override {
            return m_paintDevice->dataManager()->region();
        }
    private:
        KisPaintDevice *m_paintDevice;
    };

    ExactBoundsCache m_exactBoundsCache;
    NonDefaultPixelCache m_nonDefaultPixelAreaCache;
    RegionCache m_regionCache;

    QReadWriteLock m_thumbnailsLock;
    bool m_thumbnailsValid {false};
    QMap<int, QMap<int, QMap<qreal,QImage> > > m_thumbnails;

    QAtomicInt m_sequenceNumber;
};

#endif /* __KIS_PAINT_DEVICE_CACHE_H */
