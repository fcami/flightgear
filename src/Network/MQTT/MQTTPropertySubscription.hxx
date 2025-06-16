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
    MQTTPropertySubscription(std::shared_ptr<IMQTTClient> client, const std::string topicFilter, const std::string targetPropertyName, int qos = 0)
        : _client(client),
          _targetProperty(fgGetNode(targetPropertyName, true)),
          _topicFilter(topicFilter),
          _qos(qos)
    {
        if (!isValidTopicFilter(topicFilter))
            throw std::invalid_argument("Invalid topic filter: " + topicFilter);

        // compute the static length of the topics prefix which is everything before
        // the wildcard characters # or +
        size_t i = 0;
        while (i < _topicFilter.size()) {
            size_t j = i;
            while (j < _topicFilter.size() && _topicFilter[j] != '/') ++j;

            size_t segLen = j - i;
            if ((segLen == 1 && (_topicFilter[i] == '+' || _topicFilter[i] == '#')))
                break;

            i = (j < _topicFilter.size()) ? j + 1 : j;
        }
        _prefixLength = i;
    }

    virtual void onMessage(std::shared_ptr<IMQTTTopic> topic) override
    {
        SG_LOG(SG_NETWORK, SG_DEBUG, "[MQTT] MQTTPropertySubscription::onMessage: " << topic->getName() << " = " << topic->getPayload());
        std::string childNodeName = topic->getName().substr(std::min(_prefixLength, topic->getName().size()));
        _targetProperty->getNode(childNodeName, true)->setStringValue(topic->getPayload());
    }

    static bool isValidTopicFilter(std::string_view filter)
    {
        if (filter.empty()) return false;

        size_t start = 0;
        while (start <= filter.size()) {
            size_t end = filter.find('/', start);
            if (end == std::string_view::npos)
                end = filter.size();

            std::string_view token = filter.substr(start, end - start);

            if (token == "#") {
                // '#' must be the last token
                return end == filter.size();
            } else if (token.find('#') != std::string_view::npos) {
                // '#' must not be part of a token
                return false;
            }

            if (token != "+" && token.find('+') != std::string_view::npos) {
                // '+' must be exactly '+', not part of another token
                return false;
            }

            start = (end < filter.size()) ? end + 1 : filter.size() + 1;
        }

        return true;
    }


    virtual std::string getTopicName() const override { return _topicFilter; }
    virtual int getQos() const override { return _qos; }

private:
    MQTTPropertySubscription();
    std::shared_ptr<IMQTTClient> _client;
    SGPropertyNode_ptr _targetProperty;
    const std::string _topicFilter;
    int _qos = 0;
    size_t _prefixLength = 0;
};

} // namespace flightgear
