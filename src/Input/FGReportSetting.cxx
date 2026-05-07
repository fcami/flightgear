// FGReportSetting.cxx -- event-setting and report-setting types for input devices
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#include "FGReportSetting.hxx"

#include <simgear/debug/ErrorReportingCallback.hxx>
#include <simgear/props/props_io.hxx>
#include <simgear/structure/exception.hxx>

#include <Main/fg_props.hxx>
#include <Scripting/NasalSys.hxx>

FGEventSetting::FGEventSetting(SGPropertyNode_ptr base) : value(0.0)
{
    SGPropertyNode_ptr n;

    if ((n = base->getNode("value")) != NULL) {
        valueNode = NULL;
        value = n->getDoubleValue();
    } else {
        n = base->getNode("property");
        if (n == NULL) {
            SG_LOG(SG_INPUT, SG_WARN, "Neither <value> nor <property> defined for event setting:" << base->getLocation());
        } else {
            valueNode = fgGetNode(n->getStringValue(), true);
        }
    }

    if ((n = base->getChild("condition")) != NULL) {
        condition = sgReadCondition(base, n);
    } else {
        simgear::reportFailure(simgear::LoadFailure::Misconfigured,
                               simgear::ErrorCode::InputDeviceConfig,
                               "No condition for event setting",
                               sg_location(base));
    }
}

double FGEventSetting::GetValue()
{
    return valueNode == NULL ? value : valueNode->getDoubleValue();
}

bool FGEventSetting::Test()
{
    return condition == NULL ? true : condition->test();
}

FGReportSetting::FGReportSetting(SGPropertyNode_ptr base)
{
    location = base->getLocation();
    reportId = base->getIntValue("report-id");

    auto nas = globals->get_subsystem<FGNasalSys>();
    if (!nas) {
        SG_LOG(SG_INPUT, SG_DEV_ALERT, "Nasal subsystem not available, input report settings won't work");
        return;
    }

    const auto loc = base->getLocation();
    if (base->hasChild("nasal-function")) {
        auto nasalFunction = base->getStringValue("nasal-function");
        // we're compiling a trivial function call as code, so we can use the same
        // code path at runtime
        nasalCode = nas->createCode(nasalFunction + "()", loc.getPath(), loc.getLine());
    } else if (base->hasChild("nasal")) {
        nasalCode = nas->createCode(base->getStringValue("nasal"), loc.getPath(), loc.getLine());
    } else {
        simgear::reportFailure(simgear::LoadFailure::Misconfigured,
                               simgear::ErrorCode::InputDeviceConfig,
                               "No nasal/nasal-function defined for report setting",
                               sg_location(base));
    }

    if (base->hasChild("report-type")) {
        const auto s = base->getStringValue("report-type");
        if (s == "output") {
            _type = Type::Output;
        } else if (s == "feature") {
            _type = Type::Feature;
        } else {
            simgear::reportFailure(simgear::LoadFailure::Misconfigured,
                                   simgear::ErrorCode::InputDeviceConfig,
                                   "Invalid report type:" + s,
                                   sg_location(base));
        }
    }

    auto watchNodes = base->getChildren("watch");
    for (auto w : watchNodes) {
        std::string path = w->getStringValue();
        SGPropertyNode_ptr n = globals->get_props()->getNode(path, true);
        n->addChangeListener(this);
    }
}

bool FGReportSetting::Test()
{
    bool d = dirty;
    dirty = false;
    return d;
}

simgear::UInt8Vector FGReportSetting::reportBytes(naRef module) const
{
    naRef result = nasalCode.callWithLocals(module);

    if (naIsString(result)) {
        size_t len = naStr_len(result);
        char* bytes = naStr_data(result);
        char* endByte = bytes + len;
        return simgear::UInt8Vector(
            reinterpret_cast<uint8_t*>(bytes),
            reinterpret_cast<uint8_t*>(endByte));
    }

    if (naIsVector(result)) {
        int len = naVec_size(result);
        simgear::UInt8Vector d;
        for (int b = 0; b < len; ++b) {
            int num = naNumValue(naVec_get(result, b)).num;
            d.push_back(static_cast<uint8_t>(num));
        }

        return d;
    }

    // allow returning nil to mean no data
    if (naIsNil(result)) {
        return {};
    }

    throw sg_exception("Bad data from input report setting", "result was not a string or vector", sg_location(location));
}

void FGReportSetting::valueChanged(SGPropertyNode* n)
{
    auto it = watchValueCache.find(n);
    const auto val = n->getStringValue();
    if (it == watchValueCache.end()) {
        watchValueCache.insert(std::make_pair(n, val));
        dirty = true;
        return;
    }

    // because we use string equality, for floating-point values, we will
    // quantise to the precision of the string representation.
    // that's an almost-feature, until we define explicit precision on properties
    if (val == it->second) {
        return;
    }

    it->second = val;
    dirty = true;
}
