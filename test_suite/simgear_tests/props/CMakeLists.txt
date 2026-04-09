# SPDX-FileCopyrightText: 2016 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx test_props.cxx test_condition.cxx
    test_props.hxx test_condition.hxx
)

fg_add_test_suite(SimgearPropsTests     m)
fg_add_test_suite(SimgearConditionTests m)
