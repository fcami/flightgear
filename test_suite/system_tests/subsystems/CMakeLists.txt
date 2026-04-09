# SPDX-FileCopyrightText: 2016 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx
    test_instanced_creation.cxx test_noninstanced_creation.cxx
    test_instanced_creation.hxx test_noninstanced_creation.hxx
)

fg_add_test_suite(InstancedSubsystemTests    s)
fg_add_test_suite(NonInstancedSubsystemTests s)
