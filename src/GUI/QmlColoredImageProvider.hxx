// SPDX-FileCopyrightText: 2021 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QColor>
#include <QQuickImageProvider>

class QQmlEngine;

/**
 * @brief Helper image provider to allow re-colorizing
 * images based on the active style
 *
 */
class QmlColoredImageProvider : public QQuickImageProvider
{
public:
    QmlColoredImageProvider();

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

    QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override;

    void loadStyleColors(QQmlEngine* engine, int styleTypeId);

    static QmlColoredImageProvider* instance();

private:
    QColor _themeColor, _textColor, _themeContrastColor, _activeColor;
    QColor _destructiveColor, _disabledColor;
};
