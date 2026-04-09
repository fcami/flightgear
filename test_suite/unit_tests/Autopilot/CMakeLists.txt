# SPDX-FileCopyrightText: 2016 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx
    testDigitalFilter.cxx testPidController.cxx testPidControllerData.cxx
    testInputValue.cxx testMonostable.cxx
    testDigitalFilter.hxx testPidController.hxx testPidControllerData.hxx
    testInputValue.hxx testMonostable.hxx
)

fg_add_test_suite(MonostableTests    u)
fg_add_test_suite(DigitalFilterTests u)
fg_add_test_suite(PidControllerTests u)
fg_add_test_suite(InputValueTests    u)
