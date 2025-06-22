/*
 * SPDX-FileName: MQTTSystem.hxx
 * SPDX-FileComment: mqtt subsystem class
 * SPDX-FileCopyrightText: Copyright (C) 2025 Torsten Dreyer - torsten (at) t3r (dot) de
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <memory>
#include <simgear/props/props.hxx>
#include <simgear/structure/subsystem_mgr.hxx>

class MQTTClient;

namespace flightgear {

class MQTTPropertySubscription;
class MQTTPublisher;

class MQTTSystem : public SGSubsystem
{
public:
    MQTTSystem();
    virtual ~MQTTSystem();

    void init() override;
    void bind() override;
    void unbind() override;
    void update(double dt) override;

    void set_serviceable(bool value) { value ? resume() : suspend(); }
    bool is_serviceable() const { return false == is_suspended(); }
    static const char* staticSubsystemClassId() { return "mqtt"; }

private:
    SGPropertyNode_ptr _rootNode;
    std::vector<std::shared_ptr<MQTTClient>> _mqttClients;
    std::vector<std::shared_ptr<MQTTPublisher>> _mqttPublishers;
    std::vector<std::shared_ptr<MQTTPropertySubscription>> _mqttPropertySubscriptions;
};

} // namespace flightgear
