// SetupRootDialog.cxx - part of GUI launcher using Qt5
//
// SPDX-FileCopyrightText: 2014 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LaunchConfig.hxx"
#include "config.h"

#include "SetupRootDialog.hxx"

#include <QtCore/qsettings.h>
#include <condition_variable>
#include <mutex>

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSettings>
#include <QThread>
#include <QUrl>

#include "simgear/debug/debug_types.h"
#include "ui_SetupRootDialog.h"

#include <Main/fg_init.hxx>
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Main/options.hxx>
#include <Viewer/WindowBuilder.hxx>

#include "QtLauncher.hxx"
#include "SettingsWrapper.hxx"
#include "UpdateDownloadedFGData.hxx"
#include <GUI/QtDNSClient.hxx>
#include <Main/MultipleInstanceLock.hxx>

#include <simgear/io/iostreams/sgstream.hxx>
#include <simgear/io/untar.hxx>
#include <simgear/misc/sg_dir.hxx>

using namespace std::chrono_literals;

const quint32 static_basePackagePatchLevel = 1;
const qint64 fourMB = 4 * 1024 * 1024;

namespace {
/**
     * @brief Calculates a progress percentage from the current and total values.
     *
     * Computes (current / total) × 100 using integer math. The result is clamped
     * to the range [0, 100] and rounded down. If @p total is zero, returns 0.
     *
     * @param current The current progress value (e.g. bytes downloaded).
     * @param total   The total value representing 100% completion.
     * @return An integer percentage between 0 and 100.
     */
int calculateProgressPercentage(quint64 current, quint64 total)
{
    constexpr int max_percent = 100;

    if (total == 0) {
        return 0;
    }

    // Compute percentage and clamp to avoid overflow or narrowing
    const quint64 percent = std::min((current * max_percent) / total, static_cast<quint64>(max_percent));

    return static_cast<int>(percent);
}
} // namespace

class InstallFGDataThread : public QThread
{
    Q_OBJECT
public:
    InstallFGDataThread(QObject* pr, QNetworkAccessManager* nm) : QThread(pr),
                                                                  m_networkManager(nm),
                                                                  m_dns(new QtDNSClient(this, "dl_fgdata"))
    {
        const auto rp = flightgear::Options::sharedInstance()->downloadedDataRoot();
        // ensure we remove any existing data, since it failed validation
        if (rp.exists()) {
            simgear::Dir ed(rp);
            ed.remove(true);
        }

        m_downloadPath = rp.dirPath() / ("_download_data_" + std::to_string(FLIGHTGEAR_MAJOR_VERSION) + "_" + std::to_string(FLIGHTGEAR_MINOR_VERSION));
        m_downloadPath.set_cached(false);
        if (m_downloadPath.exists()) {
            simgear::Dir ed(m_downloadPath);
            ed.remove(true);
        }

        const auto resumeDataPath = rp.dirPath() / std::string{"_fgdata_downloading.temp"};
        // FIXME: convert via std::filesystem::path
        m_resumeData.setFileName(QString::fromStdString(resumeDataPath.utf8Str()));

        // +1 to include the leading /
        m_pathPrefixLength = m_downloadPath.utf8Str().length() + 1;

        connect(m_dns, &QtDNSClient::finished, [this]() {
            m_servers = m_dns->results();
            startRequest();
        });

        connect(m_dns, &QtDNSClient::failed, [this](QString msg) {
            qWarning() << "Failed to query download servers dynamically, falling back to download.flightgear.org";
            m_servers.append("https://download.flightgear.org");
            startRequest();
        });

        m_dns->makeDNSRequest();
    }

    bool willResume() const
    {
        // if the resume data exists and is large enough, we will attempt to resume
        return static_cast<qint64>(m_resumeData.size()) >= fourMB;
    }

