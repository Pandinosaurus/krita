/*
 *  SPDX-FileCopyrightText: 2022 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoFontRegistry.h"
#include "FlakeDebug.h"
#include "KoCssTextUtils.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGlobalStatic>
#include <QMutex>
#include <QThread>
#include <QThreadStorage>
#include <QtGlobal>
#include <utility>

#include <optional>

#include <KoResourcePaths.h>
#include <kis_debug.h>

#include "KoFontLibraryResourceUtils.h"
#include "KoFFWWSConverter.h"
#include "KoFontChangeTracker.h"
#include <KisResourceLocator.h>
#include FT_TRUETYPE_TABLES_H


static unsigned int firstCharUcs4(const QStringView qsv)
{
    if (Q_UNLIKELY(qsv.isEmpty())) {
        return 0;
    }
    const QChar high = qsv.first();
    if (Q_LIKELY(!high.isSurrogate())) {
        return high.unicode();
    }
    if (Q_LIKELY(high.isHighSurrogate() && qsv.length() >= 2)) {
        const QChar low = qsv[1];
        if (Q_LIKELY(low.isLowSurrogate())) {
            return QChar::surrogateToUcs4(high, low);
        }
    }
    return QChar::ReplacementCharacter;
}

Q_GLOBAL_STATIC(KoFontRegistry, s_instance)

class Q_DECL_HIDDEN KoFontRegistry::Private
{
private:
    FcConfigSP m_config;
    QSharedPointer<KoFFWWSConverter> fontFamilyConverter;
    QSharedPointer<KoFontChangeTracker> changeTracker;

    struct ThreadData {
        FT_LibrarySP m_library;
        QHash<QString, FcPatternSP> m_patterns;
        QHash<QString, FcFontSetSP> m_fontSets;
        QHash<QString, FT_FaceSP> m_faces;
        QHash<QString, QVector<QPair<QString, int>>> m_suggestedFiles;

        ThreadData(FT_LibrarySP lib)
            : m_library(std::move(lib))
        {
        }
    };

    QThreadStorage<QSharedPointer<ThreadData>> m_data;

    void initialize()
    {
        if (!m_data.hasLocalData()) {
            FT_Library lib = nullptr;
            FT_Error error = FT_Init_FreeType(&lib);
            if (error) {
                errorFlake << "Error with initializing FreeType library:" << error << "Current thread:" << QThread::currentThread()
                           << "GUI thread:" << qApp->thread();
            } else {
                m_data.setLocalData(QSharedPointer<ThreadData>::create(lib));
            }
        }
    }

public:
    Private()
    {
        FcConfig *config = FcConfigCreate();
        KIS_ASSERT(config && "No Fontconfig support available");

        /**
         * This loads the fontconfig configured on the host Linux system.
         *
         * Theoretically, it can cause some version inconsistency problems if
         * the version of fontconfig on the host differs from the version
         * of fontconfig shipped with Krita. But we estimate such risks as
         * negligible.
         */
        if (qgetenv("FONTCONFIG_PATH").isEmpty()) {
            QDir appdir("/etc/fonts");
            if (QFile::exists(appdir.absoluteFilePath("fonts.conf"))) {
                qputenv("FONTCONFIG_PATH", QFile::encodeName(QDir::toNativeSeparators(appdir.absolutePath())));
            } else {
                // Otherwise use default, which is defined in src/fcinit.c , windows and macos
                // default locations *are* defined in fontconfig's meson build system.
                appdir = QDir(KoResourcePaths::getApplicationRoot() +"/etc/fonts");
                if (QFile::exists(appdir.absoluteFilePath("fonts.conf"))) {
                    qputenv("FONTCONFIG_PATH", QFile::encodeName(QDir::toNativeSeparators(appdir.absolutePath())));
                }
            }
        }
        if (!FcConfigParseAndLoad(config, nullptr, FcTrue)) {
            errorFlake << "Failed loading the Fontconfig configuration";
            config = FcConfigGetCurrent();
        } else {
            FcConfigSetCurrent(config);
        }
        m_config.reset(config);

        /// Setup the change tracker.
        FcStrList *list = FcConfigGetFontDirs(m_config.data());
        FcStrListFirst(list);
        FcChar8 *dirString = FcStrListNext(list);
        QString path = QString::fromUtf8(reinterpret_cast<char *>(dirString));
        QStringList paths;
        while (!path.isEmpty()) {
            paths.append(path);
            FcStrListNext(list);
            dirString = FcStrListNext(list);
            path = QString::fromUtf8(reinterpret_cast<char *>(dirString));
        }
        FcStrListDone(list);
        changeTracker.reset(new KoFontChangeTracker(paths));
        changeTracker->resetChangeTracker();

        reloadConverter();
    }

    ~Private() = default;

    FT_LibrarySP library()
    {
        if (!m_data.hasLocalData())
            initialize();
        return m_data.localData()->m_library;
    }

    QHash<QString, FcPatternSP> &patterns()
    {
        if (!m_data.hasLocalData())
            initialize();
        return m_data.localData()->m_patterns;
    }

    QHash<QString, FcFontSetSP> &sets()
    {
        if (!m_data.hasLocalData())
            initialize();
        return m_data.localData()->m_fontSets;
    }

    QHash<QString, FT_FaceSP> &typeFaces()
    {
        if (!m_data.hasLocalData())
            initialize();
        return m_data.localData()->m_faces;
    }

    FcConfigSP config() const
    {
        return m_config;
    }

    QSharedPointer<KoFFWWSConverter> converter() const {
        return fontFamilyConverter;
    }

    QSharedPointer<KoFontChangeTracker> fontChangeTracker() const {
        return changeTracker;
    }

    bool reloadConverter() {
        fontFamilyConverter.reset(new KoFFWWSConverter());
        FcObjectSet *objectSet = FcObjectSetBuild(FC_FAMILY, FC_FILE, FC_INDEX, FC_LANG, FC_CHARSET, nullptr);
        FcFontSetSP allFonts(FcFontList(m_config.data(), FcPatternCreate(), objectSet));

        for (int j = 0; j < allFonts->nfont; j++) {
            fontFamilyConverter->addFontFromPattern(allFonts->fonts[j], library());
        }
        fontFamilyConverter->sortIntoWWSFamilies();
        return true;
    }

    QHash<QString, QVector<QPair<QString, int>>> &suggestedFileNames()
    {
        if (!m_data.hasLocalData())
            initialize();
        return m_data.localData()->m_suggestedFiles;
    }

    void debugConverter() {
        fontFamilyConverter->debugInfo();
    }

    void updateConfig() {
        if (FcConfigBuildFonts(m_config.data())) {
            reloadConverter();
            KisResourceLocator::instance()->updateFontStorage();
            changeTracker->resetChangeTracker();
        }
    }
};

