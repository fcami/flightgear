# SPDX-FileCopyrightText: 2010 James Turner
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx
    test_xmlDialog.cxx test_xmlDialog.hxx
    test_nasalMenu.cxx test_nasalMenu.hxx
)

fg_add_test_suite(XMLDialogTests u)
fg_add_test_suite(NasalMenuTests u)
