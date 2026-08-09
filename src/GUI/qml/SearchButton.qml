// SPDX-FileCopyrightText: 2018 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.2
import FlightGear 1.0

FocusScope
{
    id: root

    width: Style.strutSize * 6
    height: frame.height

    property string placeholder: qsTr("Search")
    property bool active: false

    // when true, show the selected aircraft-tag lozenges in the box, and an
    // extended tag-suggestions area below it while the box has focus
    property bool showTagFilter: false

    signal search(string term)

    property bool autoSubmit: true
    property alias autoSubmitTimeout: searchTimer.interval

    onActiveChanged: {
        if (!active) {
            clear();
        }
    }

    function clear()
    {
        buttonText.text = ""
        root.focus = false
        searchTimer.stop();
        root.search("");
    }

    readonly property bool canClear: (buttonText.text.length > 0)

    // set immediately before each showTagSuggestions() call, to control
    // whether the panel opens straight into its 'browse all tags' mode
    property bool __startTagPanelInBrowseMode: false

    function showTagSuggestions(startInBrowseMode)
    {
        if (!root.showTagFilter)
            return;
        root.__startTagPanelInBrowseMode = !!startInBrowseMode;
        OverlayShared.globalOverlay.showOverlayAtItemOffset(tagSuggestionsComponent, frame,
                                                             Qt.point(0, frame.height))
    }

    Component {
        id: tagSuggestionsComponent
        TagFilterPanel {
            width: frame.width
            filterText: buttonText.text
            selectedTags: _launcher.aircraftSearchModel.selectedTags
            showingAllTags: root.__startTagPanelInBrowseMode
            onSelected: function(tag) {
                _launcher.aircraftSearchModel.addTag(tag);
                buttonText.text = "";
            }
            onRemoveRequested: function(tag) {
                _launcher.aircraftSearchModel.removeTag(tag);
            }
            onToggleNegationRequested: function(tag) {
                _launcher.aircraftSearchModel.toggleTagNegation(tag);
            }
        }
    }

    Rectangle
    {
        id: frame
        radius: Style.roundRadius

        width: root.width
        height: Math.max(searchIcon.height, buttonText.height) + (Style.roundRadius)
        border.width: 1
        border.color: (mouse.containsMouse | active) ? Style.themeColor: Style.minorFrameColor
        clip: true
        color: Style.backgroundColor

        TextInput {
            id: buttonText
            anchors.left: parent.left
            anchors.right: tagFilterLoader.left
            anchors.margins: Style.margin
            anchors.verticalCenter: parent.verticalCenter

            color: Style.baseTextColor
            focus: true
            font.pixelSize: Style.baseFontPixelSize

            onTextChanged: {
                if (root.autoSubmit) {
                    searchTimer.restart();
                }
            }

            onEditingFinished: {
                if (text == "") {
                    clear();
                } else {
                    root.search(text);
                    root.focus = false;
                }
            }

            text: ""

            onActiveFocusChanged: {
                if (!root.showTagFilter)
                    return;
                if (activeFocus) {
                    root.showTagSuggestions();
                } else {
                    OverlayShared.globalOverlay.dismissOverlay();
                }
            }

            // placeholder text, hides itself whenever parent has non-empty text
            StyledText {
                anchors.fill: parent
                visible: parent.text == ""
                text: root.placeholder

                // weird rule here - we want to make the placeholder text very
                // subtle when the textdit is focused, so the user knows it will
                // be overwritten
                color: buttonText.activeFocus ? Style.disabledTextColor : Style.baseTextColor
            }
        }

        Image {
            id: searchIcon

            // give hover feedback when showing the clear icon
            source: clearButtonMouse.containsMouse ? "image://colored-icon/clear?destructive"
                : root.canClear ? "image://colored-icon/clear?text" : "image://colored-icon/search?text"

            anchors.right: parent.right
            anchors.rightMargin: Style.margin
            anchors.verticalCenter: parent.verticalCenter
        }

        // tag status icon: outline when no tags are active, theme-colored
        // filled icon when at least one is; click opens the tag panel
        // straight into 'browse all tags' mode
        Loader {
            id: tagFilterLoader
            active: root.showTagFilter
            anchors.right: searchIcon.left
            anchors.rightMargin: active ? Style.margin : 0
            anchors.verticalCenter: parent.verticalCenter

            sourceComponent: Image {
                source: (_launcher.aircraftSearchModel.selectedTags.length > 0)
                    ? "image://colored-icon/tag-filled?theme"
                    : "image://colored-icon/tag-outline?text"

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.forceActiveFocus();
                        buttonText.selectAll();
                        root.showTagSuggestions(true);
                    }
                }
            }
        }

        MouseArea {
            id: mouse
            anchors.left: parent.left
            height: parent.height
            anchors.right: root.showTagFilter ? tagFilterLoader.left : clearButtonMouse.left
            hoverEnabled: true

            onClicked: {
                if (!buttonText.activeFocus) {
                    root.forceActiveFocus();
                    buttonText.selectAll();
                } else {
                    root.showTagSuggestions();
                }
            }

            onDoubleClicked: {
                buttonText.selectAll();
            }
        }


        MouseArea {
            id: clearButtonMouse
            anchors.right: parent.right
            height: parent.height
            width: searchIcon.width
            hoverEnabled: root.canClear
            visible: root.canClear
            onClicked: clear();
        }

        Timer {
            id: searchTimer
            interval: 800
            onTriggered: {
                if (buttonText.text.length > 2) {
                    root.search(buttonText.text)
                } else if (buttonText.text.length == 0) {
                    root.search(""); // ensure we update with no search
                }
            }
        }
    }
} // of FocusScope