KoFontRegistry::KoFontRegistry(QObject *parent)
    : QObject(parent)
    , d(new Private())
{
    connect(d->fontChangeTracker().data(), SIGNAL(sigUpdateConfig()), this, SLOT(updateConfig()), Qt::UniqueConnection);
}

KoFontRegistry::~KoFontRegistry() {
}

KoFontRegistry *KoFontRegistry::instance()
{
    return s_instance;
}

std::vector<FT_FaceSP> KoFontRegistry::facesForCSSValues(const QStringList &families,
                                                         QVector<int> &lengths,
                                                         const QMap<QString, qreal> &axisSettings,
                                                         const QString &text,
                                                         quint32 xRes,
                                                         quint32 yRes,
                                                         qreal size,
                                                         qreal fontSizeAdjust,
                                                         int weight,
                                                         int width,
                                                         int slantMode,
                                                         int slantValue, bool disableFontMatching,
                                                         const QString &language)
{
    // Generate a hash for the cache.
    // Because FT_faces cannot be cloned, we need to include the sizes and font variation modifications.
    QString modifications;
    if (size > -1) {
        modifications += QString::number(size) + ":" + QString::number(xRes) + "x" + QString::number(yRes);
    }
    if (fontSizeAdjust != 1.0) {
        modifications += QString::number(fontSizeAdjust);
    }
    if (weight != 400) {
        modifications += "weight:"+QString::number(weight);
    }
    if (width != 100) {
        modifications += "width:"+QString::number(width);
    }
    if (slantMode != 0) {
        modifications += "slantMode:"+QString::number(slantMode);
        if (slantValue != 0) {
            modifications += "val:"+QString::number(slantValue);
        }
    }
    if (!axisSettings.isEmpty()) {
        Q_FOREACH (const QString &key, axisSettings.keys()) {
            modifications += "|" + key + QString::number(axisSettings.value(key));
        }
    }

    QVector<QPair<QString, int>> candidates;
    const QString suggestedHash = families.join("+") + ":" + QString::number(slantMode) + ":" + modifications;
    auto entry = d->suggestedFileNames().find(suggestedHash);
    if (entry != d->suggestedFileNames().end()) {
        candidates = entry.value();
    } else {
        candidates = d->converter()->candidatesForCssValues(families,
                                                            axisSettings,
                                                            xRes, yRes, size,
                                                            weight, width,
                                                            slantMode, slantValue);
        d->suggestedFileNames().insert(suggestedHash, candidates);
    }

    struct FontEntry {
        QString fileName;
        int fontIndex;


        static std::optional<FontEntry> get(const FcPattern *p) {

            FcChar8 *fileValue{};
            if (FcPatternGetString(p, FC_FILE, 0, &fileValue) != FcResultMatch) {
                debugFlake << "Failed to get font file for" << p;
                return {};
            }
            const QString fontFileName = QString::fromUtf8(reinterpret_cast<char *>(fileValue));

            int indexValue{};
            if (FcPatternGetInteger(p, FC_INDEX, 0, &indexValue) != FcResultMatch) {
                debugFlake << "Failed to get font index for" << p << "(file:" << fontFileName << ")";
                return {};
            }

            return {{fontFileName, indexValue}};
        }
    };

    QVector<FontEntry> fonts;
    lengths.clear();

    if (disableFontMatching && !candidates.isEmpty()) {
        FontEntry entry;
        entry.fileName = candidates.first().first;
        entry.fontIndex = candidates.first().second;
        fonts.append(entry);
        lengths.append(text.size());
    } else {

        FcPatternSP p(FcPatternCreate());
        Q_FOREACH (const QString &family, families) {
            QByteArray utfData = family.toUtf8();
            const FcChar8 *vals = reinterpret_cast<FcChar8 *>(utfData.data());
            FcPatternAddString(p.data(), FC_FAMILY, vals);
        }

        {
            QByteArray fallbackBuf = QString("sans-serif").toUtf8();
            FcValue fallback;
            fallback.type = FcTypeString;
            fallback.u.s = reinterpret_cast<FcChar8 *>(fallbackBuf.data());
            FcPatternAddWeak(p.data(), FC_FAMILY, fallback, true);
        }

        for (int i = 0; i < candidates.size(); i++) {
            const QPair<QString, int> file = candidates.at(i);
            QByteArray utfData = file.first.toUtf8();
            const FcChar8 *vals = reinterpret_cast<FcChar8 *>(utfData.data());
            FcPatternAddString(p.data(), FC_FILE, vals);
            FcPatternAddInteger(p.data(), FC_INDEX, file.second);
        }

        if (slantMode == 1) {
            FcPatternAddInteger(p.data(), FC_SLANT, FC_SLANT_ITALIC);
        } else if (slantMode == 2) {
            FcPatternAddInteger(p.data(), FC_SLANT, FC_SLANT_OBLIQUE);
        } else {
            FcPatternAddInteger(p.data(), FC_SLANT, FC_SLANT_ROMAN);
        }
        FcPatternAddInteger(p.data(), FC_WEIGHT, FcWeightFromOpenType(weight));
        FcPatternAddInteger(p.data(), FC_WIDTH, width);

        double pixelSize = size*(qMin(xRes, yRes)/72.0);
        FcPatternAddDouble(p.data(), FC_PIXEL_SIZE, pixelSize);

        FcConfigSubstitute(nullptr, p.data(), FcMatchPattern);
        FcDefaultSubstitute(p.data());


        p = [&]() {
            const FcChar32 hash = FcPatternHash(p.data());
            QString patternHash = families.join("+")+QString::number(hash);
            // FCPatternHash breaks down when there's mutliple family names that start
            // with the same letter and are the same length (Butcherman and Babylonica, Eater and Elsie, all 4 on google fonts).
            const auto oldPattern = d->patterns().find(patternHash);
            if (oldPattern != d->patterns().end()) {
                return oldPattern.value();
            } else {
                d->patterns().insert(patternHash, p);
                return p;
            }
        }();

        FcResult result = FcResultNoMatch;
        FcCharSetSP charSet;
        FcFontSetSP fontSet = [&]() -> FcFontSetSP {
            const FcChar32 hash = FcPatternHash(p.data());
            QString patternHash = families.join("+")+QString::number(hash);
            const auto set = d->sets().find(patternHash);

            if (set != d->sets().end()) {
                return set.value();
            } else {
                FcCharSet *cs = nullptr;
                FcFontSetSP avalue(FcFontSort(FcConfigGetCurrent(), p.data(), FcFalse, &cs, &result));
                charSet.reset(cs);
                d->sets().insert(patternHash, avalue);
                return avalue;
            }
        }();

        if (text.isEmpty()) {
            for (int j = 0; j < fontSet->nfont; j++) {
                if (std::optional<FontEntry> font = FontEntry::get(fontSet->fonts[j])) {
                    fonts.append(std::move(*font));
                    lengths.append(0);
                    break;
                }
            }
        } else {
            FcCharSet *set = nullptr;
            QVector<int> familyValues(text.size());
            QVector<int> fallbackMatchValues(text.size());
            familyValues.fill(-1);
            fallbackMatchValues.fill(-1);

            // First, we're going to split up the text into graphemes. This is both
            // because the css spec requires it, but also because of why the css
            // spec requires it: graphemes' parts should not end up in seperate
            // runs, which they will if they get assigned different fonts,
            // potentially breaking ligatures and emoji sequences.
            QStringList graphemes = KoCssTextUtils::textToUnicodeGraphemeClusters(text, language);

            // Parse over the fonts and graphemes and try to see if we can get the
            // best match for a given grapheme.
            for (int i = 0; i < fontSet->nfont; i++) {

                double fontsize = 0.0;
                FcBool isScalable = false;
                FcPatternGetBool(fontSet->fonts[i], FC_SCALABLE, 0, &isScalable);
                FcPatternGetDouble(fontSet->fonts[i], FC_PIXEL_SIZE, 0, &fontsize);
                if (!isScalable && pixelSize != fontsize) {
                    // For some reason, FC will sometimes consider a smaller font pixel-size
                    // to be more relevant to the requested pattern than a bigger one. This
                    // skips those fonts, but it does mean that such pixel fonts would not
                    // be used for fallback.
                    continue;
                }
                if (FcPatternGetCharSet(fontSet->fonts[i], FC_CHARSET, 0, &set) == FcResultMatch) {
                    int index = 0;
                    Q_FOREACH (const QString &grapheme, graphemes) {

                        // Don't worry about matching controls directly,
                        // as they are not important to font-selection (and many
                        // fonts have no glyph entry for these)
                        if (const uint first = firstCharUcs4(grapheme); QChar::category(first) == QChar::Other_Control
                                || QChar::category(first) == QChar::Other_Format) {
                            index += grapheme.size();
                            continue;
                        }
                        int familyIndex = -1;
                        if (familyValues.at(index) == -1) {
                            int fallbackMatch = fallbackMatchValues.at(index);
                            Q_FOREACH (uint unicode, grapheme.toUcs4()) {
                                if (FcCharSetHasChar(set, unicode)) {
                                    familyIndex = i;
                                    if (fallbackMatch < 0) {
                                        fallbackMatch = i;
                                    }
                                } else {
                                    familyIndex = -1;
                                    break;
                                }
                            }
                            for (int k = 0; k < grapheme.size(); k++) {
                                familyValues[index + k] = familyIndex;
                                fallbackMatchValues[index + k] = fallbackMatch;
                            }
                        }
                        index += grapheme.size();
                    }
                    if (!familyValues.contains(-1)) {
                        break;
                    }
                }
            }

            // Remove the -1 entries.
            if (familyValues.contains(-1)) {
                int value = -1;
                Q_FOREACH (const int currentValue, familyValues) {
                    if (currentValue != value) {
                        value = currentValue;
                        break;
                    }
                }
                value = qMax(0, value);
                for (int i = 0; i < familyValues.size(); i++) {
                    if (familyValues.at(i) < 0) {
                        if (fallbackMatchValues.at(i) < 0) {
                            familyValues[i] = value;
                        } else {
                            familyValues[i] = fallbackMatchValues.at(i);
                        }
                    } else {
                        value = familyValues.at(i);
                    }
                }
            }


            // Get the filenames and lengths for the entries.
            int length = 0;
            int startIndex = 0;
            int lastIndex = familyValues.at(0);
            FontEntry font{};
            if (std::optional<FontEntry> f = FontEntry::get(fontSet->fonts[lastIndex])) {
                font = std::move(*f);
            }
            for (int i = 0; i < familyValues.size(); i++) {
                if (lastIndex != familyValues.at(i)) {
                    lengths.append(text.mid(startIndex, length).size());
                    fonts.append(font);
                    startIndex = i;
                    length = 0;
                    lastIndex = familyValues.at(i);
                    if (std::optional<FontEntry> f = FontEntry::get(fontSet->fonts[lastIndex])) {
                        font = std::move(*f);
                    }
                }
                length += 1;
            }
            if (length > 0) {
                lengths.append(text.mid(startIndex, length).size());
                fonts.append(font);
            }
        }
    }

    std::vector<FT_FaceSP> faces;

    for (int i = 0; i < lengths.size(); i++) {
        FontEntry font = fonts.at(i);
        // For some reason, FontConfig will sometimes return the wrong file index.
        // We can in this case select the appropriate index, which should work as tcc collections share glyphs.
        for (int j = 0; j < candidates.size(); j++) {
            if (font.fileName == candidates.at(j).first) {
                font.fontIndex = candidates.at(j).second;
                break;
            }
        }

        const QString fontCacheEntry = font.fileName + "#" + QString::number(font.fontIndex) + "#" + modifications;
        auto entry = d->typeFaces().find(fontCacheEntry);
        if (entry != d->typeFaces().end()) {
            faces.emplace_back(entry.value());
        } else {
            FT_Face f = nullptr;
            QByteArray utfData = font.fileName.toUtf8();
            if (FT_New_Face(d->library().data(), utfData.data(), font.fontIndex, &f) == 0) {
                FT_FaceSP face(f);
                configureFaces({face}, size, fontSizeAdjust, xRes, yRes, axisSettings);
                faces.emplace_back(face);
                d->typeFaces().insert(fontCacheEntry, face);
            }
        }
    }

    return faces;
}

