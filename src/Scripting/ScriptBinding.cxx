// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 James Turner <james@flightgear.org>

#include "ScriptBinding.hxx"
#include "NasalSys.hxx"

#include <Main/globals.hxx>
#include <simgear/debug/logstream.hxx>
#include <simgear/nasal/nasal.h>

//------------------------------------------------------------------------------
void ScriptBinding::registerFactory()
{
    SGAbstractBinding::registerFactory(
        "nasal",
        [](SGPropertyNode_ptr, SGPropertyNode_ptr) -> SGSharedPtr<SGAbstractBinding> {
            return new ScriptBinding;
        });
}

//------------------------------------------------------------------------------
void ScriptBinding::read(const SGPropertyNode* node, SGPropertyNode* root)
{
    SGAbstractBinding::read(node, root);
    _script = node->getStringValue("script");
    _moduleName = node->getStringValue("module");

    const auto loc = node->getLocation();
    _filename = loc.isValid() ? loc.getPath() : node->getPath(true);
    _firstLine = loc.isValid() ? loc.getLine() : 1;
}

//------------------------------------------------------------------------------
void ScriptBinding::ensureCode() const
{
    if (_code.isValid())
        return;
    // If a previous attempt already produced errors, don't retry.
    if (!_code.getErrors().empty())
        return;

    auto* nas = globals->get_subsystem<FGNasalSys>();
    if (!nas) {
        SG_LOG(SG_NASAL, SG_ALERT, "ScriptBinding: Nasal subsystem not available");
        return;
    }

    _code = nas->createCode(_script, _filename, _firstLine);
    if (!_code.isValid()) {
        for (const auto& e : _code.getErrors()) {
            SG_LOG(SG_INPUT, SG_ALERT, e);
        }
    }
}

//------------------------------------------------------------------------------
void ScriptBinding::innerFire() const
{
    ensureCode();
    if (!_code.isValid())
        return;

    auto* nas = globals->get_subsystem<FGNasalSys>();
    if (!nas)
        return;

    nas->setCmdArg(_arg);

    naRef locals = naNil();
    int lsave = naGCSave(locals);

    if (!_moduleName.empty()) {
        locals = nas->getModule(_moduleName.c_str(), true /*create*/);
    }

    _code.callWithLocals(locals);
    naGCRelease(lsave);
}
