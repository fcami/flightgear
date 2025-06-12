/*
 * SPDX-FileName: MQTTClient.cxx
 * SPDX-FileComment: wrap paho mqtt client for the MQTT subsystem
 * SPDX-FileCopyrightText: Copyright (C) 2025 Torsten Dreyer - torsten (at) t3r (dot) de
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MQTTClient.hxx"
#include <simgear/threads/SGQueue.hxx>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <mqtt/async_client.h>
#include <simgear/debug/logstream.hxx>

class MQTTClientImpl : public virtual mqtt::callback
{
public:
    MQTTClientImpl(std::shared_ptr<IMQTTClientOptions> options) : _options(std::move(options))
    {
    }

    virtual ~MQTTClientImpl();

    void tick();

    void connect();
    void disconnect();
    void publish(std::shared_ptr<IMQTTTopic>);

    void message_arrived(mqtt::const_message_ptr msg) override
    {
        SG_LOG(SG_NETWORK, SG_DEBUG, "[MQTT] Message arrived: " << msg->get_topic() << " -> " << msg->to_string());
        auto topic = std::make_shared<MQTTTopic>(msg->get_topic(), msg->to_string(), msg->get_qos(), msg->is_retained());
        _inMessages.push(topic);
    }

    void connected(const std::string& cause) override
    {
        SG_LOG(SG_NETWORK, SG_INFO, "[MQTT] Connected to " << _options->getBrokerUri() << " as " << _client->get_client_id() << " : " << cause);
        SG_LOG(SG_NETWORK, SG_DEBUG, "[MQTT] Subscribing to " << _listeners.size() << " topics");
        for( auto topic : _listeners ) {
            SG_LOG(SG_NETWORK, SG_INFO, "[MQTT] Subscribing to " << topic->getTopicName());
            tokens.push_back(_client->subscribe(topic->getTopicName(), topic->getQos()));
        }
    }

    void connection_lost(const std::string& cause) override
    {
        SG_LOG(SG_NETWORK, SG_INFO, "[MQTT] Connection lost to " << _options->getBrokerUri() << " as " << _client->get_client_id() << " : " << cause);
    }

    void subscribe(const std::string& topic, IMQTTTopicListener* topicListener, int qos);
    void unsubscribe(IMQTTTopicListener* topicListener);


private:
    std::shared_ptr<IMQTTClientOptions> _options;
    std::unique_ptr<mqtt::async_client> _client;
    std::vector<mqtt::token_ptr> tokens;
    std::unordered_map<std::string, std::string> _payloads;
    SGLockedQueue<std::shared_ptr<IMQTTTopic>> _inMessages;
    std::vector<IMQTTTopicListener*> _listeners;
};

MQTTClientImpl::~MQTTClientImpl()
{
    /*
    Intentionally not unsubsscribing from topics. reconsider this if
    We're using persistent sessions and want to remove specific subscriptions.
    We want to explicitly control what the broker retains between sessions.
    We’re using MQTT 5 with session expiry and want to fine-tune behavior on the broker side.
    */
    if (_client && _client->is_connected()) {
        try {
            _client->disconnect()->wait(); // wait for disconnect to complete
        } catch (const mqtt::exception& e) {
            SG_LOG(SG_NETWORK, SG_ALERT, "[MQTT] Disconnect failed: " << e.what());
        }
    }
}

bool topicMatchesFilter(std::string_view topic, std::string_view filter)
{
    size_t ti = 0, fi = 0;

    while (ti < topic.size() && fi < filter.size()) {
        // Get topic token
        size_t tStart = ti, tEnd = ti;
        while (tEnd < topic.size() && topic[tEnd] != '/') ++tEnd;
        std::string_view tToken = topic.substr(tStart, tEnd - tStart);

        // Get filter token
        size_t fStart = fi, fEnd = fi;
        while (fEnd < filter.size() && filter[fEnd] != '/') ++fEnd;
        std::string_view fToken = filter.substr(fStart, fEnd - fStart);

        if (fToken == "#") return true;
        if (fToken != "+" && fToken != tToken) return false;

        // Advance
        ti = (tEnd < topic.size()) ? tEnd + 1 : topic.size();
        fi = (fEnd < filter.size()) ? fEnd + 1 : filter.size();
    }

    // Handle end of topic/filter
    // Remaining filter segment is '#' only
    if (fi < filter.size()) {
        if (filter.substr(fi) == "#") return true;
    }

    return ti == topic.size() && fi == filter.size();
}


