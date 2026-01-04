// SetupRootDialog.hxx - part of GUI launcher using Qt5
//
// SPDX-FileCopyrightText: 2014 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QDialog>
#include <QScopedPointer>
#include <QString>

#include <string>

#include <simgear/misc/sg_path.hxx>

#include "QtLauncher.hxx"

namespace Ui
{
    class SetupRootDialog;
}

class QNetworkAccessManager;

class SetupRootDialog : public QDialog
{
    Q_OBJECT
public:

    ~SetupRootDialog();

    static bool runDialog(bool usingDefaultRoot);

    static bool runUpdateDialog(bool usingDefaultRoot);

    static flightgear::SetupRootResult restoreUserSelectedRoot(SGPath& path);

    static void askRootOnNextLaunch();

    static QString rootPathKey();
private slots:

    void onBrowse();

    void onDownload();
    void onUpdate();
    void onSelectDownloadDir();
    void onUseDefaultDownloadDir();
    void updatePromptText();
private:
    enum PromptState {
        DefaultPathCheckFailed,
        ExplicitPathCheckFailed,
        VersionCheckFailed,
        NeedToUpdateDownloadedData,
        ManualChoiceRequested,
        ChoseInvalidLocation,
        ChoseInvalidVersion,
        ChoseInvalidArchive,
        DownloadingExtractingArchive,
        UpdatingViaTerrasync,
        ChoseInvalidDownloadLocation,
        DownloadFailed
    };

    SetupRootDialog(PromptState prompt, const SGPath&);

    bool locationIsWritable(QString path);

    static bool runDialog(PromptState prompt, const SGPath& checkedPath);

    static bool validatePath(QString path);
    static bool validateVersion(QString path);

    static bool defaultRootAcceptable();
    static bool downloadedDataAcceptable();
    static bool downloadedDataExistsButStale();

    PromptState m_promptState;
    QScopedPointer<Ui::SetupRootDialog> m_ui;
    QString m_browsedPath;
    SGPath m_checkedPath;
    QString m_lastErrorMessage;
    QNetworkAccessManager* m_networkManager;
};
