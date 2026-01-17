// SPDX-FileCopyrightText: 2025 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "config.h"

// Qt
#include <QTimer>

#include <GUI/QtDNSClient.hxx>
#include <Main/options.hxx>

#include <simgear/io/HTTPClient.hxx>
#include <simgear/io/HTTPRepository.hxx>

using simgear::HTTPRepository;


class UpdateFGData : public QObject
{
    Q_OBJECT
public:
    UpdateFGData(QObject* pr) : QObject(pr),
                                m_dns(new QtDNSClient(this, "fgdata"))
    {
        m_updateTimer.setInterval(20);

        connect(m_dns, &QtDNSClient::finished, [this]() {
            auto baseServer = m_dns->result();
            baseServer += QString("/fgdata_%1_%2").arg(FLIGHTGEAR_MAJOR_VERSION).arg(FLIGHTGEAR_MINOR_VERSION);
            m_updateServerUri = baseServer.toStdString();
            qInfo() << Q_FUNC_INFO << "will update from" << baseServer;
            createRepository();
        });

        connect(m_dns, &QtDNSClient::failed, [this](QString msg) {
            emit failed(tr("Update of data files failed due to a DNS error: %1").arg(msg));
        });

        m_dns->makeDNSRequest();
    }

    void createRepository()
    {
        const auto rp = flightgear::Options::sharedInstance()->downloadedDataRoot();
        m_repo.reset(new simgear::HTTPRepository(rp, &m_http));
        m_repo->setBaseUrl(m_updateServerUri);
        // in rapid RC scenarios, this can cause us to skip updates we need, and
        // then fail to check again later.
        m_repo->setRecheckTimeoutEnabled(false);
        m_repo->setMaximumPermittedFailureCount(0);
        
        m_repo->update();

        connect(&m_updateTimer, &QTimer::timeout, this, &UpdateFGData::onUpdateRepo);
        m_updateTimer.start();
    }

    void onUpdateRepo()
    {
        m_repo->process();
        m_http.update();


        if (!m_repo->isDoingSync()) {
            qInfo() << Q_FUNC_INFO << "finished sync";
            m_updateTimer.stop();
            emit finished();
        }

        const auto status = m_repo->failure();
        if (status != HTTPRepository::REPO_NO_ERROR) {
            m_error = true;
            QString errMsg = QString::fromStdString(m_repo->resultCodeAsString(status));
            m_updateTimer.stop();
            emit failed(errMsg);
        }

        const auto dlBytes = m_repo->bytesDownloaded();
        emit downloadProgress(dlBytes, dlBytes + m_repo->bytesToDownload());

        const auto lp = m_repo->lastCheckedPath();
        emit installProgress(tr("Updated %1").arg(QString::fromStdString(lp.utf8Str())), -1);
    }

    ~UpdateFGData() = default;

signals:
    void finished();

    void extractionError(QString file, QString msg);

    void installProgress(QString fileName, int percent);

    void downloadProgress(quint64 cur, quint64 total);

    void failed(QString message);

private:
    QtDNSClient* m_dns;
    std::string m_updateServerUri;

    QTimer m_updateTimer;

    std::unique_ptr<simgear::HTTPRepository> m_repo;
    simgear::HTTP::Client m_http;

    QUrl m_downloadUrl;

    bool m_error = false;
};
