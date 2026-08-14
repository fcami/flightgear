// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import QtQuick.Controls 2.2
import FlightGear 1.0

// A pill-shaped chip representing one selected aircraft-list filter tag.
// Double-click toggles negation ("!tag"); the small 'x' removes the tag
// (hidden in 'compact' mode, used for the inline unfocused display).
Rectangle {
    id: root

    property string tag: ""
    property bool negated: false
    property bool compact: false

    signal removeRequested()
    signal toggleNegationRequested()
    signal clicked()

    readonly property string __name: _launcher.aircraftTagsModel.nameForTag(root.tag)
    readonly property string __description: _launcher.aircraftTagsModel.descriptionForTag(root.tag)

    height: label.implicitHeight + Style.margin
    width: label.implicitWidth + (root.compact ? (Style.margin * 2) : (closeIcon.width + Style.margin * 2.5))
    radius: height / 2
    color: root.negated ? Style.destructiveActionColor : _launcher.aircraftTagsModel.colorForTag(root.tag)

    ToolTip.visible: hoverArea.containsMouse && (root.__description.length > 0)
    ToolTip.text: root.__description
    ToolTip.delay: 500

    Text {
        id: label
        text: root.negated ? ("! " + root.__name) : root.__name
        color: Style.themeContrastTextColor
        font.pixelSize: Style.baseFontPixelSize
        anchors {
            left: parent.left
            leftMargin: Style.margin
            verticalCenter: parent.verticalCenter
        }
    }

    Text {
        id: closeIcon
        visible: !root.compact
        text: "X"
        color: Style.themeContrastTextColor
        font.pixelSize: Style.baseFontPixelSize
        anchors {
            right: parent.right
            rightMargin: Style.margin * 0.5
            verticalCenter: parent.verticalCenter
        }
    }

    // covers the whole lozenge: click selects, double-click cycles negation
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.clicked()
        onDoubleClicked: root.toggleNegationRequested()
    }

    // on top of hoverArea, so a single click here removes the tag
    MouseArea {
        enabled: closeIcon.visible
        anchors.fill: closeIcon
        anchors.margins: -Style.margin * 0.5
        onClicked: root.removeRequested()
    }
}
