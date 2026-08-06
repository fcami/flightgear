// LauncherMainWindow.hxx - GUI launcher dialog using Qt5
//
// SPDX-FileCopyrightText: 2015 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QScopedPointer>
#include <QStringList>
#include <QModelIndex>
#include <QTimer>
#include <QUrl>
#include <QQuickView>


class QModelIndex;
class QQmlEngine;
class LaunchConfig;
class ViewCommandLinePage;
class QQuickItem;
class LauncherController;
class QmlColoredImageProvider;

class LauncherMainWindow : public QQuickView
{
    Q_OBJECT
public:
    LauncherMainWindow(bool inSimMode);
    virtual ~LauncherMainWindow();

    bool execInApp();

    bool wasRejected();

    bool event(QEvent *event) override;

private slots:
    void onQuickStatusChanged(QQuickView::Status status);
    void renderTheadSceneGraphInitialized();

private:
    bool checkQQC2Availability();

    LauncherController* m_controller;
    QmlColoredImageProvider* m_coloredIconProvider;
};
