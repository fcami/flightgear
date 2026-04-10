// FGReportSetting.hxx -- event-setting and report-setting types for input devices
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#pragma once

#include <map>
#include <string>
#include <vector>

#include <simgear/io/lowlevel.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/misc/strutils.hxx>
#include <simgear/nasal/cppbind/NasalCode.hxx>
#include <simgear/props/condition.hxx>
#include <simgear/props/props.hxx>
#include <simgear/structure/SGReferenced.hxx>
#include <simgear/structure/SGSharedPtr.hxx>
#include <simgear/structure/SGSourceLocation.hxx>

/// TODO: document
class FGEventSetting : public SGReferenced
{
public:
    FGEventSetting(SGPropertyNode_ptr base);
    // return evaluated condition or true if condition is nullptr
    bool Test();
    // return either value of valueNode or value if valueNode is nullptr
    double GetValue();

protected:
    double value{0.0};
    SGPropertyNode_ptr valueNode;
    SGSharedPtr<const SGCondition> condition;
};

typedef SGSharedPtr<FGEventSetting> FGEventSetting_ptr;
typedef std::vector<FGEventSetting_ptr> setting_list_t;

/// TODO: document
class FGReportSetting : public SGReferenced,
                        public SGPropertyChangeListener
{
public:
    enum class Type {
        Output,
        Feature
    };

    FGReportSetting(SGPropertyNode_ptr base);
    unsigned int getReportId() const { return reportId; }
    Type getReportType() const
    {
        return _type;
    }

    bool hasError() const
    {
        return error;
    }

    void markAsError()
    {
        error = true;
    }

    bool Test();
    simgear::UInt8Vector reportBytes(const std::string& moduleName) const;
    virtual void valueChanged(SGPropertyNode* node);

protected:
    unsigned int reportId = 0;
    Type _type = Type::Output;

    nasal::NasalCode nasalCode;

    bool dirty = true;
    bool error = false;
    SGSourceLocation location;

    // we only dirty when the value of a watched node changes.
    // record the value as a string
    // FIXME: use a more correct+efficient storage of the previous value,
    // either `std::any` or make `SGRaw*` accessible + comparable on SGPropertyNode
    std::map<SGPropertyNode_ptr, std::string> watchValueCache;
};

typedef SGSharedPtr<FGReportSetting> FGReportSetting_ptr;
typedef std::vector<FGReportSetting_ptr> report_setting_list_t;
