# SPDX-FileCopyrightText: 2016 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE TestSuite.cxx test_SGTime.cxx test_SGTime.hxx)

fg_add_test_suite(SimgearTimingTests m)