    /**
     * @brief setup the QNetworkRequest to do a resume download, by specifying
     * a byte-range in the HTTP request.
     *
     * @param req : the request to modify
     * @return qint64 : the number of bytes of overlap we will read
     */
    qint64 resumeDownload(QNetworkRequest& req)
    {
        m_resumeData.close(); // close because we may remove

        std::unique_lock g(m_mutex);
        qint64 resumeBytes = m_resumeData.size();
        m_resumedBytesSize = 0;
        m_readResumeFile = false;

        if (resumeBytes < fourMB) {
            m_resumeData.remove();
            return 0;
        }

        if (!m_resumeData.open(QIODevice::ReadOnly)) {
            return 0;
        }

        // allow a 4MB overlap, to verify the tail end of the
        // resume file matches what we download. This will catch weird
        // cases like the file on the server side changed, or an out-of-sync mirror
        resumeBytes -= fourMB;
        m_resumedBytesSize = resumeBytes;

        // seek to read out our overlap data into m_buffer
        m_resumeData.seek(resumeBytes);
        qInfo() << "Will resume at byte offset:" << resumeBytes;
        m_buffer = m_resumeData.read(fourMB);
        m_resumeData.close();

        if (static_cast<qint64>(m_buffer.size()) != fourMB) {
            qWarning() << "Failed to load existing downloaded data into the buffer";
            m_buffer.clear();
            m_resumeData.remove();
            return 0;
        }

        req.setRawHeader("Range", QString("bytes=%1-").arg(resumeBytes).toUtf8());
        m_readResumeFile = true;

        // finally, open the file for reading *and* writing, since once we release our
        // mutex, the running thread will start pulling data out now m_readResumeFile is set
        m_resumeData.open(QIODevice::ReadWrite);

        return fourMB;
    }

    void startRequest()
    {
        // must do this before we start any resume
        {
            std::unique_lock g(m_mutex);
            m_haveFirstMByte = false;
            m_buffer.clear();
        }

        // reset the archive
        m_archive.reset(new simgear::ArchiveExtractor(m_downloadPath));
        m_archive->setRemoveTopmostDirectory(true);
        m_archive->setCreateDirHashEntries(true);

        // SF doesn't support resuming, hard-code this for now. To be more generic we would
        // encode this in the server data somehow.
        if (willResume()) {
            if (m_servers.front().contains("sourceforge")) {
                // rotate front entry to the back; if the *only* entry is SF, we still
                // want to use it (and forget about resuming)
                auto s = m_servers.takeFirst();
                m_servers.append(s);
            }
        }

        QString templateUrl = m_servers.front() + QStringLiteral("/release-%1/FlightGear-%2.%3-data.txz");
        if (templateUrl.startsWith("https://sourceforge.net/")) {
            // deal with different SF syntax
            templateUrl += QStringLiteral("/download");
        }

        QString majorMinorVersion = QString(FLIGHTGEAR_MAJOR_MINOR_VERSION);
        m_downloadUrl = QUrl(templateUrl.arg(majorMinorVersion).arg(majorMinorVersion).arg(static_basePackagePatchLevel));

        qInfo() << "Download URI:" << m_downloadUrl;

        QNetworkRequest req{m_downloadUrl};
        req.setMaximumRedirectsAllowed(5);
        // important to get correct behaviour from SourceForge, default UA causes it not to
        // re-direct to the actual mirror correctly.
        req.setRawHeader("user-agent", "flightgear-installer");

        // check if we can resume an existing download, returns the
        // number of overlap bytes or zero for no resume.
        m_resumeOverlapBytes = resumeDownload(req);

        m_download = m_networkManager->get(req);
        m_download->setReadBufferSize(64 * 1024 * 1024);

        if (!m_readResumeFile) {
            // if we're not resuming, just write to the resume file
            m_resumeData.open(QIODevice::WriteOnly | QIODevice::Truncate);
        }

        connect(m_download, &QNetworkReply::downloadProgress, this, &InstallFGDataThread::onDownloadProgress);

        // lambda slot, but scoped to an object living on this thread.
        // this means the extraction work is done asynchronously with the
        // download
        connect(m_download, &QNetworkReply::readyRead, this, &InstallFGDataThread::processBytes);
        connect(m_download, &QNetworkReply::finished, this, &InstallFGDataThread::onReplyFinished);
        connect(m_download, &QNetworkReply::metaDataChanged, this, &InstallFGDataThread::onMetaDataChanged);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        connect(m_download, &QNetworkReply::errorOccurred, this, &InstallFGDataThread::onNetworkError);
#endif
    }

    ~InstallFGDataThread()
    {
        if (!m_done) {
            m_resumeData.close();
            if (m_download) {
                m_download->abort();
            }
            m_error = true;
        }

        if (m_download) {
            m_download->deleteLater();
            m_download = nullptr;
        }

        wait();

        if (m_error) {
            simgear::Dir ed(m_downloadPath);
            ed.remove(true);
        }
    }

    void updateProgress()
    {
        std::unique_lock g(m_mutex);
        const int percent = calculateProgressPercentage(m_extractedBytes, m_totalSize);
        auto fullPathStr = m_archive->mostRecentExtractedPath().utf8Str();
        fullPathStr.erase(0, m_pathPrefixLength);
        emit installProgress(QString::fromStdString(fullPathStr), percent);
    }

