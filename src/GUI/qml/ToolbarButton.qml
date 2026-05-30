// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-FileCopyrightText: 2026 Richard Harrison
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Derived from GridToggleButton.qml (James Turner).

import QtQuick 2.0
import FlightGear 1.0

// Square icon button for the aircraft toolbar: a themed rounded icon with a
// hover tooltip. 'active' lights the icon (for on/off filters); 'tooltip' shows
// on hover.
Rectangle {
    id: root

    radius: Style.roundRadius
    border.width: 1
    border.color: Style.themeColor
    width: height
    height: Style.baseFontPixelSize + Style.margin * 2
    color: mouse.containsMouse ? Style.minorFrameColor : Style.backgroundColor
    // Lift the hovered button and its tooltip above neighbouring toolbar items.
    z: mouse.containsMouse ? 10 : 0

    property string iconName: ""
    property bool active: false
    property string tooltip: ""

    // Colour specs for the colored-icon provider: a named colour ("theme"/"text")
    // or a "#rrggbb" literal. The defaults give the standard lit/muted behaviour.
    // Override for a state colour such as a RAG indicator; a custom colour then
    // suppresses the hover-lighten so the state colour holds.
    property string activeColorSpec: "theme"
    property string inactiveColorSpec: "text"

    signal clicked();

    readonly property string __colorSpec: {
        if (root.active)
            return root.activeColorSpec;
        if (mouse.containsMouse && (root.inactiveColorSpec === "text"))
            return "theme";
        return root.inactiveColorSpec;
    }

    Image {
        id: icon
        width: parent.width - Style.margin
        height: parent.height - Style.margin
        anchors.centerIn: parent

        source: "image://colored-icon/" + root.iconName + "?" + root.__colorSpec
    }

    MouseArea {
        id: mouse
        hoverEnabled: true
        anchors.fill: parent
        onClicked: root.clicked();
    }

    Tooltip {
        anchors.left: root.right
        anchors.leftMargin: Style.margin
        anchors.verticalCenter: root.verticalCenter
        visible: mouse.containsMouse && (root.tooltip !== "")
        text: root.tooltip
    }
}
