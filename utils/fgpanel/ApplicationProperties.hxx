// SPDX-FileCopyrightText: 2016 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <simgear/misc/sg_path.hxx>
#include <simgear/props/props.hxx>


class ApplicationProperties {
public:
  static double getDouble (const char *name, const double def = 0.0);
  static SGPath GetRootPath(const char* subDir = nullptr);
  static SGPropertyNode_ptr Properties;
  static std::string root;
};