void MQTTClientImpl::tick()
{
    std::size_t n = tokens.size();
    tokens.erase(
        std::remove_if(tokens.begin(), tokens.end(),
                       [](const mqtt::token_ptr& tok) {
                           return tok->is_complete();
                       }),
        tokens.end());
    std::size_t n2 = tokens.size();

    // Complain if we have a lot of tokens left
    if (n2 > 100) {
        SG_LOG(SG_NETWORK, SG_ALERT, "[MQTT] " << (n - n2) << " of " << n << " tokens completed, " << n2 << " remaining");
    }

    if (n2 != n) {
        SG_LOG(SG_NETWORK, SG_DEBUG, "[MQTT] " << (n - n2) << " of " << n << " tokens completed, " << n2 << " remaining");
    }

    for (auto i = 0; i < 10; i++) {
        if( _inMessages.empty() )
            break;

        auto topic = _inMessages.pop();
        SG_LOG(SG_NETWORK, SG_DEBUG, "[MQTT] Message received from: " << topic->getName());

        // distribute to listeners
        for (auto listener : _listeners) {
            // check if the topic matches the listeners filter
            if (topicMatchesFilter(topic->getName(), listener->getTopicName())) {
                listener->onMessage(topic);
            }
        }
    }
}

void MQTTClientImpl::connect()
{
    try {
        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        auto clientId = _options->getClientId() + "-" + boost::uuids::to_string(uuid);
        SG_LOG(SG_NETWORK, SG_INFO, "[MQTT] Connecting to " << _options->getBrokerUri() << " as " << clientId);

        _client = std::make_unique<mqtt::async_client>(_options->getBrokerUri(), clientId);
        mqtt::connect_options connOpts;

        connOpts.set_clean_session(_options->isCleanSession());
        connOpts.set_max_inflight(_options->getMaxInflight());
        connOpts.set_mqtt_version(_options->getMqttVersion());
        if (_options->isAutomaticReconnect()) {
            connOpts.set_automatic_reconnect(true);
            connOpts.set_automatic_reconnect(_options->getMinRetryInterval(), _options->getMaxRetryInterval());
        } else {
            connOpts.set_automatic_reconnect(false);
        }

        connOpts.set_connect_timeout(_options->getConnectTimeout());
        connOpts.set_keep_alive_interval(_options->getKeepAliveInterval());

        connOpts.set_user_name(_options->getUserName());
        connOpts.set_password(_options->getPassword());

        if (_options->isSSL()) {
            mqtt::ssl_options sslOpts;
            sslOpts.set_trust_store(_options->getTrustStore());
            sslOpts.set_key_store(_options->getKeyStore());
            sslOpts.set_private_key(_options->getPrivateKey());
            sslOpts.set_private_key_password(_options->getPrivateKeyPassword());
            connOpts.set_ssl(sslOpts);
            SG_LOG(SG_NETWORK, SG_INFO, "[MQTT] using SSL trustStore: " << sslOpts.get_trust_store() << " keyStore: " << sslOpts.get_key_store() << " privateKey: " << sslOpts.get_private_key());
        }

        _client->set_callback(*this);
        tokens.push_back(_client->connect(connOpts));

        SG_LOG(SG_NETWORK, SG_DEBUG, "[MQTT] Connection initiated to " << _options->getBrokerUri() << " as " << _client->get_client_id());
    } catch (const mqtt::exception& e) {
        SG_LOG(SG_NETWORK, SG_ALERT, "[MQTT] Connection failed: " << e.what());
    }
}

