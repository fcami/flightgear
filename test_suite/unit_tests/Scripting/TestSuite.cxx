// SPDX-FileCopyrightText: (C) 2016 Edward d'Auvergne
// SPDX-License-Identifier: GPL-2.0-or-later

#include "benchmarkPropsAccess.hxx"
#include "testGC.hxx"
#include "testNasalLib.hxx"
#include "testNasalSys.hxx"
#include "testStringFormatting.hxx"
#include "testVector.hxx"

// Set up the unit tests.
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(NasalSysTests, "Unit tests");
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(NasalGCTests, "Unit tests");
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(NasalLibTests, "Unit tests");
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(NasalStringFormattingTests, "Unit tests");
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(BenchmarkPropsAccess, "Unit tests");
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(VectorTests, "Unit tests");