bool KoFontRegistry::configureFaces(const std::vector<FT_FaceSP> &faces,
                                    qreal size,
                                    qreal fontSizeAdjust,
                                    quint32 xRes,
                                    quint32 yRes,
                                    const QMap<QString, qreal> &axisSettings)
{
    int errorCode = 0;
    const qreal ftFontUnit = 64.0;
    const qreal finalRes = qMin(xRes, yRes);
    const qreal scaleToPixel = finalRes / 72;
    Q_FOREACH (const FT_FaceSP &face, faces) {

        // set the charmap, by default Freetype will select a Unicode charmap, but when there's none, it doesn't set any and we need to select one manually.
        if (!face->charmap) {
            FT_CharMap map = nullptr;
            for (int i = 0; i < face->num_charmaps; i++) {
                FT_CharMap m = face->charmaps[i];
                if (!m) {
                    continue;
                }

                if (!map) {
                    map = m;
                    continue;
                }

                switch (m->encoding) {
                case FT_ENCODING_UNICODE:
                    map = m;
                    break;
                case FT_ENCODING_APPLE_ROMAN:
                case FT_ENCODING_ADOBE_LATIN_1:
                    if (!map || map->encoding != FT_ENCODING_UNICODE) {
                        map = m;
                    }
                    break;
                case FT_ENCODING_ADOBE_CUSTOM:
                case FT_ENCODING_MS_SYMBOL:
                    if (!map || (map->encoding != FT_ENCODING_UNICODE && map->encoding != FT_ENCODING_APPLE_ROMAN && map->encoding != FT_ENCODING_ADOBE_LATIN_1)) {
                        map = m;
                    }
                default:
                    break;
                }
            }
            FT_Set_Charmap(face.data(), map);
        }

        if (!FT_IS_SCALABLE(face)) {
            const qreal fontSizePixels = size * ftFontUnit * scaleToPixel;
            qreal sizeDelta = 0;
            int selectedIndex = -1;

            for (int i = 0; i < face->num_fixed_sizes; i++) {
                const qreal newDelta = qAbs((fontSizePixels)-face->available_sizes[i].x_ppem);
                if (newDelta < sizeDelta || i == 0) {
                    selectedIndex = i;
                    sizeDelta = newDelta;
                }
            }

            if (selectedIndex >= 0) {
                if (FT_HAS_COLOR(face)) {
                    const FT_Fixed scale = static_cast<FT_Fixed>(65535. * qreal(fontSizePixels)
                                                                 / qreal(face->available_sizes[selectedIndex].x_ppem));
                    FT_Matrix matrix;
                    matrix.xx = scale;
                    matrix.xy = 0;
                    matrix.yx = 0;
                    matrix.yy = scale;
                    FT_Vector v;
                    FT_Set_Transform(face.data(), &matrix, &v);
                }
                errorCode = FT_Select_Size(face.data(), selectedIndex);
            }
        } else {
            errorCode = FT_Set_Char_Size(face.data(), static_cast<FT_F26Dot6>(size * ftFontUnit), 0, xRes, yRes);
            hb_font_t_sp font(hb_ft_font_create_referenced(face.data()));
            hb_position_t xHeight = 0;
            hb_ot_metrics_get_position(font.data(), HB_OT_METRICS_TAG_X_HEIGHT, &xHeight);
            if (xHeight > 0 && fontSizeAdjust > 0 && fontSizeAdjust < 1.0) {
                qreal aspect = xHeight / (size * ftFontUnit * scaleToPixel);
                errorCode = FT_Set_Char_Size(face.data(),
                                             static_cast<FT_F26Dot6>((fontSizeAdjust / aspect) * (size * ftFontUnit)),
                                             0,
                                             xRes,
                                             yRes);
            }
        }

        QMap<FT_Tag, qreal> tags;
        Q_FOREACH (const QString &tagName, axisSettings.keys()) {
            if (tagName.size() == 4) {
                const QByteArray utfData = tagName.toUtf8();
                const char *tag = utfData.data();
                tags.insert(FT_MAKE_TAG(tag[0], tag[1], tag[2], tag[3]), axisSettings.value(tagName));
            }
        }
        const FT_Tag ITALIC_TAG = FT_MAKE_TAG('i', 't', 'a', 'l');
        const FT_Tag SLANT_TAG = FT_MAKE_TAG('s', 'l', 'n', 't');
        const int axisMultiplier = 65535;
        if (FT_HAS_MULTIPLE_MASTERS(face)) {
            FT_MM_Var *amaster = nullptr;
            FT_Get_MM_Var(face.data(), &amaster);
            // note: this only works for opentype, as it uses
            // tag-based-selection.
            std::vector<FT_Fixed> designCoords(amaster->num_axis);
            for (FT_UInt i = 0; i < amaster->num_axis; i++) {
                FT_Var_Axis axis = amaster->axis[i];
                designCoords[i] = axis.def;
                if (tags.contains(axis.tag)) {
                    designCoords[i] = qBound(axis.minimum, long(tags.value(axis.tag) * axisMultiplier), axis.maximum);
                } else if (axis.tag == SLANT_TAG
                           && !tags.contains(SLANT_TAG)
                           && tags.value(ITALIC_TAG, 0) > 0) {
                    designCoords[i] = qBound(axis.minimum, long(-11.0 * axisMultiplier), axis.maximum);
                } else if (axis.tag == ITALIC_TAG
                           && !tags.contains(ITALIC_TAG)
                           && !qFuzzyCompare(tags.value(SLANT_TAG, 0), 0)) {
                    designCoords[i] = qBound(axis.minimum, long(1.0 * axisMultiplier), axis.maximum);
                }
            }
            FT_Set_Var_Design_Coordinates(face.data(), amaster->num_axis, designCoords.data());
            FT_Done_MM_Var(d->library().data(), amaster);
        }
    }
    return (errorCode == 0);
}