void MQTTClientImpl::disconnect()
{
    SG_LOG(SG_NETWORK, SG_INFO, "[MQTT] Client disconnecting...");
    if (_client && _client->is_connected()) {
        try {
            auto token = _client->disconnect();
            tokens.push_back(token);
        } catch (const mqtt::exception& e) {
            SG_LOG(SG_NETWORK, SG_ALERT, "[MQTT] Disconnect failed: " << e.what());
        }
    } else {
        SG_LOG(SG_NETWORK, SG_WARN, "[MQTT] Client not connected, cannot disconnect");
    }
}

void MQTTClientImpl::publish(std::shared_ptr<IMQTTTopic> topic)
{
    if (false == (_client && _client->is_connected())) {
        SG_LOG(SG_NETWORK, SG_WARN, "[MQTT] Client not connected, cannot publish");
        return;
    };

    try {
        auto p = topic->getPayload();
        auto t = topic->getName();

        if (_options->isUniquePayload()) {
            // If unique payloads are enabled, we store the payload to avoid duplicates
            if (_payloads.find(t) != _payloads.end() && _payloads[t] == p) {
                return;
            }
            _payloads[t] = p; // Store the payload
        }

        auto message = mqtt::make_message(t, p);
        message->set_qos(topic->getQos());
        message->set_retained(topic->isRetained());
        mqtt::properties props{
            mqtt::property(mqtt::property::USER_PROPERTY, "ts", MQTTClient::getTimestamp())};
        message->set_properties(props);
        auto token = _client->publish(message);
        tokens.push_back(token);
        SG_LOG(SG_NETWORK, SG_DEBUG, "[MQTT] Published to " << topic->getName() << " with QoS " << topic->getQos() << " Retained: " << topic->isRetained());
    } catch (const mqtt::exception& e) {
        SG_LOG(SG_NETWORK, SG_ALERT, "[MQTT] Publishing to " << topic->getName() << "failed: " << e.what());
    }
}
void MQTTClientImpl::subscribe(const std::string& topic, IMQTTTopicListener* topicListener, int qos)
{
    SG_LOG(SG_NETWORK, SG_DEBUG, "[MQTT] Subscribing to " << topic << " with QoS " << qos);
    if (std ::find(_listeners.begin(), _listeners.end(), topicListener) != _listeners.end()) {
        SG_LOG(SG_NETWORK, SG_WARN, "[MQTT] Topic listener already subscribed to " << topic);
        return;
    } else {
        _listeners.push_back(topicListener);
        // defer sending the subscription to the client, need to be connected first
        // this also makes sure that the subscription gets renewed upon automatic reconnects to the broker
        // do not to this here --> tokens.push_back(_client->subscribe(topic, qos));
    }
}

void MQTTClientImpl::unsubscribe(IMQTTTopicListener* topicListener)
{
    if( _client && _client->is_connected() ) {
        SG_LOG(SG_NETWORK, SG_DEBUG, "[MQTT] Client connected, unsubscribing from topic " << topicListener->getTopicName());
        tokens.push_back(_client->unsubscribe(topicListener->getTopicName()));
    } else {
        SG_LOG(SG_NETWORK, SG_WARN, "[MQTT] Client not connected, cannot unsubscribe from topic " << topicListener->getTopicName());
    }
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), topicListener), _listeners.end());
}


MQTTClient::MQTTClient(std::shared_ptr<IMQTTClientOptions> options)
{
    impl = std::make_unique<MQTTClientImpl>(options);
}

MQTTClient::~MQTTClient()
{
}

void MQTTClient::tick()
{
    impl->tick();
}

void MQTTClient::connect()
{
    impl->connect();
}

void MQTTClient::disconnect()
{
    impl->disconnect();
}

void MQTTClient::publish(std::shared_ptr<IMQTTTopic> topic)
{
    impl->publish(topic);
}

void MQTTClient::subscribe(const std::string& topic, IMQTTTopicListener* topicListener, int qos)
{
    impl->subscribe(topic, topicListener, qos);
}
void MQTTClient::unsubscribe(IMQTTTopicListener* topicListener)
{
    impl->unsubscribe(topicListener);
}

std::int64_t MQTTClient::getCurrentTimestampMillis()
{
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
    return millis;
}

std::string MQTTClient::getTimestamp()
{
    return std::to_string(getCurrentTimestampMillis());
}
