// SPDX-FileCopyrightText: 2018 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.2
import FlightGear.Launcher 1.0 as FG
import FlightGear 1.0

FocusScope
{
    id: root

    property var __model: null
    property Component __header: null
    property Component __footer: null
    property string __lastState: "installed"

    function updateSelectionFromLauncher()
    {
        if (aircraftContent.item) {
            aircraftContent.item.updateSelectionFromLauncher();
        }
    }

    state: "installed"

    Component.onCompleted: {
        _launcher.currentAircraftModel.loadCompatibilityAndRatingsSettings();
        _launcher.selectedModel = root.state;

        // if the user has favourites defined, default to that tab
        if (_launcher.favouriteAircraftModel.count > 0) {
            root.state = "favourites"
            root.updateSelectionFromLauncher();
        }
    }

    onStateChanged: {
        _launcher.selectedModel = root.state
    }

    GettingStartedScope.controller: tipsLayer.controller

    Rectangle
    {
        id: tabBar
        height: searchButton.height + (Style.margin * 2)
        width: displayFilterPanel.width
        color: Style.backgroundColor
        anchors.horizontalCenter: parent.horizontalCenter
        z: 1

        GridToggleButton {
            id: settingsToggle
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: Style.margin
            onClicked: { displayFilterPanel.state = displayFilterPanel.state === "open" ? "closed" : "open" }

            GettingStartedTip {
                tipId: "aircraftSettingsTip"

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    horizontalCenterOffset: Style.margin
                    top: parent.bottom
                }
                arrow: GettingStartedTip.TopLeft
                text: qsTr("Show display & filtering controls for the aircraft list")
            }
        }

        Row {
            anchors.centerIn: parent
            spacing: Style.margin

            TabButton {
                id: installedAircraftButton
                text: qsTr("Installed Aircraft")
                onClicked: {
                    root.state = "installed"
                    root.updateSelectionFromLauncher();
                    searchButton.clear();
                }
                active: root.state == "installed"

                GettingStartedTip {
                    tipId: "installedAircraftTip"
                    nextTip: "gridModeTip"

                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: parent.bottom
                    }
                    arrow: GettingStartedTip.TopCenter
                    text: qsTr("Use this button to view installed aircraft")
                }
            }

            TabButton {
                id: favouritesButton
                text: qsTr("Favourites")
                onClicked: {
                    root.state = "favourites"
                    root.updateSelectionFromLauncher();
                }
                active: root.state == "favourites"
            }

            TabButton {
                id: browseButton
                enabled: _launcher.isNetworkAvailable

                text: qsTr("Browse")
                onClicked: {
                    root.state = "browse"
                    root.updateSelectionFromLauncher();
                    searchButton.clear();
                }
                active: root.state == "browse"

                GettingStartedTip {
                    tipId: "browseTip"
                    nextTip: "searchAircraftTip"

                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: parent.bottom
                    }
                    arrow: GettingStartedTip.TopCenter
                    text: qsTr("View available aircraft to download")
                }
            }

            TabButton {
                id: updatesButton
                enabled: _launcher.isNetworkAvailable
              //  visible: _launcher.baseAircraftModel.showUpdateAll
                text: qsTr("Updates")
                onClicked: {
                    root.state = "updates"
                    root.updateSelectionFromLauncher();
                }
                active: root.state == "updates"
            }
        } // of header row

        SearchButton {
            id: searchButton

            height: installedAircraftButton.height

            anchors.right: parent.right
            anchors.rightMargin: Style.margin
            anchors.verticalCenter: parent.verticalCenter

            onSearch: function(term) {
                _launcher.aircraftSearchModel.setSearchString(term)

                if (term == "") {
                    return; // avoid doing an unnecessary search when clearing the search box
                }

                root.state = "search"
                root.updateSelectionFromLauncher();
            }

            active: root.state == "search"

            GettingStartedTip {
                tipId: "searchAircraftTip"
                nextTip: "installedAircraftTip"

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    top: parent.bottom
                }
                arrow: GettingStartedTip.TopRight
                text: qsTr("Enter text to search aircraft names and descriptions.")
            }
        }
    }

    Rectangle {
        id: tabBarDivider
        color: Style.frameColor
        height: 1
        width: parent.width - Style.inset
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: tabBar.bottom
    }

    Component {
        id: noDefaultCatalogHeader
        NoDefaultCatalogPanel {
            width: aircraftContent.width
        }
    }

    Component {
        id: updateAllHeader
        UpdateAllPanel {
            width: aircraftContent.width
        }
    }

    Component {
        id: searchMoreFooter
        Rectangle {
            width: aircraftContent.width
            height: visible ? Style.strutSize : 0
            visible: _launcher.currentAircraftModel.filteredOutCount > 0

            // open the filter panel on click
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    displayFilterPanel.state = "open";
                }
            }

            StyledText {
                anchors.fill: parent
                text: qsTr("%1 aircraft matched the search, but were filtered out. Adjust your filter to see more aircraft. " +
                        "(Click here to open the list settings)").
                    arg(_launcher.currentAircraftModel.filteredOutCount)
                wrapMode: Text.WordWrap
                font.pixelSize: Style.headingFontPixelSize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    Component {
        id: showMoreFooter
        Rectangle {
            width: aircraftContent.width
            height: visible ? Style.strutSize : 0
            visible: _launcher.currentAircraftModel.filteredOutCount > 0

            // open the filter panel on click
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    displayFilterPanel.state = "open";
                }
            }

            StyledText {
                anchors.fill: parent
                text: qsTr("%1 aircraft are available, but were filtered out. Adjust your filter to see more aircraft. " +
                        "(Click here to open the list settings)").
                    arg(_launcher.currentAircraftModel.filteredOutCount)
                wrapMode: Text.WordWrap
                font.pixelSize: Style.headingFontPixelSize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    Component {
        id: noFavouritesHeader
        Rectangle {
            visible: _launcher.favouriteAircraftModel.count === 0
            width: aircraftContent.width
            height: visible ? Style.strutSize : 0

            StyledText {
                anchors.fill: parent
                text: qsTr("No favourite aircraft selected: install some aircraft and mark them as favourites by clicking the \u2605")
                wrapMode: Text.WordWrap
                font.pixelSize: Style.headingFontPixelSize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }


    Component {
        id: emptyHeaderFooter
        Item {
        }
    }

    Component {
        id: installMoreAircraftFooter
        Rectangle {
            visible: _launcher.baseAircraftModel.installedAircraftCount < 50
            width: aircraftContent.width
            height: Style.strutSize

            StyledText {
                anchors.fill: parent
                text: qsTr("To install additional aircraft, click on the 'Browse' tab at the top of this page. (This requires a network connection.)")
                wrapMode: Text.WordWrap
                font.pixelSize: Style.headingFontPixelSize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    Loader {
        id: aircraftContent
        // we use gridModeToggle vis to mean enabled, effectively
        source: _launcher.aircraftGridMode ? "qrc:///qml/AircraftGridView.qml"
                                           : "qrc:///qml/AircraftListView.qml"

        anchors {
            left: parent.left
            top: tabBarDivider.bottom
            bottom: parent.bottom
            right: parent.right
            topMargin: Style.margin
        }

        Binding {
            target: aircraftContent.item
            property: "model"
            value: root.__model
        }

        Binding {
            target: aircraftContent.item
            property: "header"
            value: root.__header
        }

        Binding {
            target: aircraftContent.item
            property: "footer"
            value: root.__footer
        }

        Connections {
            target: aircraftContent.item
            function onShowDetails(uri) {
                root.showDetails(uri);
            }
        }
    }

    StyledText {
        id: noUpdatesMessage
        anchors {
            left: parent.left
            top: tabBar.bottom
            bottom: parent.bottom
            right: parent.right
        }
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: Style.headingFontPixelSize
        text: qsTr("No aircraft updates available right now")
        visible: (root.state == "updates") && (_launcher.aircraftWithUpdatesModel.count == 0)
    }

    states: [
        State {
            name: "installed"
            PropertyChanges {
                target: root
                __model: _launcher.currentAircraftModel
                __header: emptyHeaderFooter
                __footer: installMoreAircraftFooter
            }

            PropertyChanges {
                target: settingsToggle; visible: true
            }
        },

        State {
            name: "search"
            PropertyChanges {
                target: root
                __model: _launcher.currentAircraftModel
                __header: emtyHeaderFooter
                __footer: searchMoreFooter
            }

            PropertyChanges {
                target: settingsToggle; visible: true
            }
        },

        State {
            name: "browse"
            PropertyChanges {
                target: root
                __model: _launcher.currentAircraftModel
                __header: _addOns.showNoOfficialHangar ? noDefaultCatalogHeader : emptyHeaderFooter
                __footer: showMoreFooter
            }

            PropertyChanges {
                target: settingsToggle; visible: true
            }
        },

        State {
            name: "updates"
            PropertyChanges {
                target: root
                __model: _launcher.aircraftWithUpdatesModel
                __header: (_launcher.aircraftWithUpdatesModel.count > 0) ? updateAllHeader : emptyHeaderFooter
                __footer: emptyHeaderFooter
            }

            PropertyChanges {
                target: settingsToggle; visible: false
            }
        },

        State {
            name: "favourites"

            PropertyChanges {
                target: root
                __model: _launcher.favouriteAircraftModel
                __header: noFavouritesHeader
                __footer: emptyHeaderFooter
            }

            PropertyChanges {
                target: settingsToggle; visible: false
            }
        }

    ]

    function showDetails(uri)
    {
        // set URI, start animation
        // change state
        detailsView.aircraftURI = uri;
        detailsView.visible = true
    }

    function goBack()
    {
        // details view can change the aircraft URI / variant
        updateSelectionFromLauncher();
        detailsView.visible = false;
    }

    Rectangle {
        anchors.fill: parent
        opacity: 0.1
        color: "black"
        z: displayFilterPanel.z
        // mouse are behind panel to consume clicks
        MouseArea {
            anchors.fill: parent
            onClicked: { displayFilterPanel.state = "closed"; }
        // accept so that clicks don't fall through to other
        // UI elements
            onPressed: (mouse)=> {mouse.accepted = true; }
        }

        visible: displayFilterPanel.state === "open"
    }

    AircraftRatingsPanel {
        id: displayFilterPanel
        width: Math.min(aircraftContent.width, displayFilterPanel.implicitWidth)
        height: displayFilterPanel.implicitHeight
        anchors.horizontalCenter: parent.horizontalCenter
        z: 1

        tips: tipsLayer
        onClearSelection: {
            _launcher.selectedAircraft = "";
            root.updateSelectionFromLauncher()
        }

        clip: true
        state: "closed"

        states: [
            State {
                name: "closed"
                PropertyChanges { target: displayFilterPanel; y:  -displayFilterPanel.height; }
            },

            State {
                name: "open"
                PropertyChanges { target: displayFilterPanel; y: 0}
            }
        ]

        transitions: Transition {
            PropertyAnimation { duration: 200; properties: "y"; easing.type: Easing.InOutQuad }
        }
    }

    // we don't want our tips to interfere with the details views
    GettingStartedTipLayer {
        id: tipsLayer
        anchors.fill: parent
        scopeId: "aircraft"
        z: detailsView.z - 1
    }

    AircraftDetailsView {
        id: detailsView
        anchors.fill: parent
        visible: false
        z: 10

        BackButton {
            id: backButton
            anchors { left: parent.left; top: parent.top; margins: Style.margin }
            onClicked: root.goBack();
        }
    }


}
