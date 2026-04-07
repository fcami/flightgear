// SPDX-FileCopyrightText: 2026 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Automated tests for the Nasal format() function
 */

#include "config.h"

#include <Main/globals.hxx>

#include "testStringFormatting.hxx"
#include "test_suite/FGTestApi/testGlobals.hxx"


void NasalStringFormattingTests::test_builtinTypes()
{
    FGTestApi::setUp::initTestGlobals("test_builtinTypes");

    globals->get_subsystem_mgr()->bind();
    globals->get_subsystem_mgr()->init();
    FGTestApi::setUp::initStandardNasal();
    globals->get_subsystem_mgr()->postinit();

    bool ok = FGTestApi::executeNasal(R"(
        unitTest.assert_equal("", format(""));

        var s = format("zz1 {} {{{}}}.foobar", "foo1", "foo2");
        unitTest.assert_equal("zz1 foo1 {foo2}.foobar", s);

        s = format("zz2 {1} {{{0}}} }}}}}}{{{{{{", "foo1", "foo2");
        unitTest.assert_equal("zz2 foo2 {foo1} }}}{{{", s);

        s = format("zz3 |{:d}|{:d}|{:-d}|{:-d}|", 12, -12, 12, -12);
        unitTest.assert_equal("zz3 |12|-12|12|-12|", s);

        s = format("zz4 |{:+d}|{:+d}|", 12, -12);
        unitTest.assert_equal("zz4 |+12|-12|", s);

        s = format("zz5 |{: d}|{: d}|", 12, -12);
        unitTest.assert_equal("zz5 | 12|-12|", s);

        s = format("zz6 |{: 5d}|{: 5d}|", 12, -12);
        unitTest.assert_equal("zz6 |   12|  -12|", s);

        s = format("{:04d}, {:#04x}, {:#04X}", -7, 12, 255);
        unitTest.assert_equal("-007, 0x0c, 0XFF", s);

        s = format("{:.03f}, {:c}, {:s}", 3.14, 97, "foo\n");
        unitTest.assert_equal("3.140, a, foo\n", s);

        s = format("{:{}d}|{:{}.{}f}|", 256, 5, 3.1415926, 12, 4);
        unitTest.assert_equal("  256|      3.1416|", s);

        s = format("{4:{3}d}|{2:{1}.{0}f}|", 4, 12, 3.1415926, 5, 256);
        unitTest.assert_equal("  256|      3.1416|", s);

        s = format("{:X^12s}", "abc");
        unitTest.assert_equal("XXXXabcXXXXX", s);

        s = format("{:^12s}", "abc");
        unitTest.assert_equal("    abc     ", s);

        s = format("{:<12s}", "abc");
        unitTest.assert_equal("abc         ", s);

        s = format("{:>12s}", "abc");
        unitTest.assert_equal("         abc", s);

        # Tests that depend on undefined behaviour or on specific locale settings
        if (false) {
          # Works like this on my system, however this output is not
          # guaranteed (cf. man 3 printf):
          s = format("{:#.6a}",1 + 0x0f0e * math.pow(2, -16));
          unitTest.assert_equal("1.0f0e00p+0", s);

          # With an 'en_US.UTF-8' locale
          s = format("{:Lf}", 12345678.0123456789);
          unitTest.assert_equal("12,345,678.012346", s);
          # With 'fr_FR.UTF-8', I obtain "12 345 678,012346" instead.
        }
    )");
    CPPUNIT_ASSERT(ok);
}

void NasalStringFormattingTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}
