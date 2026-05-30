// SPDX-FileCopyrightText: 2026 Richard Harrison
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear 1.0

// Hover tooltip with an opaque panel, so it stays legible where it overhangs
// other content such as the toolbar tab labels.
Rectangle {
    id: root

    property alias text: label.text
    readonly property int padding: Style.margin

    width: label.implicitWidth + (padding * 2)
    height: label.implicitHeight + (padding * 2)
    color: Style.backgroundColor
    border.width: 1
    border.color: Style.minorFrameColor
    radius: Style.roundRadius / 2
    z: 1000

    StyledText {
        id: label
        anchors.centerIn: parent
    }
}
