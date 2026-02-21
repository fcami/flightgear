// SPDX-FileCopyrightText: 2018 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QQuickItem>
#include <QUrl>
#include <QImage>
#include <QNetworkReply>

class QNetworkAccessManager;

class PreviewImageItem : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QUrl imageUrl READ imageUrl WRITE setImageUrl NOTIFY imageUrlChanged)

    Q_PROPERTY(QSize sourceSize READ sourceSize NOTIFY sourceSizeChanged)

    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)

    Q_PROPERTY(float aspectRatio READ aspectRatio NOTIFY sourceSizeChanged)

    Q_PROPERTY(QString packageId READ packageId WRITE setPackageId NOTIFY packageIdChanged)
public:
    PreviewImageItem(QQuickItem* parent = nullptr);
    ~PreviewImageItem();

    QSGNode* updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

    QUrl imageUrl() const;

    QSize sourceSize() const;

    static void setGlobalNetworkAccess(QNetworkAccessManager* netAccess);

    bool isLoading() const;

    float aspectRatio() const;

    /**
      * @brief clear the image immediately, so we don't see a stale / expired
      * one while attempting to load the next one
      */
    Q_INVOKABLE void clearImage();

    QString packageId() const;
signals:
    void imageUrlChanged();
    void sourceSizeChanged();
    void isLoadingChanged();
    void packageIdChanged();

public slots:

    void setImageUrl(QUrl url);
    void setPackageId(QString packageId);

private slots:
    void onDownloadError(QNetworkReply::NetworkError errorCode);

    void onFinished();
private:
    void clear();

    void setImage(QImage image);
    void startDownload();
    void computeUrls();

    QUrl m_imageUrl;
    QList<QUrl> m_urlsToTry;
    QString m_packageId;

    bool m_imageDirty = false;
    QImage m_image;
    unsigned int m_downloadRetryCount = 0;
    bool m_requestActive = false;
};
