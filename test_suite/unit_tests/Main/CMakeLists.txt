# SPDX-FileCopyrightText: 2016 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx
    test_autosaveMigration.cxx test_FGLocale.cxx test_posinit.cxx
    test_timeManager.cxx test_commands.cxx
    test_autosaveMigration.hxx test_FGLocale.hxx test_posinit.hxx
    test_timeManager.hxx test_commands.hxx
)

fg_add_test_suite(AutosaveMigrationTests u)
fg_add_test_suite(FGLocaleTests         u)
fg_add_test_suite(PosInitTests          u)
fg_add_test_suite(TimeManagerTests      u)
fg_add_test_suite(CommandsTests         u)
