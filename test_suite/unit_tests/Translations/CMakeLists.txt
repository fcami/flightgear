# SPDX-FileCopyrightText: 2025 Florent Rougon
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx test_FGTranslate.cxx test_TranslationResource.cxx
    test_FGTranslate.hxx test_TranslationResource.hxx
)

fg_add_test_suite(FGTranslateTests          u)
fg_add_test_suite(TranslationResourceTests  u)
