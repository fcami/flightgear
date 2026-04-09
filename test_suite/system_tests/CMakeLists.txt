# SPDX-FileCopyrightText: 2016 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

# Add each system test category.
foreach(system_test_category
        FDM
        Instrumentation
        Navaids
        Main
        subsystems
    )
    add_subdirectory(${system_test_category})
endforeach()
