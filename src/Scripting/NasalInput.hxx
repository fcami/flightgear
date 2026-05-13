//@file Expose Input module to Nasal
//
// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <simgear/nasal/nasal.h>

naRef initNasalInput(naRef globals, naContext c);
