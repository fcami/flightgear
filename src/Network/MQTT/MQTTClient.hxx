/*
 * SPDX-FileName: MQTTClient.hxx
 * SPDX-FileComment: wrap paho mqtt client for the MQTT subsystem
 * SPDX-FileCopyrightText: Copyright (C) 2025 Torsten Dreyer - torsten (at) t3r (dot) de
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <string>
#include "simgear/threads/SGQueue.hxx"
#include "EventEmitter.hxx"
namespace mqtt {
    class async_client;
    class callback;
    class token;
    using token_ptr = std::shared_ptr<token>;
}

class IMQTTClientOptions
{
public:
    virtual ~IMQTTClientOptions() = default;

    // Getters
    virtual std::string getBrokerUri() const = 0;
    virtual std::string getClientId() const = 0;
    virtual bool isCleanSession() const = 0;
    virtual int getMaxInflight() const = 0;
    virtual int getMqttVersion() const = 0;
    virtual bool isAutomaticReconnect() const = 0;
    virtual int getConnectTimeout() const = 0;
    virtual int getKeepAliveInterval() const = 0;
    virtual int getMinRetryInterval() const = 0;
    virtual int getMaxRetryInterval() const = 0;
    virtual bool isSSL() const = 0;
    virtual std::string getTrustStore() const = 0;
    virtual std::string getKeyStore() const = 0;
    virtual std::string getPrivateKey() const = 0;
    virtual std::string getPrivateKeyPassword() const = 0;
    virtual std::string getUserName() const = 0;
    virtual std::string getPassword() const = 0;
    virtual bool isUniquePayload() const = 0;
};

class IMQTTTopic
{
public:
    virtual ~IMQTTTopic() = default;
    virtual std::string getName() const = 0;
    virtual int getQos() const = 0;
    virtual bool isRetained() const = 0;
    virtual std::string getPayload() const = 0;
    virtual bool matchesFilter(std::string_view filter) = 0;
};

class MQTTTopic : public IMQTTTopic
{
public:
    MQTTTopic(std::string name, std::string payload, int qos, bool retained) : _name(std::move(name)), _payload(std::move(payload)), _qos(qos), _retained(retained) {}
    ~MQTTTopic() = default;
    std::string getName() const override
    {
        return _name;
    }
    int getQos() const override
    {
        return _qos;
    }
    bool isRetained() const override
    {
        return _retained;
    }
    std::string getPayload() const override
    {
        return _payload;
    }

    void setPayload(std::string payload)
    {
        _payload = std::move(payload);
    }
    void setQos(int qos)
    {
        _qos = qos;
    }
    void setRetained(bool retained)
    {
        _retained = retained;
    }
    void setName(std::string name)
    {
        _name = std::move(name);
    }

    bool matchesFilter(std::string_view filter);

private:
    std::string _name;
    std::string _payload;
    int _qos;
    bool _retained = false;
};

class IMQTTTopicListener
{
public:
    virtual ~IMQTTTopicListener() = default;
    virtual void onMessage(std::shared_ptr<IMQTTTopic>) = 0; // called when a message arrives
    virtual std::string getTopicName() const = 0;
    virtual int getQos() const = 0;
};

class IMQTTClient
{
public:
    virtual ~IMQTTClient() = default;
    virtual void tick() = 0; // called periodically to handle MQTT client operations

    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual void publish(std::shared_ptr<IMQTTTopic>) = 0;

    virtual void subscribe(const std::string& topic, IMQTTTopicListener* topicListener, int qos = 0) = 0;
    virtual void unsubscribe(IMQTTTopicListener* topicListener) = 0;

    virtual void onConnect() = 0;
    virtual void onDisconnect() = 0;
};
class MQTTClientCallback;

class MQTTClient : public IMQTTClient
{
public:
    MQTTClient(std::shared_ptr<IMQTTClientOptions> options);
    ~MQTTClient();

    void tick() override;
    void connect() override;
    void disconnect() override;
    void publish(std::shared_ptr<IMQTTTopic>) override;
    virtual void subscribe(const std::string& topic, IMQTTTopicListener* topicListener, int qos = 0) override;
    virtual void unsubscribe(IMQTTTopicListener* topicListener) override;
    EventEmitter::ListenerID onConnect(EventEmitter::Listener);
    EventEmitter::ListenerID onDisconnect(EventEmitter::Listener);

    static std::int64_t getCurrentTimestampMillis();
    static std::string getTimestamp();

    friend class MQTTClientCallback;
protected:
    virtual void onConnect();
    virtual void onDisconnect();
    bool queueMessage(std::shared_ptr<IMQTTTopic> topic);
    void subscribe(const std::string& topic, int qos = 0);

private:
    MQTTClient();
    std::shared_ptr<IMQTTClientOptions> _options;
    std::unique_ptr<mqtt::async_client> _client;
    std::vector<mqtt::token_ptr> tokens;
    std::unordered_map<std::string, std::string> _payloads;
    unsigned maxQueueLength = 1000; // max queue length for _inMessages
    SGLockedQueue<std::shared_ptr<IMQTTTopic>> _inMessages;
    std::vector<IMQTTTopicListener*> _listeners;

    std::unique_ptr<MQTTClientCallback> _callback;
    EventEmitter _eventEmitter;
};