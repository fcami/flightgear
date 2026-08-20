

#include "Main/globals.hxx"
#include "config.h"
#include "simgear/misc/sg_path.hxx"

#include "DownloadTerrasyncSharedData.hxx"

// Qt
#include <QNetworkAccessManager>

#include <GUI/QtDNSClient.hxx>
#include <Main/options.hxx>


#include <simgear/io/untar.hxx>
#include <simgear/misc/sg_dir.hxx>


using namespace std::chrono_literals;

namespace {

    int calculateProgressPercentage(quint64 current, quint64 total) {

        constexpr int max_percent = 100;

        if (total == 0) {
            return 0;
        }

        // Compute percentage and clamp to avoid overflow or narrowing
        const quint64 percent = std::min((current * max_percent) / total, static_cast<quint64>(max_percent));

        return static_cast<int>(percent);
    }
}


DownloadTerrasyncSharedData::DownloadTerrasyncSharedData(QObject* pr, QNetworkAccessManager* nam) :
    QThread(pr),
    m_networkManager(nam),
    m_dns(new QtDNSClient(this, "ws20"))
{
    const SGPath terrasyncRoot = flightgear::Options::sharedInstance()->actualTerrasyncDir();
    if (!terrasyncRoot.exists()) {
        simgear::Dir d(terrasyncRoot);
        d.create(0755);
    }

    m_items.emplace_back("../SharedModels.txz", terrasyncRoot / "Models", tr("shared scenery models"));
    m_items.emplace_back("Airports_archive.tgz", terrasyncRoot / "Airports", tr("airports data"));

    m_updateTimer.setInterval(20);

    connect(m_dns, &QtDNSClient::finished, [this]() {
        auto baseServer = m_dns->result();

        m_serverUri = baseServer;
        qInfo() << Q_FUNC_INFO << "will download shared data from" << baseServer;
        startNextDownload();

    });

    connect(m_dns, &QtDNSClient::failed, [this](QString msg) {
        // use us1mirror
        qWarning() << "DNS query failed:" << msg;
        m_serverUri = QStringLiteral("https://de3mirror.flightgear.org/ws2/");
        startNextDownload();
    });

    m_dns->makeDNSRequest();
}

DownloadTerrasyncSharedData::~DownloadTerrasyncSharedData()
{
    if (!m_done) {
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
}

void DownloadTerrasyncSharedData::startNextDownload()
{
    std::unique_lock g(m_mutex);
    if (m_items.empty()) {
        m_done = true; // exit the thread loop
        return;
    }

    const auto& item = m_items.front();
// if the directory exists, skip it : we will use normal TerraSync incremental updating
    if (item.terrasyncPath.exists()) {
        m_items.pop_front();
        QTimer::singleShot(0, this, &DownloadTerrasyncSharedData::startNextDownload);
        return;
    }

    // reset the archive
    m_archive.reset(new simgear::ArchiveExtractor(item.terrasyncPath));
    m_archive->setRemoveTopmostDirectory(true);
    m_archive->setCreateDirHashEntries(true);

    // +1 to include the leading /
    m_pathPrefixLength = item.terrasyncPath.utf8Str().length() + 1;

    QString url = m_serverUri + item.relativeURIPath;
    QNetworkRequest req{url};
    req.setMaximumRedirectsAllowed(5);
    req.setRawHeader("user-agent", "flightgear-installer");
    m_download = m_networkManager->get(req);
    m_download->setReadBufferSize(64 * 1024 * 1024);

    m_buffer.clear();

    connect(m_download, &QNetworkReply::downloadProgress, this, &DownloadTerrasyncSharedData::onDownloadProgress);
    connect(m_download, &QNetworkReply::readyRead, this, &DownloadTerrasyncSharedData::processBytes);
    connect(m_download, &QNetworkReply::finished, this, &DownloadTerrasyncSharedData::onReplyFinished);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_download, &QNetworkReply::errorOccurred, this, &DownloadTerrasyncSharedData::onNetworkError);
#endif

    m_totalSize = 0;
    m_haveFirstMByte = false;

    emit beginArchive(item.archiveName);
    m_items.pop_front();
}

void DownloadTerrasyncSharedData::onNetworkError(QNetworkReply::NetworkError code)
{
    if (code == QNetworkReply::OperationCanceledError) {
        // abort() is handled differently,
        // eg when a resume fails
        return;
    }

    SG_LOG(SG_IO, SG_WARN, "shared data download failed (" << m_download->errorString().toStdString() << "): for "
        << m_download->url().toString().toStdString());

    m_error = true;
    emit failed(m_download->errorString());
}

void DownloadTerrasyncSharedData::onDownloadProgress(quint64 got, quint64 total)
{
    emit downloadProgress(got, total);
    m_totalSize = total;
}

void DownloadTerrasyncSharedData::processBytes()
{
    QByteArray bytes = m_download->readAll();
    {
        std::unique_lock g(m_mutex);
        m_buffer.append(bytes);
        m_bufferWait.notify_one();
    }
}

void DownloadTerrasyncSharedData::onReplyFinished()
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

void DownloadTerrasyncSharedData::updateProgress()
{
    if (!m_archive) {
        return; // happens upon completion
    }

    std::unique_lock g(m_mutex);
    const int percent = calculateProgressPercentage(m_extractedBytes, m_totalSize);
    auto fullPathStr = m_archive->mostRecentExtractedPath().utf8Str();
    fullPathStr.erase(0, m_pathPrefixLength);
    emit installProgress(QString::fromStdString(fullPathStr), percent);
}

void DownloadTerrasyncSharedData::run()
{
    while (!m_error & !m_done) {
        QByteArray localBytes;
        {
            std::unique_lock g(m_mutex);
            if (m_buffer.isEmpty()) {
                m_bufferWait.wait_for(g, 100ms);
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
        }

        if (!localBytes.isEmpty()) {
            m_archive->extractBytes((const uint8_t*)localBytes.constData(), localBytes.size());
            m_extractedBytes += localBytes.size();
        }

        updateProgress();
        if (m_archive) {
            if (m_archive->hasError()) {
                m_error = true;
                qWarning() << "Archive error";
            } else if (m_archive->isAtEndOfArchive()) {
                m_archive.reset();
                // start the next download, or set m_done=true if nothing left to do
                QTimer::singleShot(0, this, &DownloadTerrasyncSharedData::startNextDownload);
            }
        }
    }

    if (m_error) {
        // ensure the archive is cleaned up, including any files,
        // since we will likely attempt to remove it.
        m_archive.reset();
    }
}