// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string.h>

#include "FGTextureLoaderInterface.hxx"


class FGDummyTextureLoader : public FGTextureLoaderInterface {
public:
  virtual GLuint loadTexture (const std::string& filename);
};
