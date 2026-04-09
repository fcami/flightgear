# SPDX-FileCopyrightText: 2018 Edware d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx
    testAeroElement.cxx testFGInterface.cxx testYASimAtmosphere.cxx testYASimGear.cxx
    testAeroElement.hxx testFGInterface.hxx testYASimAtmosphere.hxx testYASimGear.hxx
)

fg_add_test_suite(AeroElementTests    u)
fg_add_test_suite(YASimAtmosphereTests u)
fg_add_test_suite(YASimGearTests      u)
fg_add_test_suite(FGInterfaceTests    u)
