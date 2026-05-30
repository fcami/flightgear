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
        _launcher.browseAircraftModel.loadCompatibilityAndRatingsSettings();

        // if the user has favourites defined, default to that tab
        if (_launcher.favouriteAircraftModel.count > 0) {
            root.state = "favourites"
            root.updateSelectionFromLauncher();
        }
    }

    GettingStartedScope.controller: tipsLayer.controller

    Rectangle
    {
        id: tabBar
        height: searchButton.height + (Style.margin * 2)
        width: parent.width
        color: Style.backgroundColor
        // Sit above the content Loader: toolbar and tab tooltips drop below the
        // bar into the list area, and must not be painted behind the list.
        z: 5

        GridToggleButton {
            id: gridModeToggle
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: Style.margin
            gridMode: !_launcher.aircraftGridMode
            onClicked: _launcher.aircraftGridMode = !_launcher.aircraftGridMode

            GettingStartedTip {
                tipId: "gridModeTip"

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    horizontalCenterOffset: Style.margin
                    top: parent.bottom
                }
                arrow: GettingStartedTip.TopLeft
                text: qsTr("Toggle between grid and list view")
            }
        }

        // Compact sort and filter indicators. Sort applies to every tab;
        // compatibility and ratings only apply while browsing.
        Row {
            id: toolbarIndicators
            anchors.left: gridModeToggle.right
            anchors.leftMargin: Style.margin * 2
            anchors.verticalCenter: parent.verticalCenter
            spacing: Style.margin
            // Lift above the tab row (declared later) so the hover tooltips,
            // which extend right over the tabs, are not painted underneath.
            z: 100

            ToolbarButton {
                id: sortButton
                anchors.verticalCenter: parent.verticalCenter
                iconName: "up-down"
                tooltip: (root.__model && root.__model.sortMode === FG.AircraftProxyModel.SortByName)
                         ? qsTr("Sorted by name – click to sort by rating")
                         : qsTr("Sorted by rating – click to sort by name")
                onClicked: {
                    if (!root.__model) return;
                    root.__model.sortMode = (root.__model.sortMode === FG.AircraftProxyModel.SortByRating)
                                            ? FG.AircraftProxyModel.SortByName
                                            : FG.AircraftProxyModel.SortByRating;
                }
            }

            ToolbarButton {
                id: compatButton
                anchors.verticalCenter: parent.verticalCenter
                visible: root.state === "browse"
                iconName: "toolbox-aircraft"
                active: _launcher.browseAircraftModel.compatibilityFilterEnabled
                // RAG indicator: green while restricted to compatible aircraft,
                // amber when showing everything, compatible or not.
                activeColorSpec: "#3aaa35"
                inactiveColorSpec: "#e8a000"
                tooltip: active ? qsTr("Showing compatible aircraft only – click to show all")
                                : qsTr("Showing all aircraft – click to show compatible only")
                onClicked: {
                    var v = !_launcher.browseAircraftModel.compatibilityFilterEnabled;
                    _launcher.browseAircraftModel.compatibilityFilterEnabled = v;
                    _launcher.saveUISetting("enable-compatibility-filter", v);
                }
            }

            Item {
                id: ratingsIndicatorWrap
                anchors.verticalCenter: parent.verticalCenter
                visible: root.state === "browse"
                width: Math.max(ratingsIndicator.width, countText.width)
                height: ratingsIndicator.height + countText.height + 1

                RatingsFilterIndicator {
                    id: ratingsIndicator
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    thresholds: _launcher.browseAircraftModel.ratings
                    active: _launcher.browseAircraftModel.ratingsFilterEnabled
                }

                // Count sits under the stars, using the spare vertical room, so
                // it does not extend right over the tab labels.
                StyledText {
                    id: countText
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: ratingsIndicator.bottom
                    color: Style.disabledTextColor
                    font.pixelSize: Style.baseFontPixelSize - 1
                    text: _launcher.browseAircraftModel.summaryText
                }

                MouseArea {
                    id: ratingsIndicatorMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: ratingsPopup.visible = !ratingsPopup.visible
                }

                Tooltip {
                    anchors.left: parent.right
                    anchors.leftMargin: Style.margin
                    anchors.verticalCenter: parent.verticalCenter
                    visible: ratingsIndicatorMouse.containsMouse
                    text: qsTr("Filter by minimum ratings")
                }
            }
        }

        Item {
            id: tabsZone
            anchors.left: toolbarIndicators.right
            anchors.leftMargin: Style.margin
            anchors.right: searchButton.left
            anchors.rightMargin: Style.margin
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height

            // Drop the labels to icons only once the full-label row no longer
            // fits between the indicators and the search box.
            readonly property real __fullWidth: installedAircraftButton.fullWidth
                                               + favouritesButton.fullWidth
                                               + browseButton.fullWidth
                                               + updatesButton.fullWidth
                                               + (tabRow.spacing * 3)
            readonly property bool showLabels: __fullWidth <= width

          Row {
            id: tabRow
            anchors.centerIn: parent
            spacing: Style.margin

            TabButton {
                id: installedAircraftButton
                iconName: "tab-installed"
                showLabel: tabsZone.showLabels
                text: qsTr("Installed Aircraft")
                onClicked: {
                    root.state = "installed"
                    root.updateSelectionFromLauncher();
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
                iconName: "star-filled"
                showLabel: tabsZone.showLabels
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
                iconName: "tab-browse"
                showLabel: tabsZone.showLabels

                text: qsTr("Browse")
                onClicked: {
                    root.state = "browse"
                    root.updateSelectionFromLauncher();
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
                iconName: "tab-updates"
                showLabel: tabsZone.showLabels
                text: qsTr("Updates")
                onClicked: {
                    root.state = "updates"
                    root.updateSelectionFromLauncher();
                }
                active: root.state == "updates"
            }
          } // of tab row
        } // of tabs zone

        SearchButton {
            id: searchButton

            height: installedAircraftButton.height

            anchors.right: parent.right
            anchors.rightMargin: Style.margin
            anchors.verticalCenter: parent.verticalCenter

            onSearch: {
               _launcher.searchAircraftModel.setAircraftFilterString(term)
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

    // Ratings filter popup, opened from the ratings indicator. A full-window
    // dismiss layer closes it on an outside click.
    MouseArea {
        id: ratingsPopupDismiss
        anchors.fill: parent
        visible: ratingsPopup.visible
        z: 200
        onClicked: ratingsPopup.visible = false
    }

    Rectangle {
        id: ratingsPopup
        visible: false
        z: 201
        width: Style.strutSize * 8
        height: popupColumn.height + (Style.margin * 2)
        anchors.top: tabBarDivider.bottom
        anchors.left: toolbarIndicators.left
        color: Style.backgroundColor
        border.width: 1
        border.color: Style.minorFrameColor

        // Swallow clicks on empty popup space, so they do not reach the dismiss layer.
        MouseArea { anchors.fill: parent }

        Column {
            id: popupColumn
            x: Style.margin
            y: Style.margin
            width: parent.width - (Style.margin * 2)
            spacing: Style.margin

            ToggleSwitch {
                id: ratingsEnableToggle
                checked: _launcher.browseAircraftModel.ratingsFilterEnabled
                label: qsTr("Filter using ratings")
                onCheckedChanged: {
                    _launcher.browseAircraftModel.ratingsFilterEnabled = checked;
                    _launcher.saveUISetting("enable-ratings-filter", checked);
                }
            }

            StyledText {
                width: parent.width
                wrapMode: Text.WordWrap
                text: qsTr("Aircraft are rated by the community on four criteria, from one to five, to help gauge how complete and functional an aircraft is.")
            }

            RatingSlider {
                label: qsTr("Minimum flight-model (FDM) rating")
                ratings: _launcher.browseAircraftModel.ratings
                ratingIndex: 0
            }
            RatingSlider {
                label: qsTr("Minimum systems rating")
                ratings: _launcher.browseAircraftModel.ratings
                ratingIndex: 1
            }
            RatingSlider {
                label: qsTr("Minimum cockpit visual rating")
                ratings: _launcher.browseAircraftModel.ratings
                ratingIndex: 2
            }
            RatingSlider {
                label: qsTr("Minimum external visual model rating")
                ratings: _launcher.browseAircraftModel.ratings
                ratingIndex: 3
            }
        }
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
        id: searchHeader
        Rectangle {
            visible: _launcher.searchAircraftModel.count === 0
            width: aircraftContent.width
            height: visible ? Style.strutSize : 0

            StyledText {
                anchors.fill: parent
                text: qsTr("No aircraft match the search.")
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
        source: (gridModeToggle.visible && _launcher.aircraftGridMode) ? "qrc:///qml/AircraftGridView.qml"
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

    // Browse can filter down to zero aircraft - usually because the compatibility
    // filter excludes aircraft that do not declare compatibility with this
    // FlightGear version. Rather than show a blank page, explain why and offer a
    // one-click way to relax the filters. Shown only when a filter is active, so a
    // genuinely empty catalog (e.g. no network) is left to the no-catalog handling.
    Item {
        id: browseEmptyState
        anchors {
            left: parent.left
            right: parent.right
            top: tabBarDivider.bottom
            bottom: parent.bottom
            margins: Style.strutSize
        }
        visible: (root.state === "browse")
                 && (_launcher.browseAircraftModel.count === 0)
                 && (_launcher.browseAircraftModel.compatibilityFilterEnabled
                     || _launcher.browseAircraftModel.ratingsFilterEnabled)

        Column {
            anchors.centerIn: parent
            width: Math.min(parent.width, Style.strutSize * 10)
            spacing: Style.margin * 2

            StyledText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: Style.headingFontPixelSize
                text: _launcher.browseAircraftModel.compatibilityFilterEnabled
                      ? qsTr("None of the available aircraft are marked as compatible with this version of FlightGear.")
                      : qsTr("No aircraft match the current ratings filter.")
            }

            ClickableText {
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: Style.headingFontPixelSize
                text: qsTr("Show all aircraft")
                // Address the filter named in the message above. Clearing the
                // compatibility filter alone restores the rated aircraft, so fall
                // back to the ratings filter only if that is the remaining cause.
                onClicked: {
                    if (_launcher.browseAircraftModel.compatibilityFilterEnabled) {
                        _launcher.browseAircraftModel.compatibilityFilterEnabled = false;
                        _launcher.saveUISetting("enable-compatibility-filter", false);
                    } else if (_launcher.browseAircraftModel.ratingsFilterEnabled) {
                        _launcher.browseAircraftModel.ratingsFilterEnabled = false;
                        _launcher.saveUISetting("enable-ratings-filter", false);
                    }
                }
            }
        }
    }

    states: [
        State {
            name: "installed"
            PropertyChanges {
                target: root
                __model: _launcher.installedAircraftModel
                __header: emptyHeaderFooter
                __footer: installMoreAircraftFooter
            }

            PropertyChanges {
                target: gridModeToggle; visible: true
            }
        },

        State {
            name: "search"
            PropertyChanges {
                target: root
                __model: _launcher.searchAircraftModel
                __header: searchHeader
                __footer: emptyHeaderFooter
            }

            PropertyChanges {
                target: gridModeToggle; visible: true
            }
        },

        State {
            name: "browse"
            PropertyChanges {
                target: root
                __model: _launcher.browseAircraftModel
                __header: _addOns.showNoOfficialHangar ? noDefaultCatalogHeader : emptyHeaderFooter
                __footer: emptyHeaderFooter
            }

            PropertyChanges {
                target: gridModeToggle; visible: true
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
                target: gridModeToggle; visible: false
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
                target: gridModeToggle; visible: true
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

    // we don't want our tips to interfere with the details views
    GettingStartedTipLayer {
        id: tipsLayer
        anchors.fill: parent
        scopeId: "aircraft"
    }

    AircraftDetailsView {
        id: detailsView
        anchors.fill: parent
        visible: false
        // Full-screen overlay: stay above the raised tab bar and the ratings popup.
        z: 300

        BackButton {
            id: backButton
            anchors { left: parent.left; top: parent.top; margins: Style.margin }
            onClicked: root.goBack();
        }
    }


}
