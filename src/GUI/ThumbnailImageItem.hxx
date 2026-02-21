// SPDX-FileCopyrightText: 2018 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QQuickItem>
#include <QUrl>
#include <QImage>

#include <simgear/package/PackageCommon.hxx>

class ThumbnailImageItem : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QString aircraftUri READ aircraftUri WRITE setAircraftUri NOTIFY aircraftUriChanged)
    // Q_PROPERTY(QUrl url READ url NOTIFY aircraftUriChanged)

    Q_PROPERTY(QSize sourceSize READ sourceSize NOTIFY sourceSizeChanged)

    Q_PROPERTY(QSize maximumSize READ maximumSize WRITE setMaximumSize NOTIFY maximumSizeChanged)
public:
    ThumbnailImageItem(QQuickItem* parent = nullptr);
    ~ThumbnailImageItem();

    QSGNode* updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

    QUrl url() const;

    QString aircraftUri() const;

    QSize sourceSize() const;

    QSize maximumSize() const;

signals:
    void aircraftUriChanged();

    void sourceSizeChanged();

    void maximumSizeChanged(QSize maximumSize);

public slots:

    void setAircraftUri(QString uri);

    void setMaximumSize(QSize maximumSize);

private:
    class ThumbnailPackageDelegate;
    friend class ThumbnailPackageDelegate;

    void setImage(QImage image);
    void clearImage();

    std::string packageId() const;

    QString m_aircraftUri;
    std::unique_ptr<ThumbnailPackageDelegate> m_delegate;

    QUrl m_imageUrl;
    bool m_imageDirty = false;
    QImage m_image;
    QSize m_maximumSize;
};
