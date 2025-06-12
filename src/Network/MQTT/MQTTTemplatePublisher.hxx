/*
 * SPDX-FileName: MQTTSystem.cxx
 * SPDX-FileComment: mqtt subsystem class
 * SPDX-FileCopyrightText: Copyright (C) 2025 Torsten Dreyer - torsten (at) t3r (dot) de
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once
#include "MQTTPublisher.hxx"
#include "StringInterpolation.hxx"

namespace flightgear {

class MQTTTemplatePublisher : public MQTTPublisher
{
public:
    MQTTTemplatePublisher(std::shared_ptr<IMQTTClient> client, const std::string& topicName, const std::string& payloadTemplate, int qos = 0, bool retain = false)
        : MQTTPublisher(client, topicName, qos, retain),
          _payloadTemplate(payloadTemplate)
    {
    }
    virtual void tick() override
    {
        auto payload = StringInterpolator::interpolate(_payloadTemplate);
        _client->publish(std::make_shared<MQTTTopic>(_name, payload, _qos, _retain));
    }

private:
    std::string _payloadTemplate;
};
}