    void run() override
    {
        while (!m_error & !m_done) {
            // do the resume read first, as fast as the disk IO will allow
            if (m_readResumeFile) {
                QByteArray resumeBytes;
                {
                    std::unique_lock g(m_mutex);
                    const auto bytesToRead = std::min(static_cast<qint64>(m_resumedBytesSize - m_resumeData.pos()), static_cast<qint64>(16 * 0x100000));
                    resumeBytes = m_resumeData.read(bytesToRead);
                }
                m_archive->extractBytes((const uint8_t*)resumeBytes.constData(), resumeBytes.size());
                m_extractedBytes += resumeBytes.size();
                if (m_resumeData.pos() >= m_resumedBytesSize) {
                    qInfo() << "done reading resume file bytes";
                    m_readResumeFile = false;
                }
                updateProgress();
                continue;
            }

            QByteArray localBytes;
            {
                std::unique_lock g(m_mutex);
                if (m_buffer.isEmpty()) {
                    m_bufferWait.wait_for(g, 100ms);
                }

                // don't start pulling bytes out of the buffer while we
                // are checking the resume overlap
                if (m_resumeOverlapBytes > 0) {
                    continue;
                }

                // don't start passing bytes to the archive extractor, until we have 1MB
                // this is necessary to avoid passing redirect/404 page bytes in, and breaking
                // the extractor.
                if (!m_haveFirstMByte && (m_buffer.size() < 0x100000)) {
                    continue;
                } else {
                    m_haveFirstMByte = true;
                }

                // take at most 1MB
                localBytes = m_buffer.left(0x100000);
                m_buffer.remove(0, localBytes.length());
                m_resumeData.write(localBytes);
            }

            if (!localBytes.isEmpty()) {
                m_archive->extractBytes((const uint8_t*)localBytes.constData(), localBytes.size());
                m_extractedBytes += localBytes.size();
            }

            updateProgress();

            if (m_archive->hasError()) {
                m_error = true;
                // remove any resume file, since we probably have corrupted data somehow
                m_resumeData.close();
                m_resumeData.remove();
                qWarning() << "Archive error, installation will terminate";
            }

            if (m_archive->isAtEndOfArchive()) {
                // end the thread's event loop
                m_done = true;
            }
        }

        if (m_error) {
            // ensure the archive is cleaned up, including any files,
            // since we will likely attempt to remove it.
            m_archive.reset();
        } else {
            // create marker file for future updates
            {
                SGPath setupInfoPath = m_downloadPath / ".setup-info";
                sg_ofstream stream(setupInfoPath, std::ios::out | std::ios::binary);
                stream << m_downloadUrl.toString().toStdString();
            }

            const auto finalDataPath = flightgear::Options::sharedInstance()->downloadedDataRoot();
            SG_LOG(SG_IO, SG_INFO, "Renaming downloaded data to: " << finalDataPath);
            bool renamedOk = m_downloadPath.rename(finalDataPath);
            if (!renamedOk) {
                m_error = true;
            }

            // remove the resume-data file from disk, now we succeeded.
            m_resumeData.remove();
        }
    }

    void onNetworkError(QNetworkReply::NetworkError code)
    {
        if (code == QNetworkReply::OperationCanceledError) {
            // abort() is handled differently,
            // eg when a resume fails
            return;
        }

        SG_LOG(SG_IO, SG_WARN, "FGdata download failed, will re-try next mirror:" << code << " (" << m_download->errorString().toStdString() << ")");

        // don't need to delete, onReplyFinished will also fire
        m_servers.pop_front();
        if (m_servers.empty()) {
            m_error = true;
            emit failed(m_download->errorString());
        } else {
            startRequest();
            // will try a new request
        }
    }

    void onMetaDataChanged()
    {
        const int status = m_download->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (m_resumeOverlapBytes > 0) {
            if (status == 206) {
                // we will get the range request, excellent
            } else if (status == 200) {
                // full content, abandon resume
                m_resumeOverlapBytes = 0;
                m_buffer.clear();
                m_resumeData.close();
                m_resumeData.open(QIODevice::WriteOnly | QIODevice::Truncate);
                m_readResumeFile = false;
                qWarning() << "Server can't resume, reverting to full download";
            }
        }
    }

    void onDownloadProgress(quint64 got, quint64 total)
    {
        emit downloadProgress(got, total);
        m_totalSize = total + m_resumedBytesSize;
    }

