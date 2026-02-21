// SPDX-FileCopyrightText: 2018 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PreviewImageItem.hxx"

#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QTimer>

#include <Main/globals.hxx>


#include <simgear/package/Catalog.hxx>
#include <simgear/package/Package.hxx>
#include <simgear/package/Root.hxx>

namespace {

QNetworkAccessManager* global_previewNetAccess = nullptr;

}

PreviewImageItem::PreviewImageItem(QQuickItem* parent) :
    QQuickItem(parent)
{
    setFlag(ItemHasContents);
  //  setImplicitWidth(STANDARD_THUMBNAIL_WIDTH);
   // setImplicitHeight(STANDARD_THUMBNAIL_HEIGHT);

    Q_ASSERT(global_previewNetAccess);
}

PreviewImageItem::~PreviewImageItem() = default;

QSGNode *PreviewImageItem::updatePaintNode(QSGNode* oldNode, QQuickItem::UpdatePaintNodeData *)
{
    if (m_image.isNull()) {
        delete oldNode;
        return nullptr;
    }

    QSGSimpleTextureNode* textureNode = static_cast<QSGSimpleTextureNode*>(oldNode);
    if (m_imageDirty || !textureNode) {
        if (!textureNode) {
            textureNode = new QSGSimpleTextureNode;
            textureNode->setOwnsTexture(true);
        }

        QSGTexture* tex = window()->createTextureFromImage(m_image);
        textureNode->setTexture(tex);
        textureNode->markDirty(QSGBasicGeometryNode::DirtyMaterial);
        m_imageDirty = false;
    }

    textureNode->setRect(QRectF(0, 0, width(), height()));
    return textureNode;
}

QUrl PreviewImageItem::imageUrl() const
{
    return m_imageUrl;
}

QSize PreviewImageItem::sourceSize() const
{
    return m_image.size();
}

QString PreviewImageItem::packageId() const
{
    return m_packageId;
}

void PreviewImageItem::setPackageId(QString packageId)
{
    if (m_packageId == packageId)
        return;

    m_packageId = packageId;
    emit packageIdChanged();
}

void PreviewImageItem::setGlobalNetworkAccess(QNetworkAccessManager *netAccess)
{
    global_previewNetAccess = netAccess;
}

bool PreviewImageItem::isLoading() const
{
    return m_requestActive;
}

float PreviewImageItem::aspectRatio() const
{
    return static_cast<float>(m_image.width()) / m_image.height();
}

void PreviewImageItem::clearImage()
{
    m_image = QImage{};
    m_imageDirty = true;
    update();
    emit isLoadingChanged();
}

void PreviewImageItem::clear()
{
    m_imageUrl.clear();
    m_packageId.clear();
    m_requestActive = false;

    emit imageUrlChanged();
    emit isLoadingChanged();

    clearImage();
}

void PreviewImageItem::setImageUrl( QUrl url)
{
    if (m_imageUrl == url)
        return;

    m_imageUrl = url;
    m_urlsToTry.clear();
    m_downloadRetryCount = 0;

    if (m_imageUrl.isEmpty()) {
        clear();
    } else {
        // deferred since packageId may also change at the same time,
        // and this impacts URL resolution
        QTimer::singleShot(0, this, [this]() {
            computeUrls();
            startDownload();
        });
    }

    emit imageUrlChanged();
}

void PreviewImageItem::computeUrls()
{
    m_urlsToTry.clear();
    if (m_imageUrl.isEmpty())
        return;

    if (m_imageUrl.isRelative()) {
        auto pkg = globals->packageRoot()->getPackageById(m_packageId.toStdString());
        if (!pkg) {
            return;
        }

        auto urls = pkg->catalog()->resolveUrl(m_imageUrl.toString().toStdString());
        for (const auto& u : urls) {
            m_urlsToTry.push_back(QString::fromStdString(u));
        }
    } else {
        m_urlsToTry.append(m_imageUrl);
    }
}

void PreviewImageItem::startDownload()
{
    if (m_urlsToTry.isEmpty())
        return;

    QNetworkRequest request(m_urlsToTry.front());
    QNetworkReply* reply = global_previewNetAccess->get(request);
    connect(reply, &QNetworkReply::finished, this, &PreviewImageItem::onFinished);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(reply, &QNetworkReply::errorOccurred,
            this, &PreviewImageItem::onDownloadError);
#else
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(onDownloadError(QNetworkReply::NetworkError)));
#endif
    m_requestActive = true;
    emit isLoadingChanged();
}

void PreviewImageItem::setImage(QImage image)
{
    m_image = image;
    m_imageDirty = true;
    setImplicitSize(m_image.width(), m_image.height());
    emit sourceSizeChanged();
    update();
}

void PreviewImageItem::onFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    reply->deleteLater();

    if (m_urlsToTry.empty() || (reply->url() != m_urlsToTry.front())) {
        // if replies arrive out of order, don't trample the correct one
        return;
    }

    QImage img;
    if (!img.load(reply, nullptr)) {
        qWarning() << Q_FUNC_INFO << "failed to read image data from" << reply->url();
        return;
    }

    setImage(img);
    m_requestActive = false;
    emit isLoadingChanged();
}

void PreviewImageItem::onDownloadError(QNetworkReply::NetworkError errorCode)
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    // remove the URL that just failed; this also ensures that onFinished()
    // doesn't process this QNetworkReply further.
    m_urlsToTry.pop_front();

    if (m_urlsToTry.isEmpty()) {
        qWarning() << Q_FUNC_INFO << "download failed for" << reply->url() << "with error code" << errorCode
                   << "and no more URLs to try";
        m_requestActive = false;
        emit isLoadingChanged();
        return;
    }

    qInfo() << "Retrying download for" << reply->url() << ", next URL" << m_urlsToTry.front();
    startDownload();
}
