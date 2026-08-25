// SPDX-FileCopyrightText: Copyright (C) 2018 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear.Launcher 1.0
import FlightGear 1.0

Item {
    id: root

    property var model: undefined
    property string displayRole: "display"

    implicitHeight: button.height
    implicitWidth: button.width

    signal selected(var index);

    Rectangle {
        id: button

        width: icon.width
        height: icon.height
        radius: Style.roundRadius

        color: enabled ? (mouse.containsMouse ? Style.activeColor : Style.themeColor) : Style.disabledThemeColor

        Image {
            id: icon
            source: "image://colored-icon/history?themeContrast"
            anchors.centerIn: parent
        }

        MouseArea {
            anchors.fill: parent
            id: mouse
            hoverEnabled: root.enabled
            enabled: root.enabled
            onClicked: {
                  OverlayShared.globalOverlay.showOverlayAtItemOffset(menu, root,
                                                                Qt.point(root.width, root.height))
            }
        }
    }

    Component {
        id: menu
        OverlayMenu {
            model: root.model
            onSelect: root.selected(index)
            alignRightEdge: true
        }
    }
}
