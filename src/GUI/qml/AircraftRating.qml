// SPDX-FileCopyrightText: 2018 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear 1.0

Item {
    id: root

    property string title: ""
    property double value: 3

    property alias showValue: ratingValue.visible

    implicitWidth: childrenRect.width
    implicitHeight: Math.max(label.height, 24)

    Row {
        id: contentRow
        spacing: Style.margin

        StyledText {
            id: label
            anchors.verticalCenter: parent.verticalCenter

            horizontalAlignment: Text.AlignRight
            text: root.title + ":"
        }

        Row {
            id: ratingRow

            spacing: 0 // keep this really tight
            anchors.verticalCenter: parent.verticalCenter

            Repeater {
                model: 5

                delegate: Image {
                    source: {
                        var b = (model.index + 1) <= root.value;
                        return b ? "image://colored-icon/star-filled?theme" : "image://colored-icon/star-outline?text";
                    }
                }
            }
        }

        StyledText {
            id: ratingValue
            visible: false // hidden by default
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignLeft
            text: "(" + Number(root.value).toLocaleString(null, 'f', 2) + ")"
            width: Style.strutSize
        }
    }
}
