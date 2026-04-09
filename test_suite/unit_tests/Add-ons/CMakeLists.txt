# SPDX-FileCopyrightText: 2016 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE TestSuite.cxx test_AddonManagement.cxx test_AddonManagement.hxx)

fg_add_test_suite(AddonManagementTests u)
