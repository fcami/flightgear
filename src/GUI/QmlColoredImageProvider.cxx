// SPDX-FileCopyrightText: 2021 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "QmlColoredImageProvider.hxx"

#include <QDebug>
#include <QQmlComponent>
#include <QQmlEngine>

static QmlColoredImageProvider* static_instance = nullptr;

QmlColoredImageProvider::QmlColoredImageProvider() : QQuickImageProvider(QQmlImageProviderBase::Image)
{
    static_instance = this;
}

void QmlColoredImageProvider::loadStyleColors(QQmlEngine* engine, int styleTypeId)
{
    QJSValue styleValue = engine->singletonInstance<QJSValue>(styleTypeId);
    if (styleValue.isNull() || !styleValue.isQObject()) {
        qWarning() << "Unable to load Style object";
        return;
    }

    QObject* styleObject = styleValue.toQObject();
    _themeColor = QColor{styleObject->property("themeColor").toString()};
    _disabledColor = QColor{styleObject->property("disabledThemeColor").toString()};
    _textColor = QColor{styleObject->property("baseTextColor").toString()};
    _themeContrastColor = QColor{styleObject->property("themeContrastTextColor").toString()};
    _activeColor = QColor{styleObject->property("activeColor").toString()};
    _destructiveColor = QColor{styleObject->property("destructiveActionColor").toString()};
}

QImage QmlColoredImageProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    QString path = ":/icon/" + id;
    QColor c = _themeColor;

    auto queryPos = id.indexOf('?');
    if (queryPos >= 0) {
        path = ":/icon/" + id.left(queryPos); // without the query part
        const QString q = id.mid(queryPos + 1);
        if (q.startsWith('#')) {
            c = QColor(q); // allow directly specifying a color
        } else if (q == "text") {
            c = _textColor;
        } else if (q == "themeContrast") {
            c = _themeContrastColor;
        } else if (q == "active") {
            c = _activeColor;
        } else if (q == "destructive") {
            c = _destructiveColor;
        } else if (q == "theme") {
            // default already
        } else if (q == "disabled") {
            c = _disabledColor;
        } else {
            qWarning() << Q_FUNC_INFO << "Unrecognized color specification:" << id;
        }
    }

    QImage originalImage = QImage{path};
    if (originalImage.isNull()) {
        qWarning() << Q_FUNC_INFO << "Failed to load image:" << path;
        return {};
    }

    if (!originalImage.isGrayscale()) {
        qWarning() << Q_FUNC_INFO << "Source image is not a greyscale mask:" << path;
    }

    const int baseRed = c.red();
    const int baseGreen = c.green();
    const int baseBlue = c.blue();
    *size = originalImage.size();

    // colorize it
    QImage colored{originalImage.size(), QImage::Format_ARGB32_Premultiplied};
    const int width = size->width();
    const int height = size->height();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // this is 0..255 ranged alpha/intensity value
            const int alpha = originalImage.pixel(x, y);
            colored.setPixel(x, y, qPremultiply(qRgba(baseRed, baseGreen, baseBlue, alpha)));
        }
    }

    return colored;
}

QPixmap QmlColoredImageProvider::requestPixmap(const QString& id, QSize* size, const QSize& requestedSize)
{
    QImage img = requestImage(id, size, requestedSize);
    return QPixmap::fromImage(img);
}

QmlColoredImageProvider* QmlColoredImageProvider::instance()
{
    return static_instance;
}
