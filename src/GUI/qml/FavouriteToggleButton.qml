// SPDX-FileCopyrightText: 2021 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear 1.0

Item {
    id: root
    property bool checked: false

    implicitWidth: icon.width + Style.margin
    implicitHeight: icon.height + Style.margin

    signal toggle(var on);


    Image {
        id: icon
        source: {
            var b = root.checked;
            if (mouse.containsMouse && !b) {
                return "image://colored-icon/heart-plus?theme"
            }
            return b ? "image://colored-icon/heart-filled?theme" : "image://colored-icon/heart-outline?text";
        }

        anchors.centerIn: parent
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.toggle(!root.checked);

        cursorShape: Qt.PointingHandCursor
    }
}