    void processBytes()
    {
        QByteArray bytes = m_download->readAll();
        {
            std::unique_lock g(m_mutex);
            if (m_resumeOverlapBytes > 0) {
                const auto bytesToCompare = std::min(m_resumeOverlapBytes, static_cast<qint64>(bytes.size()));
                auto checkBytes = m_buffer.mid(m_buffer.size() - m_resumeOverlapBytes, bytesToCompare);
                if (checkBytes == bytes.left(bytesToCompare)) {
                    // all good, remove from the bytes downloaded,
                    // since they are already in m_buffer
                    bytes.remove(0, bytesToCompare);
                    m_resumeOverlapBytes -= bytesToCompare;
                } else {
                    g.unlock();
                    // remove the resume file, but don't adjust m_servers since we
                    // can reuse the same one
                    qWarning() << "Resume overlap bytes mismatch, will abandon resume and re-try";
                    m_resumeData.close();
                    m_resumeData.remove();
                    m_download->abort();
                    QTimer::singleShot(0, this, &InstallFGDataThread::startRequest);
                    return;
                }

                if (m_resumeOverlapBytes == 0) {
                    qInfo() << "resumed download correctly";
                }

                // if all available bytes were consumed,
                // don't bother waking up the thread
                if (bytes.isEmpty()) {
                    return;
                }
            }

            m_buffer.append(bytes);
            m_bufferWait.notify_one();
        }
    }

    void onReplyFinished()
    {
        // we can't use m_download here because in the case of re-trying,
        // we already replaced m_download with our new request.
        QNetworkReply* r = qobject_cast<QNetworkReply*>(sender());
        r->deleteLater();

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        if (r->error() != QNetworkReply::NoError) {
            onNetworkError(r->error());
        }
#endif
    }

    bool hasError() const
    {
        return m_error;
    }
signals:
    void extractionError(QString file, QString msg);

    void installProgress(QString fileName, int percent);

    void downloadProgress(quint64 cur, quint64 total);

    void failed(QString message);

private:
    QNetworkAccessManager* m_networkManager = nullptr;
    QtDNSClient* m_dns = nullptr;
    QStringList m_servers;
    std::mutex m_mutex;
    std::condition_variable m_bufferWait;
    QByteArray m_buffer;
    quint64 m_totalSize = 0;
    quint64 m_extractedBytes = 0;
    quint64 m_resumedBytesSize = 0;

    bool m_readResumeFile = false;
    bool m_haveFirstMByte = false;

    /// remaining bytes of overalp in m_buffer, that we need to receive
    /// before we are adding fresh bytes
    qint64 m_resumeOverlapBytes = 0;

    QUrl m_downloadUrl;
    QFile m_resumeData; // on-disk cache of downloaded TXZ, for resuming downloads

    bool m_done = false;
    QPointer<QNetworkReply> m_download;
    SGPath m_downloadPath;
    std::unique_ptr<simgear::ArchiveExtractor> m_archive;
    bool m_error = false;
    uint32_t m_pathPrefixLength = 0;
};

/////////////////////////////////////////////////////////////////////////////////////////////

QString SetupRootDialog::rootPathKey()
{
    // return a settings key like fg-root-2018-3-0
    return QString("fg-root-%1-%2").arg(FLIGHTGEAR_MAJOR_VERSION).arg(FLIGHTGEAR_MINOR_VERSION);
}

