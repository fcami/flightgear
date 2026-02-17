// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#include "FGPanelTransformation.hxx"

FGPanelTransformation::FGPanelTransformation () :
  table (0) {
}

FGPanelTransformation::~FGPanelTransformation () {
  delete table;
}
