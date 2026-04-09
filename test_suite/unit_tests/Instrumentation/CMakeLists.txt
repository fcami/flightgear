# SPDX-FileCopyrightText: 2016 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx
    test_navRadio.cxx test_gps.cxx test_hold_controller.cxx test_rnav_procedures.cxx
    test_dme.cxx test_commRadio.cxx test_transponder.cxx test_headingIndicator.cxx
    test_navRadio.hxx test_gps.hxx test_hold_controller.hxx test_rnav_procedures.hxx
    test_dme.hxx test_commRadio.hxx test_transponder.hxx test_headingIndicator.hxx
)

fg_add_test_suite(NavRadioTests       u)
fg_add_test_suite(GPSTests            u)
fg_add_test_suite(HoldControllerTests u)
fg_add_test_suite(RNAVProcedureTests  u)
fg_add_test_suite(DMEReceiverTests    u)
fg_add_test_suite(CommRadioTests      u)
fg_add_test_suite(TransponderTests    u)
fg_add_test_suite(HeadingIndicatorTests u)
