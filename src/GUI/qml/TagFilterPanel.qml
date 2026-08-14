// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import QtQuick.Controls 2.2
import FlightGear 1.0
import FlightGear.Launcher 1.0

// Extended area shown below the aircraft search box while it has focus.
// Initially shows the currently selected tags (regular-mode lozenges, with
// an 'x' to remove and double-click to negate) plus a '+' lozenge; clicking
// '+' reveals the full, filterable list of every known tag (flat list;
// grouping by category is left for a follow-up) so another one can be
// added. Filtering the list uses the search box's own text (passed in via
// filterText) rather than a separate search field. Selecting a tag does not
// close this panel, so several can be added in a row; the search box is
// responsible for dismissing it (focus loss / click outside).
Rectangle {
    id: root

    signal selected(string tag)
    signal removeRequested(string tag)
    signal toggleNegationRequested(string tag)

    property string filterText: ""
    property var selectedTags: [] // [{tag, negated}, ...]

    property bool showingAllTags: false

    property bool dismissOnClickOutside: true
    property int maximumPermittedHeight: 350

    color: Style.panelBackground
    border.width: 1
    border.color: Style.minorFrameColor
    height: outerColumn.implicitHeight + (Style.margin * 2)
    clip: true

    y: -OverlayShared.globalOverlay.verticalOverflowFor(height)

    function doSelect(tag)
    {
        root.selected(tag);
    }

    Column {
        id: outerColumn
        x: Style.margin
        y: Style.margin
        width: root.width - (Style.margin * 2)
        spacing: Style.margin

        StyledText {
            width: parent.width
            wrapMode: Text.WordWrap
            color: Style.disabledTextColor
            text: root.selectedTags.length > 0 ? qsTr("Filtering by tags:")
                                                : qsTr("Enter a search term above, or '[' to search tags. Click + to browse tags.")
        }

        Flow {
            width: parent.width
            spacing: Style.margin

            Repeater {
                model: root.selectedTags
                delegate: TagLozenge {
                    tag: modelData.tag
                    negated: modelData.negated
                    onRemoveRequested: root.removeRequested(modelData.tag)
                    onToggleNegationRequested: root.toggleNegationRequested(modelData.tag)
                }
            }

            AddTagLozenge {
                onClicked: root.showingAllTags = true
            }
        }

        Rectangle {
            visible: root.showingAllTags
            width: parent.width
            height: 1
            color: Style.minorFrameColor
        }

        AircraftTagsFilterModel {
            id: filteredTagsModel
            sourceModel: _launcher.aircraftTagsModel
            filterText: root.filterText
            excludedTags: root.selectedTags.map(function (t) { return t.tag; })
        }

        Flickable {
            id: flick
            visible: root.showingAllTags
            width: parent.width
            height: root.showingAllTags ? Math.min(itemsFlow.childrenRect.height, root.maximumPermittedHeight) : 0
            contentHeight: itemsFlow.childrenRect.height
            clip: true
            ScrollBar.vertical: ScrollBar {}

            Flow {
                id: itemsFlow
                width: flick.width
                spacing: Style.margin / 2

                Repeater {
                    id: tagsRepeater
                    model: filteredTagsModel
                    delegate: TagLozenge {
                        readonly property string tagId: model.tag
                        compact: true
                        tag: model.tag
                        onClicked: root.doSelect(tagId)
                    }
                }
            }
        }

        StyledText {
            width: parent.width
            visible: root.showingAllTags && (tagsRepeater.count === 0)
            color: Style.disabledTextColor
            text: qsTr("No matching tags")
        }
    }
}
