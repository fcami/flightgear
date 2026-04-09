# SPDX-FileCopyrightText: 2016 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx test_ATCController.cxx test_Quadtree.cxx
    test_ATCController.hxx test_Quadtree.hxx
)

fg_add_test_suite(QuadtreeTests       u)
fg_add_test_suite(ATCControllerTests  u)
