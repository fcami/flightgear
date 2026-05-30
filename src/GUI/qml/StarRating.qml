// SPDX-FileCopyrightText: 2026 Richard Harrison
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear 1.0

// Star rating: stars fill gold left-to-right up to 'value' (0..maximum), with a
// partial final star. 'value' is typically the average of the four aircraft
// ratings.
Item {
    id: root

    property real value: 0          // 0 .. maximum
    property int maximum: 5
    property int starSize: Style.baseFontPixelSize + 4
    property int starSpacing: 2

    // Colour specs pass straight to the colored-icon provider, which tints the
    // greyscale star masks. See QmlColoredImageProvider.
    property string filledColorSpec: "#ffb400"
    property string emptyColorSpec: Style.inactiveThemeColor

    readonly property real __fraction: Math.max(0, Math.min(value, maximum)) / maximum

    implicitWidth: (maximum * starSize) + ((maximum - 1) * starSpacing)
    implicitHeight: starSize

    // Base: outline stars.
    Row {
        id: emptyRow
        spacing: root.starSpacing
        Repeater {
            model: root.maximum
            delegate: Image {
                width: root.starSize
                height: root.starSize
                smooth: true
                fillMode: Image.PreserveAspectFit
                source: "image://colored-icon/star-outline?" + root.emptyColorSpec
            }
        }
    }

    // Overlay: gold stars clipped to the value fraction, giving the partial
    // final star.
    Item {
        height: root.height
        width: root.implicitWidth * root.__fraction
        clip: true

        Row {
            spacing: root.starSpacing
            Repeater {
                model: root.maximum
                delegate: Image {
                    width: root.starSize
                    height: root.starSize
                    smooth: true
                    fillMode: Image.PreserveAspectFit
                    source: "image://colored-icon/star-filled?" + root.filledColorSpec
                }
            }
        }
    }
}
