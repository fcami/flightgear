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


class MQTTClientOptionsProps : public IMQTTClientOptions
{
public:
    MQTTClientOptionsProps(SGPropertyNode_ptr rootNode) : _rootNode(rootNode) {}
    virtual ~MQTTClientOptionsProps() = default;

    std::string getBrokerUri() const override
    {
        return getStringValueOrEnv("uri");
    }
    std::string getClientId() const override
    {
        return simgear::strutils::strip(_rootNode->getStringValue("client-id", "FlightGear"));
    }
    bool isCleanSession() const override
    {
        return _rootNode->getBoolValue("clean-session", true);
    }
    int getMaxInflight() const override
    {
        return _rootNode->getIntValue("max-inflight", 100);
    }
    int getMqttVersion() const override
    {
        return _rootNode->getIntValue("mqtt-version", 5);
    }
    bool isAutomaticReconnect() const override
    {
        return _rootNode->hasChild("automatic-reconnect");
    }
    int getConnectTimeout() const override
    {
        return _rootNode->getIntValue("connect-timeout", 30);
    }
    int getMinRetryInterval() const override
    {
        return _rootNode->getIntValue("automatic-reconnect/min-retry-interval", 1);
    }
    int getMaxRetryInterval() const override
    {
        return _rootNode->getIntValue("automatic-reconnect/max-retry-interval", 10);
    }
    int getKeepAliveInterval() const override
    {
        return _rootNode->getIntValue("keep-alive-interval", 30);
    }

    bool isSSL() const override
    {
        return _rootNode->hasChild("ssl");
    }
    std::string getTrustStore() const override
    {
        return getStringValueOrEnv("ssl/trust-store");
    }
    std::string getKeyStore() const override
    {
        return getStringValueOrEnv("ssl/key-store");
    }
    std::string getPrivateKey() const override
    {
        return getStringValueOrEnv("ssl/private-key");
    }
    std::string getPrivateKeyPassword() const override
    {
        return getStringValueOrEnv("ssl/private-key-password");
    }

    std::string getUserName() const override
    {
        return getStringValueOrEnv("authentication/username");
    }
    std::string getPassword() const override
    {
        return getStringValueOrEnv("authentication/password");
    }
    bool isUniquePayload() const override
    {
        return _rootNode->getBoolValue("unique-payload", false);
    }

private:
    std::string getStringValueOrEnv(const std::string& key) const
    {
        auto keyNode = _rootNode->getNode(key, true);
        if (keyNode->hasChild("env")) {
            auto envVar = keyNode->getStringValue("env", "");
            auto p = std::getenv(envVar.c_str());
            return p ? std::string(p) : "";
        } else {
            return keyNode->getStringValue();
        }
    }

    SGPropertyNode_ptr _rootNode;
};
}