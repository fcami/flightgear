# SPDX-FileCopyrightText: 2016 Edward d'Auvergne
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE TestSuite.cxx testCanvas.cxx testCanvas.hxx)

fg_add_test_suite(CanvasTests m)
