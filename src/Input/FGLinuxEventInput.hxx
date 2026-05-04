// FGEventInput.hxx -- handle event driven input devices for the Linux O/S
//
// Written by Torsten Dreyer, started July 2009
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#pragma once

#include "FGEventInput.hxx"
#include <linux/input.h>

struct FGLinuxEventData : public FGEventData {
    FGLinuxEventData( struct input_event & event, double dt, int modifiers ) :
        FGEventData( (double)event.value, dt, modifiers ),
        type(event.type),
        code(event.code) {
    }
    unsigned type;
    unsigned code;
};

/*
 * A implementation for linux event devices
 */
class FGLinuxInputDevice : public FGInputDevice
{
public:
    FGLinuxInputDevice();
    FGLinuxInputDevice(std::string aName, std::string aDevname, std::string aSerial, std::string aDevpath);
    virtual ~FGLinuxInputDevice();

    bool Open() override;
    void Send( const char * eventName, double value ) override;
    const char * TranslateEventName( FGEventData & eventData ) override;

    void SetDevname( const std::string & name );
    std::string GetDevFile() const { return devfile; }
    std::string GetDevPath() const { return devpath; }

    int GetFd() { return fd; }

    double Normalize( struct input_event & event );
private:
    void Close() override;

    std::string devfile;
    std::string devpath;
    int fd {-1};
    std::map<unsigned int,input_absinfo> absinfo;
};

class FGLinuxEventInput : public FGEventInput
{
public:
    FGLinuxEventInput();
    virtual ~ FGLinuxEventInput();

    // Subsystem API.
    void postinit() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "input-event"; }

protected:
};
