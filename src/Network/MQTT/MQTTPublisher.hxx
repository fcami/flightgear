/*
 * SPDX-FileName: MQTTSystem.cxx
 * SPDX-FileComment: mqtt subsystem class
 * SPDX-FileCopyrightText: Copyright (C) 2025 Torsten Dreyer - torsten (at) t3r (dot) de
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once
#include "MQTTClient.hxx"

namespace flightgear {

class MQTTPublisher
{
public:
    MQTTPublisher(std::shared_ptr<IMQTTClient> client, const std::string topicName, int qos = 0, bool retain = false)
        : _client(client),
          _name(topicName),
          _qos(qos),
          _retain(retain)
    {
    }
    virtual ~MQTTPublisher() = default;
    virtual void tick() = 0;

protected:
    std::shared_ptr<IMQTTClient> _client;
    std::string _name;
    int _qos = 0;
    bool _retain = false;

private:
    MQTTPublisher();
};

}