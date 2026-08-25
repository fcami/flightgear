// SPDX-FileCopyrightText: Copyright (C) 2021 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear 1.0

Item {
    id: root
    signal clicked

    width: image.width + Style.margin
    height: image.height + Style.margin

    Image {
        id: image
        anchors.centerIn: parent
        source:  mouse.containsMouse ? "image://colored-icon/back?active" : "image://colored-icon/back"
    }

    MouseArea {
        id: mouse
        hoverEnabled: true
        anchors.fill: parent

        onClicked: {
            root.clicked();
        }
    }
}
