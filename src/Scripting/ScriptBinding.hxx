// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 James Turner <james@flightgear.org>

/**
 * @file ScriptBinding.hxx
 * @brief SGAbstractBinding subclass that compiles and caches Nasal scripts.
 */

#pragma once

#include <simgear/nasal/cppbind/NasalCode.hxx>
#include <simgear/structure/SGBinding.hxx>

#include <string>

/**
 * A binding that parses a Nasal <script> element once and caches the compiled
 * code object, avoiding re-compilation on every invocation. Handles the same
 * "script" and "module" arguments as the legacy "nasal" command.
 */
class ScriptBinding : public SGAbstractBinding
{
public:
    /**
     * Register the ScriptBinding factory for the "nasal" command name with
     * SGAbstractBinding. Call this from FGNasalSys::init() so the factory is
     * in place before input-device bindings are read.
     */
    static void registerFactory();

    void read(const SGPropertyNode* node, SGPropertyNode* root) override;

private:
    void innerFire() const override;

    /// Compile the script on first use.
    void ensureCode() const;

    std::string _script;
    std::string _moduleName;
    std::string _filename;
    int _firstLine = 1;

    mutable nasal::NasalCode _code;
};
