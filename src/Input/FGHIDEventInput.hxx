// FGHIDEventInput.hxx -- handle event driven input devices via HIDAPI
//
// Written by James Turner
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2017 James Turner

#pragma once

#include <memory>

#include "FGEventInput.hxx"


int extractBits(uint8_t* bytes, size_t lengthInBytes, size_t bitOffset, size_t bitSize);
int signExtend(int inValue, size_t bitSize);
void writeBits(uint8_t* bytes, size_t bitOffset, size_t bitSize, int value);


class FGHIDEventInput : public FGEventInput
{
public:
    FGHIDEventInput();

    virtual ~FGHIDEventInput();

    // Subsystem API.
    void postinit() override;
    void reinit() override;
    void shutdown() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "input-event-hid"; }

private:
    class FGHIDEventInputPrivate;

    std::unique_ptr<FGHIDEventInputPrivate> d;
};
