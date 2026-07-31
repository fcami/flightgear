// SPDX-FileCopyrightText: 2026 Richard Harrison
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear 1.0

// The four-axis ratings filter, drawn as four stars (FDM, systems, cockpit,
// exterior). Each star fills vertically to its minimum-rating threshold, and
// greys out when the filter is off. The vertical fill is deliberate: it sets
// this apart from the per-aircraft horizontal rating.
Row {
    id: root

    // the four minimum-rating thresholds, each 0..maximumRating
    property var thresholds: [0, 0, 0, 0]
    property int maximumRating: 5
    property bool active: true
    property int starSize: Style.baseFontPixelSize + 2

    // Hand-tuned fill fraction per rating, indexed by threshold (0..maximumRating).
    // The scale is non-linear so the clustered high end separates: a linear fill
    // renders 4 and 5 almost identically.
    readonly property var __fillLevels: [0.0, 0.25, 0.35, 0.4, 0.6, 1.0]

    spacing: 2

    readonly property string __filledSpec: active ? "#ffb400" : Style.inactiveThemeColor

    Repeater {
        model: 4

        delegate: Item {
            width: root.starSize
            height: root.starSize

            readonly property real __fraction: {
                var v = root.thresholds[index];
                if (v === undefined) return 0;
                var clamped = Math.max(0, Math.min(Math.round(v), root.maximumRating));
                return root.__fillLevels[clamped];
            }

            // Base: outline star.
            Image {
                anchors.fill: parent
                smooth: true
                fillMode: Image.PreserveAspectFit
                source: "image://colored-icon/star-outline?" + Style.inactiveThemeColor
            }

            // Overlay: gold fill, clipped from the bottom up to the threshold.
            Item {
                width: parent.width
                height: parent.height * parent.__fraction
                anchors.bottom: parent.bottom
                clip: true

                Image {
                    width: parent.width
                    height: root.starSize           // full star height
                    anchors.bottom: parent.bottom   // bottom-aligned; the clip reveals the lower part
                    smooth: true
                    fillMode: Image.PreserveAspectFit
                    source: "image://colored-icon/star-filled?" + root.__filledSpec
                }
            }
        }
    }
}
