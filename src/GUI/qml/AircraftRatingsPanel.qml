// SPDX-FileCopyrightText: 2018 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.2
import FlightGear.Launcher 1.0 as FG
import FlightGear 1.0

Rectangle
{
    id: aircraftFilterPanel
    signal clearSelection();

    property GettingStartedTipLayer tips

    implicitHeight: contents.implicitHeight + Style.margin * 2
    implicitWidth: Style.strutSize * 24
    color: Style.backgroundColor

    Column {
        id: contents
        y: Style.margin
        spacing: (Style.margin * 2)

        ToggleSwitch {
            id: useGridMode
            checked: _launcher.aircraftGridMode

            onCheckedChanged: {
                _launcher.aircraftGridMode = checked
                _launcher.saveUISetting("aircraft-grid-mode", checked);
            }

            label: checked ? qsTr("Display aircraft in a grid") : qsTr("Display aircraft in a list")
            width: parent.width - Style.inset
            anchors.horizontalCenter: parent.horizontalCenter
        }

        PopupChoice {
            id: sortChoice
            model: StringListModel{
                values: [qsTr("By name"), qsTr("By rating"), qsTr("By user rating")]
                keys: [FG.AircraftFilterModel.SortByName, FG.AircraftFilterModel.SortByRating, FG.AircraftFilterModel.SortByUserRating]
            }

            currentIndex: model.indexOfKey(_launcher.currentAircraftModel.sorting)
            onCurrentIndexChanged: {
                _launcher.currentAircraftModel.sorting = model.keys[currentIndex]
            }

            label: qsTr("Sort aircraft by: ")
            width: parent.width - Style.inset
            anchors.horizontalCenter: parent.horizontalCenter
        }

        ToggleSwitch {
            id: doCompatibilityCheck
            checked: _launcher.currentAircraftModel.compatibilityFilterEnabled

            onCheckedChanged: {
                _launcher.currentAircraftModel.compatibilityFilterEnabled = checked
                _launcher.saveUISetting("enable-compatibility-filter", checked);
            }

            label: qsTr("Only display compatible aircraft")
            width: parent.width - Style.inset
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Rectangle {
            color: Style.frameColor
            height: 1
            width: parent.width - Style.inset
            anchors.horizontalCenter: parent.horizontalCenter
        }

        ToggleSwitch {
            id: doFilterCheck
            checked: _launcher.currentAircraftModel.ratingsFilterEnabled

            onCheckedChanged: {
                _launcher.currentAircraftModel.ratingsFilterEnabled = checked
                _launcher.saveUISetting("enable-ratings-filter", checked);
            }

            label: qsTr("Filter aircraft")
            width: parent.width - Style.inset
            anchors.horizontalCenter: parent.horizontalCenter
        }

        StyledText {
            text: qsTr("Aircraft are rated by the community based on four criteria, on a scale from " +
                        "one to five. The ratings are designed to help make an informed guess how "+
                        "complete and functional an aircraft is.")
            width: parent.width - Style.inset
            wrapMode: Text.WordWrap
            anchors.horizontalCenter: parent.horizontalCenter
        }

        RatingSlider {
            label: qsTr("Minimum flight-model (FDM) rating")
            ratingIndex: 0
            enabled: _launcher.currentAircraftModel.ratingsFilterEnabled
        }

        RatingSlider {
            label: qsTr("Minimum systems rating")
            ratingIndex: 1
            enabled: _launcher.currentAircraftModel.ratingsFilterEnabled
        }

        RatingSlider {
            label: qsTr("Minimum cockpit visual rating")
            ratingIndex: 2
            enabled: _launcher.currentAircraftModel.ratingsFilterEnabled
        }

        RatingSlider {
            label: qsTr("Minimum external visual model rating")
            ratingIndex: 3
            enabled: _launcher.currentAircraftModel.ratingsFilterEnabled
        }

        Rectangle {
            color: Style.frameColor
            height: 1
            width: parent.width - Style.inset
            anchors.horizontalCenter: parent.horizontalCenter
        }

    } // of Column

}
