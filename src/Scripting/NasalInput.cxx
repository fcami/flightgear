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

static simgear::UInt8Vector dataFromArg(const nasal::CallContext& ctx, size_t index)
{
    if (ctx.isString(index)) {
        std::string str = ctx.getArg<std::string>(index);
        return dataFromString(str);
    } else if (ctx.isVector(index)) {
        std::vector<int> ints = ctx.getArg<std::vector<int>>(index);
        simgear::UInt8Vector data;
        data.reserve(ints.size());
        for (int v : ints) {
            if (v < 0 || v > 255) {
                ctx.runtimeError("data vector contains out-of-range value: %d, (must be 0-255)", v);
            }

            data.push_back(static_cast<uint8_t>(v));
        }
        return data;
    } else {
        ctx.runtimeError("data argument must be a string or a vector of integers");
        return {};
    }
}

static naRef f_sendFeatureReport(FGInputDevice& device, const nasal::CallContext& ctx)
{
    if (ctx.argc < 2) {
        ctx.runtimeError("sendFeatureReport(reportId, data) requires 2 arguments");
    }

    unsigned int reportId = ctx.requireArg<unsigned int>(0);
    simgear::UInt8Vector data = dataFromArg(ctx, 1);
    device.SendFeatureReport(reportId, data);
    return naNil();
}

static naRef f_sendOutputReport(FGInputDevice& device, const nasal::CallContext& ctx)
{
    if (ctx.argc < 2) {
        ctx.runtimeError("sendOutputReport(reportId, data) requires 2 arguments");
    }

    unsigned int reportId = ctx.requireArg<unsigned int>(0);
    simgear::UInt8Vector data = dataFromArg(ctx, 1);

    device.SendOutputReport(reportId, data);
    return naNil();
}

//------------------------------------------------------------------------------
naRef initNasalInput(naRef globals, naContext c)
{
    using InputDeviceRef = SGSharedPtr<FGInputDevice>;
    using NasalInputDevice = nasal::Ghost<InputDeviceRef>;

    NasalInputDevice::init("FGInputDevice")
        .method("sendFeatureReport", &f_sendFeatureReport)
        .method("sendOutputReport", &f_sendOutputReport);

    return naNil();
}
