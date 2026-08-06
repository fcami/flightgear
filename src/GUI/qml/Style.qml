// SPDX-FileCopyrightText: 2017 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

pragma Singleton
import QtQml 2.0

QtObject
{
    readonly property int margin: 8
    readonly property int roundRadius: 6

    readonly property int inset: 20
    readonly property int strutSize: 50

    readonly property string frameColor: "#68A6E1"
    readonly property string minorFrameColor: "#9f9f9f"

    readonly property string backgroundColor: "#ffffff"
    readonly property string themeColor: "#1b7ad3"
    readonly property string destructiveActionColor: "#c62703"
    readonly property string panelBackground: "#efefef"

    readonly property string activeColor: Qt.darker(themeColor)
    readonly property string inverseActiveColor: "#2f2f2f"

    readonly property string inactiveThemeColor: "#9f9f9f"
    readonly property string disabledThemeColor: disabledTextColor
    readonly property string disabledMinorFrameColor: "#afafaf"

    readonly property string noteFrameColor: "#fe6600"
    readonly property string noteBackgroundColor: "#f6a773"

    // one color per aircraft tag category/group, in the same order as
    // AircraftTagsModel's (C++) knownGroups: role, manufacturer, era,
    // feature, propulsion, sim-feature. Read by
    // AircraftTagsModel::loadStyleColors() for the ColorRole.
    readonly property var tagCategoryColors: [
        "#1b7ad3", // role
        "#8e44ad", // manufacturer
        "#16a085", // era
        "#e67e22", // feature
        "#2c3e50", // propulsion
        "#27ae60"  // sim-feature
    ]

    readonly property string baseTextColor: "#2f2f2f"
    readonly property string themeContrastTextColor: "#efefef"
    readonly property string themeContrastLinkColor: baseTextColor

    readonly property int baseFontPixelSize: 12
    readonly property int subHeadingFontPixelSize: 14
    readonly property int headingFontPixelSize: 18
    readonly property int smallFontPixelSize: baseFontPixelSize - 2

    readonly property string disabledTextColor: "#6f6f6f"

    readonly property double panelOpacity: 0.7

    readonly property int menuItemHeight: baseFontPixelSize + (margin * 2)
}
