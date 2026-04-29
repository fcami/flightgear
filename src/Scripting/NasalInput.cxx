// Expose Input module to Nasal
//
// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "NasalInput.hxx"

#include <Input/FGInputDevice.hxx>

#include <simgear/nasal/cppbind/Ghost.hxx>
#include <simgear/nasal/cppbind/NasalHash.hxx>
#include <simgear/nasal/cppbind/from_nasal.hxx>
#include <simgear/nasal/cppbind/to_nasal.hxx>


static simgear::UInt8Vector dataFromString(const std::string& str)
{
    return simgear::UInt8Vector(
        reinterpret_cast<const uint8_t*>(str.data()),
        reinterpret_cast<const uint8_t*>(str.data() + str.size()));
}

static naRef f_sendFeatureReport(FGInputDevice& device, const nasal::CallContext& ctx)
{
    if (ctx.argc < 2) {
        ctx.runtimeError("sendFeatureReport(reportId, data) requires 2 arguments");
    }

    unsigned int reportId = ctx.requireArg<unsigned int>(0);
    simgear::UInt8Vector data;

    if (ctx.isString(1)) {
        std::string str = ctx.getArg<std::string>(1);
        data = dataFromString(str);
    } else if (ctx.isVector(1)) {
        std::vector<int> ints = ctx.getArg<std::vector<int>>(1);
        data.reserve(ints.size());
        for (int v : ints) {
            data.push_back(static_cast<uint8_t>(v));
        }
    } else {
        ctx.runtimeError("data argument must be a string or a vector of integers");
    }

    device.SendFeatureReport(reportId, data);
    return naNil();
}


//------------------------------------------------------------------------------
naRef initNasalInput(naRef globals, naContext c)
{
    using InputDeviceRef = SGSharedPtr<FGInputDevice>;
    using NasalInputDevice = nasal::Ghost<InputDeviceRef>;

    NasalInputDevice::init("FGInputDevice")
        .method("sendFeatureReport", &f_sendFeatureReport);

    return naNil();
}
