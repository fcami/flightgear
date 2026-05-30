// SPDX-FileCopyrightText: 2018 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear 1.0

Slider {
    property var ratings: []
    property int ratingIndex: 0

    min: 0
    max: 5

    value: ratings[ratingIndex]
    width: parent.width - Style.strutSize
    anchors.horizontalCenter: parent.horizontalCenter
    sliderWidth: width / 2

    onValueChanged: {
         ratings[ratingIndex] = value
        _launcher.browseAircraftModel.saveCompatibilityAndRatingsSettings();
    }
}
