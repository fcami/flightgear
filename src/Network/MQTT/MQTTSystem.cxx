/*
 * SPDX-FileName: MQTTSystem.cxx
 * SPDX-FileComment: mqtt subsystem class
 * SPDX-FileCopyrightText: Copyright (C) 2025 Torsten Dreyer - torsten (at) t3r (dot) de
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MQTTSystem.hxx"
#include "MQTTClient.hxx"
#include "MQTTClientOptionsProps.hxx"
#include "MQTTPropertyPublisher.hxx"
#include "MQTTPublisher.hxx"
#include "MQTTTemplatePublisher.hxx"
#include "Main/fg_props.hxx"
#include "StringInterpolation.hxx"
#include <string_view>

#include "MQTTPropertySubscription.hxx"

namespace flightgear {

MQTTSystem::MQTTSystem() : _rootNode(fgGetNode("/sim/mqtt", true))
{
}

MQTTSystem::~MQTTSystem()
{
    _mqttClients.clear();
}

void MQTTSystem::init()
{
    SG_LOG(SG_NETWORK, SG_INFO, "[MQTT] initializing from " << _rootNode->getPath());
    _mqttClients.clear();

    // configure each /broker node
    for (auto broker : _rootNode->getChildren("broker")) {
        if (broker->hasChild("enabled") && !broker->getBoolValue("enabled")) {
            SG_LOG(SG_NETWORK, SG_WARN, "[MQTT] broker " << broker->getPath() << " is disabled, skipping");
            continue;
        }
        SG_LOG(SG_NETWORK, SG_INFO, "[MQTT] creating client from " << broker->getPath());
        auto props = std::make_shared<MQTTClientOptionsProps>(broker);
        auto client = std::make_shared<MQTTClient>(props);

        if (broker->hasChild("publish")) {
            // publish all topics in /broker/publish/topic
            for (auto topic : broker->getNode("publish")->getChildren("topic")) {
                if (topic->hasChild("enabled") && !topic->getBoolValue("enabled")) {
                    SG_LOG(SG_NETWORK, SG_WARN, "[MQTT] topic " << topic->getPath() << " is disabled, skipping");
                    continue;
                }

                if (!topic->hasChild("payload")) {
                    SG_LOG(SG_NETWORK, SG_WARN, "[MQTT] topic " << topic->getPath() << " has no payload, skipping");
                    continue;
                }
                if (topic->getChildren("payload").size() > 1) {
                    SG_LOG(SG_NETWORK, SG_WARN, "[MQTT] topic " << topic->getPath() << " has multiple payloads, skipping");
                    continue;
                }

                int qos = topic->getIntValue("qos", 0);
                bool retain = topic->getBoolValue("retain", false);
                std::string topicName = simgear::strutils::strip(StringInterpolator::interpolate(topic->getStringValue("name", "")));
                if (topicName.empty()) {
                    SG_LOG(SG_NETWORK, SG_WARN, "[MQTT] topic name at " << topic->getPath() << " is empty, skipping");
                    continue;
                }

                // configure all property payloads under /broker/publish/topic/payload/property
                for (auto propertyPayload : topic->getNode("payload")->getChildren("property")) {
                    std::string propertyName;
                    bool recursive = false;
                    if (propertyPayload->hasChild("name")) {
                        propertyName = propertyPayload->getStringValue("name");
                        recursive = propertyPayload->getBoolValue("recursive", false);
                    } else {
                        propertyName = propertyPayload->getStringValue();
                    }
                    propertyName = simgear::strutils::strip(propertyName);
                    if (propertyName.empty()) {
                        SG_LOG(SG_NETWORK, SG_WARN, "[MQTT] property name is empty, skipping");
                        continue;
                    }

                    auto pp = std::make_shared<MQTTPropertyPublisher>(client, topicName, propertyName, recursive, qos, retain);
                    _mqttPublishers.push_back(pp);
                }

                // configure the templated payloads under /broker/publish/topic/payload/template
                for (auto payloadTemplate : topic->getNode("payload")->getChildren("template")) {
                    auto tp = std::make_shared<MQTTTemplatePublisher>(client, topicName, payloadTemplate->getStringValue(), qos, retain);
                    _mqttPublishers.push_back(tp);
                }
            }
        }

        if (broker->hasChild("subscribe")) {
            // subscribe to all topics in /broker/subscribe/topic
            for (auto topic : broker->getNode("subscribe")->getChildren("topic")) {
                if (topic->hasChild("enabled") && !topic->getBoolValue("enabled")) {
                    SG_LOG(SG_NETWORK, SG_WARN, "[MQTT] topic " << topic->getPath() << " is disabled, skipping");
                    continue;
                }
                int qos = topic->getIntValue("qos", 0);
                std::string topicName = simgear::strutils::strip(StringInterpolator::interpolate(topic->getStringValue("name", "")));
                if (topicName.empty()) {
                    SG_LOG(SG_NETWORK, SG_WARN, "[MQTT] topic name at " << topic->getPath() << " is empty, skipping");
                    continue;
                }

                for (auto target : topic->getChildren("target")) {
                    try {
                        auto subscription = std::make_shared<MQTTPropertySubscription>(client, topicName, target->getStringValue("property"), qos);
                        _mqttPropertySubscriptions.push_back(subscription);
                        client->subscribe(topicName, subscription.get(), qos);
                    }
                    catch (const std::exception& e) {
                        // subscribing might fail due to e.g. invalid topic filters
                        SG_LOG(SG_NETWORK, SG_WARN, "[MQTT] failed to subscribe to " << topicName << " : " << e.what());
                    }
                }
            }
        }

        _mqttClients.push_back(client);
        client->connect();
    }
}

void MQTTSystem::bind()
{
    SG_LOG(SG_NETWORK, SG_DEBUG, "[MQTT] binding from " << _rootNode->getPath());
    fgTie(_rootNode->getNode("serviceable", true)->getPath().c_str(), this,
          &MQTTSystem::is_serviceable, &MQTTSystem::set_serviceable);
}

void MQTTSystem::unbind()
{
    SG_LOG(SG_NETWORK, SG_DEBUG, "[MQTT] unbinding from " << _rootNode->getPath());
    _rootNode->untie("serviceable");
}

void MQTTSystem::update(double dt)
{
    for (auto& mqttClient : _mqttClients) {
        mqttClient->tick();
    }
}

} // namespace flightgear
