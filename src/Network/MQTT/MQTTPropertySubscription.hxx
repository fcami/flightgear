/*
 * SPDX-FileName: MQTTSystem.cxx
 * SPDX-FileComment: mqtt subsystem class
 * SPDX-FileCopyrightText: Copyright (C) 2025 Torsten Dreyer - torsten (at) t3r (dot) de
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once
#include "MQTTClient.hxx"
#include "Main/fg_props.hxx"

namespace flightgear {

class MQTTPropertySubscription : public IMQTTTopicListener
{
public:
    MQTTPropertySubscription(std::shared_ptr<IMQTTClient> client, const std::string topicName, const std::string targetPropertyName, int qos = 0)
        : _client(client),
          _targetProperty(fgGetNode(targetPropertyName, true)),
          _topicName(topicName),
          _qos(qos)
    {
        // compute the static length of the topics prefix which is everything before
        // the wildcard characters # or +
        size_t i = 0;
        while (i < _topicName.size()) {
            size_t j = i;
            while (j < _topicName.size() && _topicName[j] != '/') ++j;

            size_t segLen = j - i;
            if ((segLen == 1 && (_topicName[i] == '+' || _topicName[i] == '#')))
                break;

            i = (j < _topicName.size()) ? j + 1 : j;
        }
        _prefixLength = i;
    }

    virtual void onMessage(std::shared_ptr<IMQTTTopic> topic) override
    {
        SG_LOG(SG_NETWORK, SG_DEBUG, "[MQTT] MQTTPropertySubscription::onMessage: " << topic->getName() << " = " << topic->getPayload());
        std::string childNodeName = topic->getName().substr(std::min(_prefixLength, topic->getName().size()));
        _targetProperty->getNode(childNodeName, true)->setStringValue(topic->getPayload());
    }

    virtual std::string getTopicName() const override { return _topicName; }
    virtual int getQos() const override { return _qos; }

private:
    MQTTPropertySubscription();
    std::shared_ptr<IMQTTClient> _client;
    SGPropertyNode_ptr _targetProperty;
    const std::string _topicName;
    int _qos = 0;
    size_t _prefixLength = 0;
};

}
