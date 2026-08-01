// SPDX-FileCopyrightText: Copyright (C) 2018 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear 1.0

Text {
    id: root
    font.pixelSize: Style.baseFontPixelSize

    property bool hover: false

    color: hover ? Style.themeColor : (root.enabled ? Style.baseTextColor : Style.disabledTextColor)
}
