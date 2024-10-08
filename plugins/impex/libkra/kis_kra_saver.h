/* This file is part of the KDE project
 * Copyright 2008 (C) Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KIS_KRA_SAVER
#define KIS_KRA_SAVER

#include <kis_types.h>

#include <QDomDocument>
#include <QDomElement>
#include <QStringList>
#include <QString>

class KisDocument;
class KoStore;


#include "kritalibkra_export.h"
#include "KoColor.h"

class KRITALIBKRA_EXPORT KisKraSaver
{
public:

    KisKraSaver(KisDocument* document, const QString &filename, bool addMergedImage = true);

    ~KisKraSaver();

    QDomElement saveXML(QDomDocument& doc,  KisImageSP image);

    bool saveKeyframes(KoStore *store, const QString &uri, bool external);

    bool saveBinaryData(KoStore* store, KisImageSP image, const QString & uri, bool external, bool addMergedImage);

    bool saveResources(KoStore *store, KisImageSP image, const QString &uri);

    bool saveStoryboard(KoStore *store, KisImageSP image, const QString &uri);

    bool saveAnimationMetadata(KoStore *store, KisImageSP image, const QString &uri);

    bool saveAudio(KoStore *store);

    /// @return a list with everything that went wrong while saving
    QStringList errorMessages() const;

    /// @return a list with non-critical issues that happened while saving
    QStringList warningMessages() const;

private:
    void saveBackgroundColor(QDomDocument& doc, QDomElement& element, KisImageSP image);
    void saveAssistantsGlobalColor(QDomDocument& doc, QDomElement& element);
    void saveWarningColor(QDomDocument& doc, QDomElement& element, KisImageSP image);
    void saveCompositions(QDomDocument& doc, QDomElement& element, KisImageSP image);
    bool saveAssistants(KoStore *store,QString uri, bool external);
    bool saveAssistantsList(QDomDocument& doc, QDomElement& element);
    bool saveGrid(QDomDocument& doc, QDomElement& element);
    bool saveGuides(QDomDocument& doc, QDomElement& element);
    bool saveMirrorAxis(QDomDocument& doc, QDomElement& element);
    bool saveAudioXML(QDomDocument& doc, QDomElement& element);
    bool saveNodeKeyframes(KoStore *store, QString location, const KisNode *node);
    void saveResourcesToXML(QDomDocument& doc, QDomElement &element);
    void saveStoryboardToXML(QDomDocument& doc, QDomElement &element);
    void saveAnimationMetadataToXML(QDomDocument& doc, QDomElement &element, KisImageSP image);
    void saveColorHistory(QDomDocument &doc, QDomElement &element);

    bool saveKoColors(QDomDocument& doc, QDomElement &element, const QList<KoColor> &colors) const;

    struct Private;
    Private * const m_d;
};

#endif
