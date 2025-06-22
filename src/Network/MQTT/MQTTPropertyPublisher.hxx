/*
 * SPDX-FileName: MQTTSystem.cxx
 * SPDX-FileComment: mqtt subsystem class
 * SPDX-FileCopyrightText: Copyright (C) 2025 Torsten Dreyer - torsten (at) t3r (dot) de
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once
#include "MQTTPublisher.hxx"
#include "Main/fg_props.hxx"

namespace flightgear {

class MQTTPropertyPublisher : public SGPropertyChangeListener, public MQTTPublisher
{
public:
    MQTTPropertyPublisher(std::shared_ptr<IMQTTClient> client, const std::string topicBase, const std::string propertyName, bool recursive, int qos = 0, bool retain = false)
        : MQTTPublisher(client, topicBase, qos, retain),
          _recursive(recursive)
    {
        _name = "/" + trimSlashes(topicBase);
        _propNode = fgGetNode(propertyName, true);
        _propNode->addChangeListener(this, false);
    }

    virtual ~MQTTPropertyPublisher()
    {
        for (auto n : _listenList) {
            n->removeChangeListener(this);
        }
        _propNode->removeChangeListener(this);
    }

    virtual void valueChanged(SGPropertyNode* node) override
    {
        auto payload = node->getStringValue();
        // _name is sanitized in the constructor and never ends with a slash. Property path does start
        // with a slash so our topic name will be good by just concatenating name and path
        auto topic = std::make_shared<MQTTTopic>(_name + node->getPath(true), payload, _qos, _retain);
        _client->publish(topic);
    }

    virtual void childAdded(SGPropertyNode* parent, SGPropertyNode* child) override
    {
        if (_recursive) {
            SG_LOG(SG_NETWORK, SG_DEBUG, "[MQTT] Child " << child->getPath() << " added to parent " << parent->getPath());
            child->addChangeListener(this, false);
            _listenList.push_back(child);
        }
    }

    virtual void childRemoved(SGPropertyNode* parent, SGPropertyNode* child) override
    {
        if (_recursive) {
            SG_LOG(SG_NETWORK, SG_DEBUG, "[MQTT] Child " << child->getPath() << " removed from parent " << parent->getPath());
            child->removeChangeListener(this);
            _listenList.erase(std::remove(_listenList.begin(), _listenList.end(), child), _listenList.end());
        }
    }

    virtual void tick() override {} // noop, this publishes on change

private:
    static inline std::string trimSlashes(const std::string& s)
    {
        auto start = s.find_first_not_of('/');
        auto end = s.find_last_not_of('/');
        return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    }

    MQTTPropertyPublisher();
    bool _recursive;
    SGPropertyNode_ptr _propNode;
    std::vector<SGPropertyNode_ptr> _listenList;
};
} // namespace flightgear