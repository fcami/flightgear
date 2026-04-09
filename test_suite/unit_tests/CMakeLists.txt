# SPDX-FileCopyrightText: (C) 2016  Edware d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

# Add each unit test category.
foreach(unit_test_category
        Add-ons
        Aircraft
        general
        FDM
        Input
        Main
        Navaids
        Network
        Instrumentation
        Scripting
        AI
        Airports
        ATC
        Autopilot
        GUI
        Translations
    )
    add_subdirectory(${unit_test_category})
endforeach()
