// SPDX-FileCopyrightText: 2018 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear 1.0

Slider {
    property int ratingIndex: 0

    min: 0
    max: 5

    value: _launcher.currentAircraftModel.ratings[ratingIndex]
    width: aircraftFilterPanel.width - Style.strutSize
    anchors.horizontalCenter: parent.horizontalCenter
    sliderWidth: width / 2

    onValueChanged: {
        _launcher.currentAircraftModel.ratings[ratingIndex] = value
        _launcher.currentAircraftModel.saveCompatibilityAndRatingsSettings();
    }
}
