// SPDX-FileCopyrightText: Copyright (C) 2018 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick 2.4
import FlightGear 1.0

Item
{
    implicitHeight: Style.menuItemHeight
    width: parent.width // take width from our parent menu

    function minWidth() { return 0; }

    function closeMenu()
    {
        parent.requestClose();
    }

    function menu()
    {
        return parent.getMenu();
    }
}
