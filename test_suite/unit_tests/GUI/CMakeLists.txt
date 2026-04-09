# SPDX-FileCopyrightText: 2010 James Turner
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE TestSuite.cxx test_xmlDialog.cxx test_xmlDialog.hxx)

fg_add_test_suite(XMLDialogTests u)
