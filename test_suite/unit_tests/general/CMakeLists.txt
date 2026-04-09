# SPDX-FileCopyrightText: 2016 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx test-mktime.cxx test_Views.cxx
    test-mktime.hxx test_Views.hxx
)

fg_add_test_suite(MktimeTests u)
fg_add_test_suite(ViewsTests  u)
