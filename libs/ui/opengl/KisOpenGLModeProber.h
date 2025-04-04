/*
 *  SPDX-FileCopyrightText: 2017 Alvin Wong <alvinhochun@gmail.com>
 *  SPDX-FileCopyrightText: 2019 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2023 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISOPENGLMODEPROBER_H
#define KISOPENGLMODEPROBER_H

#include "kritaui_export.h"
#include "kis_config.h"
#include <QSurfaceFormat>
#include <boost/optional.hpp>
#include "kis_opengl.h"

class KoColorProfile;
class KisSurfaceColorSpaceWrapper;

class KRITAUI_EXPORT KisOpenGLModeProber
{
public:
    class Result;

public:
    KisOpenGLModeProber();
    ~KisOpenGLModeProber();

    static KisOpenGLModeProber* instance();

    bool useHDRMode() const;
    QSurfaceFormat surfaceformatInUse() const;

    const KoColorProfile *rootSurfaceColorProfile() const;

    boost::optional<Result> probeFormat(const KisOpenGL::RendererConfig &rendererConfig,
                                        bool adjustGlobalState = true);
    static bool fuzzyCompareColorSpaces(const KisSurfaceColorSpaceWrapper &lhs,
                                        const KisSurfaceColorSpaceWrapper &rhs);
    static QString angleRendererToString(KisOpenGL::AngleRenderer renderer);

public:
    static void initSurfaceFormatFromConfig(KisConfig::RootSurfaceFormat config,
                                            QSurfaceFormat *format);
    static bool isFormatHDR(const QSurfaceFormat &format);
};

class KisOpenGLModeProber::Result {
public:
    Result(QOpenGLContext &context);

    int glMajorVersion() const {
        return m_glMajorVersion;
    }

    int glMinorVersion() const {
        return m_glMinorVersion;
    }

    bool supportsDeprecatedFunctions() const {
        return m_supportsDeprecatedFunctions;
    }

    bool isOpenGLES() const {
        return m_isOpenGLES;
    }

    QString rendererString() const {
        return m_rendererString;
    }

    QString driverVersionString() const {
        return m_driverVersionString;
    }

    bool isSupportedVersion() const {
        // Technically we could support GLES2 (I added the extensions for
        // floating point surfaces). But given that Dmitry required VAOs
        // on GLES, I've kept the check mostly as-is.
        // Note:
        //  - we've not supported OpenGL 2.1 for a Long time (commit e0d9a4feba0d4b5e70dddece8b76f38a6043fc88)
        return (m_isOpenGLES && m_glMajorVersion >= 3) || ((m_glMajorVersion * 100 + m_glMinorVersion) >= 303);
    }

    bool supportsLoD() const {
        return m_supportsLod;
    }

    bool supportsVAO() const {
        /**
         * Theoretically, we could also test for ARB_vertex_array_object on
         * openGL and OES_vertex_array_object on openGLES and enable this
         * feature for openGL 2.1 as well. But we have no hardware to test if
         * it really works in our code (our VAO code also uses buffers
         * extensively), so we limit this feature to openGL/GLES 3.0 only.
         *
         * Feel free to test it on the relevant hardware and enable it if
         * needed.
         */
        return (m_glMajorVersion * 100 + m_glMinorVersion) >= 300;
    }

    bool hasOpenGL3() const {
        return (m_glMajorVersion * 100 + m_glMinorVersion) >= 302;
    }

    bool supportsFenceSync() const {
        return m_glMajorVersion >= 3;
    }

    bool supportsFBO() const {
        return m_supportsFBO;
    }

    bool supportsBufferMapping() const {
        return m_supportsBufferMapping;
    }

    bool supportsBufferInvalidation() const {
        return m_supportsBufferInvalidation;
    }

#ifdef Q_OS_WIN
    // This is only for detecting whether ANGLE is being used.
    // For detecting generic OpenGL ES please check isOpenGLES
    bool isUsingAngle() const {
        return m_rendererString.startsWith("ANGLE", Qt::CaseInsensitive);
    }
#endif

    QString shadingLanguageString() const
    {
        return m_shadingLanguageString;
    }

    QString vendorString() const
    {
        return m_vendorString;
    }

    QSurfaceFormat format() const
    {
        return m_format;
    }

    QSet<QByteArray> extensions() const
    {
        return m_extensions;
    }

private:
    int m_glMajorVersion = 0;
    int m_glMinorVersion = 0;
    bool m_supportsDeprecatedFunctions = false;
    bool m_isOpenGLES = false;
    bool m_supportsFBO = false;
    bool m_supportsBufferMapping = false;
    bool m_supportsBufferInvalidation = false;
    bool m_supportsLod = false;
    QString m_rendererString;
    QString m_driverVersionString;
    QString m_vendorString;
    QString m_shadingLanguageString;
    QSurfaceFormat m_format;
    QSet<QByteArray> m_extensions;
};

#endif // KISOPENGLMODEPROBER_H
