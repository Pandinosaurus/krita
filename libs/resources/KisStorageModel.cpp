/*
 * SPDX-FileCopyrightText: 2019 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2023 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisStorageModel.h"

#include <QBuffer>
#include <QDir>
#include <QFont>
#include <QSqlError>
#include <QSqlQuery>
#include <KisResourceLocator.h>
#include <KoResourcePaths.h>
#include <KisResourceModelProvider.h>
#include <KisResourceThumbnailCache.h>
#include <QFileInfo>
#include <kis_assert.h>

#include <kconfig.h>
#include <kconfiggroup.h>
#include <ksharedconfig.h>

#include <kis_debug.h>

Q_GLOBAL_STATIC(KisStorageModel, s_instance)

struct KisStorageModel::Private {
    QList<QString> storages;
};

KisStorageModel::KisStorageModel(QObject *parent)
    : QAbstractTableModel(parent)
    , d(new Private())
{
    connect(KisResourceLocator::instance(), &KisResourceLocator::storageAdded, this, &KisStorageModel::addStorage);
    connect(KisResourceLocator::instance(), &KisResourceLocator::storageRemoved, this, &KisStorageModel::removeStorage);
    connect(KisResourceLocator::instance(), &KisResourceLocator::storageResynchronized, this, &KisStorageModel::storageResynchronized);
    connect(KisResourceLocator::instance(), &KisResourceLocator::storagesBulkSynchronizationFinished, this, &KisStorageModel::slotStoragesBulkSynchronizationFinished);

    resetQuery();
}

KisStorageModel *KisStorageModel::instance()
{
    return s_instance;
}

KisStorageModel::~KisStorageModel()
{
}

int KisStorageModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return d->storages.size();

}

int KisStorageModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return (int)MetaData;
}

QImage KisStorageModel::getThumbnailFromQuery(const QSqlQuery &query)
{
    const QString storageLocation = query.value("location").toString();
    const QString storageType = query.value("storage_type").toString();
    const QString storageIdAsString = query.value("id").toString();

    QImage img = KisResourceThumbnailCache::instance()->originalImage(storageLocation, storageType, storageIdAsString);
    if (!img.isNull()) {
        return img;
    } else {
        const int storageId = query.value("id").toInt();
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(storageId >= 0, img);

        bool result = false;
        QSqlQuery thumbQuery;
        result = thumbQuery.prepare("SELECT thumbnail FROM storages WHERE id = :id");
        if (!result) {
            qWarning() << "Failed to prepare query for thumbnail of" << storageId << thumbQuery.lastError();
            return img;
        }

        thumbQuery.bindValue(":id", storageId);

        result = thumbQuery.exec();

        if (!result) {
            qWarning() << "Failed to execute query for thumbnail of" << storageId << thumbQuery.lastError();
            return img;
        }

        if (!thumbQuery.next()) {
            qWarning() << "Failed to find thumbnail of" << storageId;
            return img;
        }

        QByteArray ba = thumbQuery.value("thumbnail").toByteArray();
        QBuffer buf(&ba);
        buf.open(QBuffer::ReadOnly);
        img.load(&buf, "PNG");
        KisResourceThumbnailCache::instance()->insert(storageLocation, storageType, storageIdAsString, img);
        return img;
    }
}

QVariant KisStorageModel::data(const QModelIndex &index, int role) const
{
    QVariant v;

    if (!index.isValid()) return v;
    if (index.row() > rowCount()) return v;
    if (index.column() > (int)MetaData) return v;

    if (role == Qt::FontRole) {
        return QFont();
    }

    QString location = d->storages.at(index.row());

    QSqlQuery query;

    bool r = query.prepare(
        "SELECT storages.id as id\n"
        ",      storage_types.name as storage_type\n"
        ",      location\n"
        ",      timestamp\n"
        ",      pre_installed\n"
        ",      active\n"
        "FROM   storages\n"
        ",      storage_types\n"
        "WHERE  storages.storage_type_id = storage_types.id\n"
        "AND    location = :location");

    if (!r) {
        qWarning() << "Could not prepare KisStorageModel data query" << query.lastError();
        return v;
    }

    query.bindValue(":location", location);

    r = query.exec();

    if (!r) {
        qWarning() << "Could not execute KisStorageModel data query" << query.lastError() << query.boundValues();
        return v;
    }

    if (!query.first()) {
        qWarning() << "KisStorageModel data query did not return anything";
        return v;
    }

    if ((role == Qt::DisplayRole || role == Qt::EditRole) && index.column() == Active) {
        return query.value("active");
    } else {
        switch (role) {
        case Qt::DisplayRole:
        {
            switch(index.column()) {
            case Id:
                return query.value("id");
            case StorageType:
                return query.value("storage_type");
            case Location:
                return query.value("location");
            case TimeStamp:
                return QDateTime::fromSecsSinceEpoch(query.value("timestamp").value<int>()).toString();
            case PreInstalled:
                return query.value("pre_installed");
            case Active:
                return query.value("active");
            case Thumbnail:
            {
                return getThumbnailFromQuery(query);
            }
            case DisplayName:
            {
                QMap<QString, QVariant> r = KisResourceLocator::instance()->metaDataForStorage(query.value("location").toString());
                QVariant name = query.value("location");
                if (r.contains(KisResourceStorage::s_meta_name) && !r[KisResourceStorage::s_meta_name].toString().isNull()) {
                    name = r[KisResourceStorage::s_meta_name];
                }
                else if (r.contains(KisResourceStorage::s_meta_title) && !r[KisResourceStorage::s_meta_title].toString().isNull()) {
                    name = r[KisResourceStorage::s_meta_title];
                }
                return name;
            }
            case Qt::UserRole + MetaData:
            {
                QMap<QString, QVariant> r = KisResourceLocator::instance()->metaDataForStorage(query.value("location").toString());
                return r;
            }
            default:
                return v;
            }
        }
        case Qt::CheckStateRole: {
            switch (index.column()) {
            case PreInstalled:
                if (query.value("pre_installed").toInt() == 0) {
                    return Qt::Unchecked;
                } else {
                    return Qt::Checked;
                }
            case Active:
                if (query.value("active").toInt() == 0) {
                    return Qt::Unchecked;
                } else {
                    return Qt::Checked;
                }
            default:
                return {};
            }
        }
        case Qt::DecorationRole: {
            if (index.column() == Thumbnail) {
                return getThumbnailFromQuery(query);
            }
            return {};
        }
        case Qt::UserRole + Id:
            return query.value("id");
        case Qt::UserRole + DisplayName:
        {
            QMap<QString, QVariant> r = KisResourceLocator::instance()->metaDataForStorage(query.value("location").toString());
            QVariant name = query.value("location");
            if (r.contains(KisResourceStorage::s_meta_name) && !r[KisResourceStorage::s_meta_name].toString().isNull()) {
                name = r[KisResourceStorage::s_meta_name];
            }
            else if (r.contains(KisResourceStorage::s_meta_title) && !r[KisResourceStorage::s_meta_title].toString().isNull()) {
                name = r[KisResourceStorage::s_meta_title];
            }
            return name;
        }
        case Qt::UserRole + StorageType:
            return query.value("storage_type");
        case Qt::UserRole + Location:
            return query.value("location");
        case Qt::UserRole + TimeStamp:
            return query.value("timestamp");
        case Qt::UserRole + PreInstalled:
            return query.value("pre_installed");
        case Qt::UserRole + Active:
            return query.value("active");
        case Qt::UserRole + Thumbnail:
            return getThumbnailFromQuery(query);
        case Qt::UserRole + MetaData:
        {
            QMap<QString, QVariant> r = KisResourceLocator::instance()->metaDataForStorage(query.value("location").toString());
            return r;
        }

        default:
            ;
        }
    }

    return v;
}

bool KisStorageModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid()) {

        if (role == Qt::CheckStateRole) {
            QSqlQuery query;
            bool r = query.prepare("UPDATE storages\n"
                                   "SET    active = :active\n"
                                   "WHERE  id = :id\n");
            query.bindValue(":active", value);
            query.bindValue(":id", index.data(Qt::UserRole + Id));

            if (!r) {
                qWarning() << "Could not prepare KisStorageModel update query" << query.lastError();
                return false;
            }

            r = query.exec();

            if (!r) {
                qWarning() << "Could not execute KisStorageModel update query" << query.lastError();
                return false;
            }

        }

        Q_EMIT dataChanged(index, index, {role});

        if (value.toBool()) {
            Q_EMIT storageEnabled(data(index, Qt::UserRole + Location).toString());
        }
        else {
            Q_EMIT storageDisabled(data(index, Qt::UserRole + Location).toString());
        }

    }
    return true;
}

Qt::ItemFlags KisStorageModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return QAbstractTableModel::flags(index) | Qt::ItemIsEditable | Qt::ItemNeverHasChildren;
}

KisResourceStorageSP KisStorageModel::storageForIndex(const QModelIndex &index) const
{

    if (!index.isValid()) return 0;
    if (index.row() > rowCount()) return 0;
    if (index.column() > (int)MetaData) return 0;

    QString location = d->storages.at(index.row());

    return KisResourceLocator::instance()->storageByLocation(KisResourceLocator::instance()->makeStorageLocationAbsolute(location));
}

KisResourceStorageSP KisStorageModel::storageForId(const int storageId) const
{
    QSqlQuery query;

    bool r = query.prepare("SELECT location\n"
                           "FROM   storages\n"
                           "WHERE  storages.id = :storageId");

    if (!r) {
        qWarning() << "Could not prepare KisStorageModel data query" << query.lastError();
        return 0;
    }

    query.bindValue(":storageId", storageId);

    r = query.exec();

    if (!r) {
        qWarning() << "Could not execute KisStorageModel data query" << query.lastError() << query.boundValues();
        return 0;
    }

    if (!query.first()) {
        qWarning() << "KisStorageModel data query did not return anything";
        return 0;
    }

    return KisResourceLocator::instance()->storageByLocation(KisResourceLocator::instance()->makeStorageLocationAbsolute(query.value("location").toString()));
}

QString findUnusedName(QString location, QString filename)
{
    // the Save Incremental Version incrementation in KisViewManager is way too complex for this task
    // and in that case there is a specific file to increment, while here we need to find just
    // an unused filename
    QFileInfo info = QFileInfo(location + "/" + filename);
    if (!info.exists()) {
        return filename;
    }

    QString extension = info.suffix();
    QString filenameNoExtension = filename.left(filename.length() - extension.length());


    QDir dir = QDir(location);
    QStringList similarEntries = dir.entryList(QStringList() << filenameNoExtension + "*");

    QList<int> versions;
    int maxVersionUsed = -1;
    for (int i = 0; i < similarEntries.count(); i++) {
        QString entry = similarEntries[i];
        //QFileInfo fi = QFileInfo(entry);
        if (!entry.endsWith(extension)) {
            continue;
        }
        QString versionStr = entry.right(entry.length() - filenameNoExtension.length()); // strip the common part
        versionStr = versionStr.left(versionStr.length() - extension.length());
        if (!versionStr.startsWith("_")) {
            continue;
        }
        versionStr = versionStr.right(versionStr.length() - 1); // strip '_'
        // now the part left should be a number
        bool ok;
        int version = versionStr.toInt(&ok);
        if (!ok) {
            continue;
        }
        if (version > maxVersionUsed) {
            maxVersionUsed = version;
        }
    }

    int versionToUse = maxVersionUsed > -1 ? maxVersionUsed + 1 : 1;
    int versionStringLength = 3;
    QString baseNewVersion = QString::number(versionToUse);
    while (baseNewVersion.length() < versionStringLength) {
        baseNewVersion.prepend("0");
    }

    QString newFilename = filenameNoExtension + "_" + QString::number(versionToUse) + extension;
    bool success = !QFileInfo(location + "/" + newFilename).exists();

    if (!success) {
        qCritical() << "The new filename for the bundle does exist." << newFilename;
    }

    return newFilename;

}

bool KisStorageModel::importStorage(QString filename, StorageImportOption importOption) const
{
    // 1. Copy the bundle/storage to the resource folder
    QFileInfo oldFileInfo(filename);
    QString newDir = KoResourcePaths::getAppDataLocation();
    QString newName = oldFileInfo.fileName();
    QString newLocation = newDir + '/' + newName;

    QFileInfo newFileInfo(newLocation);
    if (newFileInfo.exists()) {
        if (importOption == Overwrite) {
            //QFile::remove(newLocation);
            return false;
        } else if (importOption == Rename) {
            newName = findUnusedName(newDir, newName);
            newLocation = newDir + '/' + newName;
            newFileInfo = QFileInfo(newLocation);
        } else { // importOption == None
            return false;
        }
    }
    QFile::copy(filename, newLocation);

    // 2. Add the bundle as a storage/update database
    KisResourceStorageSP storage = QSharedPointer<KisResourceStorage>::create(newLocation);
    KIS_ASSERT(!storage.isNull());
    if (storage.isNull()) { return false; }
    if (!KisResourceLocator::instance()->addStorage(newLocation, storage)) {
        qWarning() << "Could not add bundle to the storages" << newLocation;
        return false;
    }
    return true;
}

QVariant KisStorageModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QVariant v = QVariant();
    if (role != Qt::DisplayRole) {
        return v;
    }
    if (orientation == Qt::Horizontal) {
        switch(section) {
        case Id:
            return i18n("Id");
        case StorageType:
            return i18n("Type");
        case Location:
            return i18n("Location");
        case TimeStamp:
            return i18n("Creation Date");
        case PreInstalled:
            return i18n("Preinstalled");
        case Active:
            return i18n("Active");
        case Thumbnail:
            return i18n("Thumbnail");
        case DisplayName:
            return i18n("Name");
        default:
            v = QString::number(section);
        }
        return v;
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

void KisStorageModel::addStorage(const QString &location)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    d->storages.append(location);
    endInsertRows();
}

void KisStorageModel::removeStorage(const QString &location)
{
    int row = d->storages.indexOf(QFileInfo(location).fileName());
    beginRemoveRows(QModelIndex(), row, row);
    d->storages.removeAt(row);
    endRemoveRows();
}

void KisStorageModel::slotStoragesBulkSynchronizationFinished()
{
    beginResetModel();
    resetQuery();
    endResetModel();

    Q_EMIT storagesBulkSynchronizationFinished();
}

void KisStorageModel::resetQuery()
{
    QSqlQuery query;

    bool r = query.prepare(
        "SELECT location\n"
        "FROM   storages\n"
        "ORDER BY id");
    if (!r) {
        qWarning() << "Could not prepare KisStorageModel query" << query.lastError();
    }

    r = query.exec();

    if (!r) {
        qWarning() << "Could not execute KisStorageModel query" << query.lastError();
    }

    d->storages.clear();
    while (query.next()) {
        d->storages << query.value(0).toString();
    }
}