SetupRootDialog::SetupRootDialog(PromptState prompt, const SGPath& checked) : QDialog(),
                                                                              m_promptState(prompt),
                                                                              m_checkedPath(checked)
{
    auto exLock = flightgear::ExclusiveInstanceLock::instance();
    exLock->updateReason("setup-fgdata");

    m_ui.reset(new Ui::SetupRootDialog);
    m_ui->setupUi(this);

    connect(m_ui->browseButton, &QPushButton::clicked,
             this, &SetupRootDialog::onBrowse);
    connect(m_ui->downloadButton, &QPushButton::clicked,
            this, &SetupRootDialog::onDownload);
    connect(m_ui->changeDownloadLocation, &QPushButton::clicked,
            this, &SetupRootDialog::onSelectDownloadDir);
    connect(m_ui->defaultDownloadLocation, &QPushButton::clicked,
            this, &SetupRootDialog::onUseDefaultDownloadDir);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    auto options = flightgear::Options::sharedInstance();
    if (options->isOptionSet("download-dir")) {
        // if download dir is set on the command line, don't allow changing it here
        m_ui->changeDownloadLocation->setEnabled(false);
        m_ui->defaultDownloadLocation->hide();
    }

    // download only works for release builds where we generate TXZs for now
    // if we fix https://gitlab.com/flightgear/fgdata/-/issues/56 this may change,
    // eg generating FGData / base-package snapshot once per month
    if (strcmp(FG_BUILD_TYPE, "Release")) {
        m_ui->downloadButton->setEnabled(false);
    }

    m_ui->versionLabel->setText(tr("<h1>FlightGear %1</h1>").arg(FLIGHTGEAR_VERSION));
    m_ui->bigIcon->setPixmap(QPixmap(":/app-icon-large"));
    m_ui->contentsPages->setCurrentIndex(0);

    updatePromptText();

    if (prompt == NeedToUpdateDownloadedData) {
        m_ui->downloadButton->setText(tr("Update"));
        m_ui->changeDownloadLocation->hide();
        m_ui->defaultDownloadLocation->hide();
    }

    m_networkManager = new QNetworkAccessManager(this);
    m_networkManager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

bool SetupRootDialog::runDialog(bool usingDefaultRoot)
{
    // this code path is only used if have Qt enabled, but didn't use the launcher.
    // in that case, we're coming from Options::setupRoot, which stores the path
    // it checked in gloabls::get_fg_root() *before* it calls us here.
    SetupRootDialog::PromptState prompt =
        usingDefaultRoot ? DefaultPathCheckFailed : ExplicitPathCheckFailed;
    return runDialog(prompt, globals->get_fg_root());
}

bool SetupRootDialog::runUpdateDialog(bool usingDefaultRoot)
{
    SG_UNUSED(usingDefaultRoot);
    return runDialog(SetupRootDialog::PromptState::NeedToUpdateDownloadedData, SGPath{});
}

bool SetupRootDialog::runDialog(PromptState prompt, const SGPath& checkedPath)
{
    // avoid double Apple menu and other weirdness if both Qt and OSG
    // try to initialise various Cocoa structures.
    flightgear::WindowBuilder::setPoseAsStandaloneApp(false);

    SetupRootDialog dlg(prompt, checkedPath);
    dlg.exec();
    if (dlg.result() != QDialog::Accepted) {
        return false;
    }

    return true;
}


flightgear::SetupRootResult SetupRootDialog::restoreUserSelectedRoot(SGPath& sgpath)
{
    const auto readOnly = fgGetBool("/sim/fghome-readonly", false);
    auto options = flightgear::Options::sharedInstance();
    auto settings = flightgear::getQSettings();
    QString path = settings.value(rootPathKey()).toString();
    const bool ask = flightgear::checkKeyboardModifiersForSettingFGRoot();

    QString downloadDir = settings.value("download-dir").toString();
    if (!downloadDir.isEmpty()) {
        options->setCustomDownloadDir(SGPath::fromUtf8(downloadDir.toStdString()));
    }

    if (ask || (path == QStringLiteral("!ask"))) {
        if (readOnly) {
            // assume the primary copy will ask, so just bail out
            SG_LOG(SG_GENERAL, SG_MANDATORY_INFO, "restoreUserSelectedRoot: choice is 'ask', but we are read-only, exiting.");
            return flightgear::SetupRootResult::UserExit;
        }

        bool ok = runDialog(ManualChoiceRequested, SGPath{});
        if (!ok) {
            return flightgear::SetupRootResult::UserExit;
        }

        sgpath = globals->get_fg_root();
        return flightgear::SetupRootResult::UserSelected;
    }

    if (flightgear::ExclusiveInstanceLock::isLocked()) {
        bool ok = flightgear::ExclusiveInstanceLock::showWaitDialog();
        if (!ok) {
            return flightgear::SetupRootResult::UserExit;
        }
    }

    if (path.isEmpty()) {
        if (downloadedDataExistsButStale()) {
            bool ok = runDialog(NeedToUpdateDownloadedData, options->downloadedDataRoot());
            if (!ok) {
                return flightgear::SetupRootResult::UserExit;
            }

            // assume update worked, fall through
        }

        const auto pkgData = options->platformDefaultRoot();
        if (flightgear::Options::isFGData(pkgData)) {
            const auto pkgDataQt = QString::fromStdString(pkgData.utf8Str());
            if (validateVersion(pkgDataQt)) {
                return flightgear::SetupRootResult::UseDefault;
            }

            qWarning() << pkgDataQt << "contains FGData, but version doesn't match required:" << FLIGHTGEAR_VERSION;
        }
    }

    // to give better feedback, we need to record which path we tried,
    // that failed our check
    SGPath checkedPath;
    if (validatePath(path)) {
        if (validateVersion(path)) {
            sgpath = SGPath::fromUtf8(path.toStdString());
            return flightgear::SetupRootResult::RestoredOk;
        }

        // path seemed good, but version failed, so this is the one to report
        checkedPath = SGPath::fromUtf8(path.toStdString());
    }

    if (downloadedDataAcceptable()) {
        return flightgear::SetupRootResult::UseDefault;
    } else if (checkedPath.isNull()) {
        // if the download data exists, use that as our 'location we checked'
        SGPath r = flightgear::Options::sharedInstance()->downloadedDataRoot();
        if (flightgear::Options::isFGData(r)) {
            checkedPath = r;
        }
    }

    // okay, we don't have an acceptable FG_DATA anywhere we can find, we
    // have to ask the user what they want to do.
    bool ok = false;
    if (checkedPath.isNull()) {
        ok = runDialog(DefaultPathCheckFailed, SGPath{});
    } else {
        ok = runDialog(VersionCheckFailed, checkedPath);
    }

    if (!ok) {
        return flightgear::SetupRootResult::UserExit;
    }

    // run dialog sets fg_root, so this
    // behaviour is safe and correct.
    sgpath = globals->get_fg_root();
    return flightgear::SetupRootResult::UserSelected;
}

void SetupRootDialog::askRootOnNextLaunch()
{
    auto settings = flightgear::getQSettings();
    // set the option to the magic marker value
    settings.setValue(rootPathKey(), "!ask");
}

bool SetupRootDialog::validatePath(QString path)
{
    return flightgear::Options::isFGData(SGPath::fromUtf8(path.toStdString()));
}

/**
 * @brief Ensure the base package at 'path' is the same or more recent than our
 * specified base package minimum version.
 *
 * @param path : candidate base package folder

 */
bool SetupRootDialog::validateVersion(QString path)
{
    std::string minBasePackageVersion = std::to_string(FLIGHTGEAR_MAJOR_VERSION) + "." + std::to_string(FLIGHTGEAR_MINOR_VERSION) + "." + std::to_string(static_basePackagePatchLevel);

    std::string ver = fgBasePackageVersion(SGPath::fromUtf8(path.toStdString()));

    // ensure major & minor fields match exactly
    if (simgear::strutils::compare_versions(minBasePackageVersion, ver, 2) != 0) {
        return false;
    }

    return simgear::strutils::compare_versions(minBasePackageVersion, ver) >= 0;
}

bool SetupRootDialog::downloadedDataAcceptable()
{
    SGPath r = flightgear::Options::sharedInstance()->downloadedDataRoot();
    QString dlRoot = QString::fromStdString(r.utf8Str());
    return validatePath(dlRoot) && validateVersion(dlRoot);
}

bool SetupRootDialog::downloadedDataExistsButStale()
{
    SGPath r = flightgear::Options::sharedInstance()->downloadedDataRoot();
    QString dlRoot = QString::fromStdString(r.utf8Str());
    if (!validatePath(dlRoot)) {
        return false;
    }

    std::string minBasePackageVersion = std::to_string(FLIGHTGEAR_MAJOR_VERSION) + "." + std::to_string(FLIGHTGEAR_MINOR_VERSION) + "." + std::to_string(static_basePackagePatchLevel);
    std::string ver = fgBasePackageVersion(r);

    // major or minor mismatch, we can't use it
    // this 'should' be impossible given how we comput downloadedDataRoot
    if (simgear::strutils::compare_versions(minBasePackageVersion, ver, 2) != 0) {
        return false;
    }

    // check for suffix mismatch, then we will always update
    const auto info = fgBasePackageInfo(r);
    if (!info) {
        SG_LOG(SG_IO, SG_INFO, "downloaded data missing the base_package.json file, won't attempt to update it");
        return false;
    }

    // BUILD_SUFFIX includes a leading hyphen, but the JSON info doesn't
    std::string buildSuffix(BUILD_SUFFIX);
    if (buildSuffix.find("-") == 0) {
        buildSuffix.erase(0, 1); // effectively a 'pop front'
    }

    if (info && (info.value().suffix != buildSuffix)) {
        SG_LOG(SG_IO, SG_INFO, "Base package suffix mismatch, build suffix is '" << buildSuffix << "'");
        return true;
    }

    // update needed if the on-disk base package version is *lower* than static_basePackagePatchLevel
    return simgear::strutils::compare_versions(ver, minBasePackageVersion) < 0;
}

SetupRootDialog::~SetupRootDialog() = default;

void SetupRootDialog::onBrowse()
{
    m_browsedPath = QFileDialog::getExistingDirectory(this,
                                                     tr("Choose FlightGear data folder"));
    if (m_browsedPath.isEmpty()) {
        return;
    }

    if (!validatePath(m_browsedPath)) {
        m_promptState = ChoseInvalidLocation;
        updatePromptText();
        return;
    }

    if (!validateVersion(m_browsedPath)) {
        m_promptState = ChoseInvalidVersion;
        updatePromptText();
        return;
    }

    globals->set_fg_root(m_browsedPath.toStdString());

    auto settings = flightgear::getQSettings();
    settings.setValue(rootPathKey(), m_browsedPath);

    accept(); // we're done
}

bool SetupRootDialog::locationIsWritable(QString path)
{
    // we don't use QFileInfo::isWriteable here because of complexity around
    // NTFS ACL checks (needs Qt 6.6 for QNtfsPermissionCheckGuard)
    QFile f(path + "/_check_write");
    if (!f.open(QIODevice::NewOnly | QIODevice::WriteOnly)) {
        return false;
    }
    f.remove(); // closes
    return true;
}

void SetupRootDialog::onSelectDownloadDir()
{
    auto settings = flightgear::getQSettings();

    auto dd = flightgear::Options::sharedInstance()->actualDownloadDir();
    const auto dlp = QString::fromStdString(dd.utf8Str());

    QString downloadDir = QFileDialog::getExistingDirectory(this,
                                                            tr("Choose location to store downloaded files."), dlp);
    if (downloadDir.isEmpty()) {
        return;
    }

    if (!locationIsWritable(downloadDir)) {
        m_browsedPath = downloadDir;
        m_promptState = ChoseInvalidDownloadLocation;
        updatePromptText();
        return;
    }

    m_promptState = ManualChoiceRequested;
    settings.setValue("download-dir", downloadDir);
    flightgear::Options::sharedInstance()->setOption("download-dir", downloadDir.toStdString());
    updatePromptText();
}

void SetupRootDialog::onUseDefaultDownloadDir()
{
    auto settings = flightgear::getQSettings();
    settings.remove("download-dir");
    flightgear::Options::sharedInstance()->clearOption("download-dir");
    updatePromptText();
}

void SetupRootDialog::onDownload()
{
    // clear !ask value or custom root
    auto settings = flightgear::getQSettings();
    settings.remove(rootPathKey());

    if (m_promptState == NeedToUpdateDownloadedData) {
        onUpdate();
        return;
    }

    m_promptState = DownloadingExtractingArchive;
    updatePromptText();

    m_ui->contentsPages->setCurrentIndex(1);

    auto installThread = new InstallFGDataThread(this, m_networkManager);
    connect(installThread, &InstallFGDataThread::downloadProgress, this, [this](quint64 current, quint64 total) {
        m_ui->downloadProgress->setValue(current);
        m_ui->downloadProgress->setMaximum(total);

        const quint64 currentMb = current / (1024 * 1024);
        const quint64 totalMb = total / (1024 * 1024);

        const int percent = calculateProgressPercentage(current, total);

        m_ui->downloadText->setText(tr("Downloaded %1 of %2 MB (%3%)").arg(currentMb).arg(totalMb).arg(percent));
    });

    connect(installThread, &InstallFGDataThread::installProgress, this, [this](QString s, int percent) {
        m_ui->installText->setText(tr("Installation %1% complete.\nExtracting %2").arg(percent).arg(s));
        m_ui->installProgress->setValue(percent);
        // m_ui->installProgress->setMaximum(total);
    });

    connect(installThread, &InstallFGDataThread::failed, this, [this](QString s) {
        m_ui->downloadText->setText(tr("Download failed: %1").arg(s));
        m_lastErrorMessage = s;
        m_promptState = DownloadFailed;
    });

    connect(installThread, &InstallFGDataThread::finished, this, [this, installThread]() {
        if (installThread->hasError()) {
            // go back to the first page
            m_promptState = DownloadFailed;
            updatePromptText();
            m_ui->contentsPages->setCurrentIndex(0);
        } else {
            accept();
        }
    });

    installThread->start();
}

void SetupRootDialog::onUpdate()
{
    m_promptState = UpdatingViaTerrasync;
    updatePromptText();

    m_ui->contentsPages->setCurrentIndex(1);
    m_ui->installProgress->setMaximum(0); // show a 'unknown amount' progress

    auto updateThread = new UpdateFGData(this);
    connect(updateThread, &UpdateFGData::downloadProgress, this, [this](quint64 cur, quint64 total) {
        m_ui->downloadProgress->setValue(cur);
        m_ui->downloadProgress->setMaximum(total);

        const int curMb = cur / (1024 * 1024);
        const int totalMb = total / (1024 * 1024);
        const int percent = total > 0 ? ((cur * 100) / total) : 0;
        m_ui->downloadText->setText(tr("Downloaded %1 of %2 MB (%3%)").arg(curMb).arg(totalMb).arg(percent));
    });

    connect(updateThread, &UpdateFGData::installProgress, this, [this](QString s, int percent) {
        m_ui->installText->setText(s);
    });

    connect(updateThread, &UpdateFGData::failed, this, [this](QString s) {
        m_ui->downloadText->setText(tr("Update failed: %1").arg(s));
    });

    connect(updateThread, &UpdateFGData::finished, this, [this]() {
        accept();
    });
}

// void SetupRootDialog::onUseDefaults()
// {
//     SGPath r = flightgear::Options::sharedInstance()->platformDefaultRoot();
//     m_browsedPath = QString::fromStdString(r.utf8Str());
//     globals->set_fg_root(r);
//     auto settings = flightgear::getQSettings();
//     settings.remove(rootPathKey()); // remove any setting
//     accept();
// }

void SetupRootDialog::updatePromptText()
{
    QString t;
    QString curRoot = QString::fromStdString(m_checkedPath.utf8Str());
    switch (m_promptState) {
    case DefaultPathCheckFailed:
        t = tr("FlightGear needs to download additional data files. This can be done automatically by pressing 'Download', or you can download them yourself and select their location.");
        break;

    case ExplicitPathCheckFailed:
        t = tr("The requested location '%1' does not appear to be a valid set of data files for FlightGear").arg(curRoot);
        break;

    case VersionCheckFailed:
    {
        QString curVer = QString::fromStdString(fgBasePackageVersion(m_checkedPath));
        t = tr("Detected incompatible version of the data files: version %1 found, but this is FlightGear %2. " \
               "(At location: '%3') " \
               "Please install or select a matching set of data files.").arg(curVer).arg(QString::fromLatin1(FLIGHTGEAR_VERSION)).arg(curRoot);
        break;
    }

    case ManualChoiceRequested:
        t = tr("Please select or download a copy of the FlightGear data files.");
        break;

    case ChoseInvalidLocation:
        t = tr("The chosen location (%1) does not appear to contain FlightGear data files. Please try another location.").arg(m_browsedPath);
        break;

    case ChoseInvalidVersion:
    {
        QString curVer = QString::fromStdString(fgBasePackageVersion(m_browsedPath.toStdString()));
        t = tr("The chosen location (%1) contains files for version %2, but this is FlightGear %3. "
               "Please update or try another location")
                .arg(m_browsedPath)
                .arg(curVer)
                .arg(QString::fromLatin1(FLIGHTGEAR_VERSION));
        break;
    }

    case ChoseInvalidArchive:
        t = tr("The chosen file (%1) is not a valid compressed archive.").arg(m_browsedPath);
        break;

    case ChoseInvalidDownloadLocation:
        t = tr("The chosen download location (%1) is not writable. Please select another location.").arg(m_browsedPath);
        break;

    case DownloadingExtractingArchive:
        t = tr("Please wait while the data files are downloaded, extracted and verified.");
        break;


    case UpdatingViaTerrasync:
        t = tr("Please wait while the data files are updated and verified.");
        break;

    case NeedToUpdateDownloadedData:
        t = tr("The data files (found at '%2') need to be updated to version %1. "
               "Please press 'Update', or if you prefer, manually download the correct data files and then select them.")
                .arg(QString::fromLatin1(FLIGHTGEAR_VERSION))
                .arg(curRoot);
        break;


    case DownloadFailed:
        t = tr("Automatic download of the data files has failed. Please download the files manually, using the instructions at https://www.flightgear.org/download/data/ (Error details: %1)").arg(m_lastErrorMessage);
        break;
    }

    m_ui->promptText->setText(t);

    auto dd = flightgear::Options::sharedInstance()->actualDownloadDir();
    const auto dlp = QString::fromStdString(dd.utf8Str());
    m_ui->downloadLocationLabel->setText(tr("Data files will be downloaded to: %1").arg(dlp));

    m_ui->defaultDownloadLocation->setEnabled(flightgear::Options::sharedInstance()->isOptionSet("download-dir"));
}

#include "SetupRootDialog.moc"
