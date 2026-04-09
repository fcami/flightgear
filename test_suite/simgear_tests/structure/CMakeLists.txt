# SPDX-FileCopyrightText: 2020  James Turner <james@flightgear.org>
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx test_binding.cxx test_event.cxx
    test_binding.hxx test_event.hxx
)

fg_add_test_suite(SimgearEventTests   m)
fg_add_test_suite(SimgearBindingTests m)
