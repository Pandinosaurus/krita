/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2013-2014 Yue Liu <yue.liu@mail.com>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KOFILEDIALOG_H
#define KOFILEDIALOG_H

#include "kritawidgetutils_export.h"

#include <QFileDialog>
#include <QString>
#include <QStringList>


/**
 * Wrapper around QFileDialog providing native file dialogs
 * on KDE/Gnome/Windows/OSX/etc.
 */
class KRITAWIDGETUTILS_EXPORT KoFileDialog : public QObject
{
    Q_OBJECT

public:
    enum DialogType {
        OpenFile,
        OpenFiles,
        OpenDirectory,
        ImportFile,
        ImportFiles,
        ImportDirectory,
        SaveFile
    };

    /**
     * @brief constructor
     * @param parent The parent of the file dialog
     * @param dialogType usage of the file dialog
     * @param dialogName the name for the file dialog. This will be used to open
     * the filedialog in the last open location, instead the specified directory.
     *
     * @return The name of the entry user selected in the file dialog
     *
     */
    KoFileDialog(QWidget *parent,
                 KoFileDialog::DialogType type,
                 const QString &dialogName);

    ~KoFileDialog() override;

    void setCaption(const QString &caption);

    /**
     * @brief setDefaultDir set the default directory to defaultDir.
     *
     * @param defaultDir a path to a file or directory
     */
    void setDefaultDir(const QString &defaultDir, bool force = false);

    /**
     * @brief setDirectoryUrl set the default URI to defaultUri.
     * @param defaultUri a Uri to a file from some ContentProvider
     *
     * Used only on Android.
     */
    void setDirectoryUrl(const QUrl &defaultUri);

    /**
     * @brief setImageFilters sets the name filters for the file dialog to all
     * image formats Qt's QImageReader supports.
     */
    void setImageFilters();

    /**
     * @brief setMimeTypeFilters Update the list of file filters from mime types.
     * @param mimeTypeList A list of mime types that forms the basis of this dialog's file filters
     * @param defaultMimeType Sets the default filter based on this mime type
     */
    void setMimeTypeFilters(const QStringList &mimeTypeList,
                            QString defaultMimeType = QString());

    // Set the file type filter
    void setNameFilter(const QString &filter);

    //Set the selected file type filter
    void selectNameFilter(const QString &filter);

    /// Get the file names the user selected in the file dialog
    QStringList filenames();

    /// Get the file name the user selected in the file dialog
    QString filename();

    /**
     * @brief selectedNameFilter returns the name filter the user selected, either
     *    directory or by clicking on it.
     * @return
     */
    QString selectedNameFilter() const;

    QString selectedMimeType() const;

public Q_SLOTS:
    // Set default file extension matching the filter.
    void onFilterSelected(const QString &filter);

private:
    /**
     * @brief splitNameFilter take a single line of a QDialog name filter and split it
     *   into several lines. This is needed because a single line name filter can contain
     *   more than one mimetype, making it impossible to figure out the correct extension.
     *
     *   The methods takes care of some duplicated extensions, like jpeg and jpg.
     * @param nameFilter the namefilter to be split
     * @param mimeList a pointer to the list with mimes that shouldn't be added.
     * @return a stringlist of all name filters.
     */
    static QStringList splitNameFilter(const QString &nameFilter, QStringList *mimeList);

    void createFileDialog();

    QString getUsedDir(const QString &dialogName);
    void saveUsedDir(const QString &fileName, const QString &dialogName);

    // Get list of user friendly descriptions and map of these descriptions to the proposed file extension.
    const QPair<QStringList, QMap<QString, QString>> getFilterStringListFromMime(const QStringList &mimeList,
                                                  bool withAllSupportedEntry = false);

    class Private;
    Private * const d;
};

#endif /* KOFILEDIALOG_H */
