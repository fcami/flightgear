// SPDX-FileCopyrightText: 2017 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear 1.0

Rectangle {
    id: root

    property alias text: buttonText.text
    property string iconName: ""
    property bool showLabel: true
    property bool active: false
    property bool enabled: true

    signal clicked

    // the width this button would occupy with its label shown, regardless of
    // the current showLabel state. The toolbar uses the sum of these to decide
    // when there is no longer room for labels (kept independent of showLabel so
    // there is no feedback loop).
    readonly property real fullWidth: (iconImage.visible ? iconImage.width + content.spacing : 0)
                                      + buttonText.implicitWidth + (radius * 2)

    width: content.width + (radius * 2)
    height: content.height + (radius * 2)
    radius: 6

    color: mouse.containsMouse ? Style.activeColor : (active ? Style.themeColor : Style.backgroundColor)

    readonly property string __iconColorSpec: (active || mouse.containsMouse) ? "themeContrast" : "text"

    Row {
        id: content
        anchors.centerIn: parent
        spacing: Style.margin / 2

        Image {
            id: iconImage
            visible: root.iconName !== ""
            width: visible ? buttonText.implicitHeight : 0
            height: width
            anchors.verticalCenter: parent.verticalCenter
            smooth: true
            fillMode: Image.PreserveAspectFit
            source: visible ? ("image://colored-icon/" + root.iconName + "?" + root.__iconColorSpec) : ""
        }

        StyledText {
            id: buttonText
            anchors.verticalCenter: parent.verticalCenter
            visible: root.showLabel
            width: visible ? implicitWidth : 0
            color: root.enabled ? ((active || mouse.containsMouse) ? Style.themeContrastTextColor : Style.baseTextColor)
                                : Style.disabledTextColor
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: root.enabled
        enabled: root.enabled

        onClicked: {
            root.clicked();
        }
    }

    // when the label is hidden (icon-only), reveal the name on hover so the
    // icons remain discoverable
    Tooltip {
        anchors.top: root.bottom
        anchors.topMargin: Style.margin / 2
        anchors.horizontalCenter: root.horizontalCenter
        visible: !root.showLabel && mouse.containsMouse && (root.text !== "")
        text: root.text
    }
}
