# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: (C) 2015 Edward d'Auvergne

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx
    testNasalSys.cxx testNasalLib.cxx testGC.cxx benchmarkPropsAccess.cxx testVector.cxx
    testNasalSys.hxx testNasalLib.hxx testGC.hxx benchmarkPropsAccess.hxx testVector.hxx
)

fg_add_test_suite(NasalSysTests      u)
fg_add_test_suite(NasalGCTests       u)
fg_add_test_suite(NasalLibTests      u)
fg_add_test_suite(BenchmarkPropsAccess u)
fg_add_test_suite(VectorTests        u)
