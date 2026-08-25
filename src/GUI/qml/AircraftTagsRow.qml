// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear 1.0

// Displays an aircraft's own tags as compact lozenges next to a tag-outline
// icon, wrapping via Flow. Hides itself entirely when there are no tags.
Item {
    id: root

    property var tags: []

    height: tagsFlow.height
    visible: (root.tags !== undefined) && (root.tags.length > 0)

    Image {
        id: tagsIcon
        source: "image://colored-icon/tag-outline?text"
        anchors.left: parent.left
        anchors.top: parent.top
    }

    Flow {
        id: tagsFlow
        anchors {
            left: tagsIcon.right
            leftMargin: Style.margin
            right: parent.right
        }
        spacing: Style.margin / 2

        Repeater {
            model: root.tags
            delegate: TagLozenge {
                compact: true
                tag: modelData
            }
        }
    }
}