QList<KoFontFamilyWWSRepresentation> KoFontRegistry::collectRepresentations() const
{
    return d->converter()->collectFamilies();
}

std::optional<KoFontFamilyWWSRepresentation> KoFontRegistry::representationByFamilyName(const QString &familyName) const
{
    return d->converter()->representationByFamilyName(familyName);
}

std::optional<QString> KoFontRegistry::wwsNameByFamilyName(const QString familyName) const
{
    return d->converter()->wwsNameByFamilyName(familyName);
}

void KoFontRegistry::updateConfig()
{
    d->updateConfig();
}

QFont::Style KoFontRegistry::slantMode(FT_FaceSP face)
{
    constexpr unsigned OS2_ITALIC = 1u << 0; /// Is italic
    constexpr unsigned OS2_REGULAR = 1u << 6; /// Is truly regular (instead of italic or oblique)
    constexpr unsigned OS2_OBLIQUE = 1u << 9; // Is an oblique instead of an italic.

    if (FT_IS_SFNT(face.data())) {
        hb_font_t_sp hbFont(hb_ft_font_create_referenced(face.data()));
        bool isItalic = hb_style_get_value(hbFont.data(), HB_STYLE_TAG_ITALIC) > 0;
        bool isOblique = false;

        if (FT_HAS_MULTIPLE_MASTERS(face)) {
            isOblique = hb_style_get_value(hbFont.data(), HB_STYLE_TAG_SLANT_ANGLE) != 0;
        } else {
            TT_OS2 *os2Table = nullptr;
            os2Table = (TT_OS2*)FT_Get_Sfnt_Table(face.data(), FT_SFNT_OS2);
            if (os2Table) {
                isItalic = os2Table->fsSelection & OS2_ITALIC;
                isOblique = os2Table->fsSelection & OS2_OBLIQUE;
            }
            if (os2Table->fsSelection & OS2_REGULAR) {
                isItalic = false;
                isOblique = false;
            }
        }
        return isItalic? isOblique? QFont::StyleOblique: QFont::StyleItalic: QFont::StyleNormal;
    } else {
        return face->style_flags & FT_STYLE_FLAG_ITALIC? QFont::StyleItalic: QFont::StyleNormal;
    }
}

bool KoFontRegistry::addFontFilePathToRegistery(const QString &path)
{
    const QByteArray utfData = path.toUtf8();
    const FcChar8 *vals = reinterpret_cast<const FcChar8 *>(utfData.data());
    bool success = false;
    if (FcConfigAppFontAddFile(d->config().data(), vals)) {
        success = d->reloadConverter();
    }
    return success;
}

bool KoFontRegistry::addFontFileDirectoryToRegistery(const QString &path)
{
    const QByteArray utfData = path.toUtf8();
    const FcChar8 *vals = reinterpret_cast<const FcChar8 *>(utfData.data());
    bool success = false;
    if (FcConfigAppFontAddDir(d->config().data(), vals)) {
        success = d->reloadConverter();
    }
    return success;
}
