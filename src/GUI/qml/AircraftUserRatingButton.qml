// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear 1.0

// Shows the combined user rating (average + vote count) for an aircraft,
// the user's own rating if they have already rated it, or a clickable
// 1-5 star picker if the aircraft is eligible to be rated (installed and
// flown for long enough) and has not been rated yet.
Item {
    id: root

    property double average: 0
    property int votes: 0
    property int ownRating: -1 // -1 == not rated by this user
    property bool canRate: false

    signal rateRequested(int rating)

    visible: (root.votes > 0) || (root.ownRating >= 0) || root.canRate
    implicitWidth: contentRow.width
    implicitHeight: contentRow.height

    Row {
        id: contentRow
        spacing: Style.margin * 2

        Row {
            visible: root.votes > 0
            spacing: Style.margin
            anchors.verticalCenter: parent.verticalCenter

            AircraftRating {
                title: qsTr("Users")
                value: root.average
                anchors.verticalCenter: parent.verticalCenter
            }

            StyledText {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("(%1)").arg(root.votes)
            }
        }

        StyledText {
            visible: root.ownRating >= 0
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("You rated this: %1/5").arg(root.ownRating)
        }

        Row {
            visible: root.canRate && (root.ownRating < 0)
            spacing: Style.margin
            anchors.verticalCenter: parent.verticalCenter

            StyledText {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Rate this aircraft:")
            }

            Row {
                spacing: 0
                anchors.verticalCenter: parent.verticalCenter

                Repeater {
                    model: 5

                    delegate: Image {
                        source: "image://colored-icon/star-outline?text"

                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.rateRequested(model.index + 1)
                        }
                    }
                }
            }
        }
    }
}
