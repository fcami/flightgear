/*
 * SPDX-FileCopyrightText: (C) 2022 Lars Toenning <dev@ltoenning.de>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "test_MQTT.hxx"

#include "test_suite/FGTestApi/testGlobals.hxx"
#include <Scripting/NasalSys.hxx>


#include "Network/MQTT/MQTTClient.hxx"
#include "Network/MQTT/MQTTPropertyPublisher.hxx"
#include "Network/MQTT/MQTTPropertySubscription.hxx"
#include "Network/MQTT/MQTTTemplatePublisher.hxx"

#include <Main/fg_props.hxx>
#include <Main/util.hxx>

class MockClient : public IMQTTClient
{
public:
    virtual void tick()
    {
        functionCalls["tick"]++;
    }

    virtual void connect()
    {
        functionCalls["connect"]++;
    }
    virtual void disconnect()
    {
        functionCalls["disconnect"]++;
    }
    virtual void publish(std::shared_ptr<IMQTTTopic> t)
    {
        functionCalls["publish"]++;
        publishedTopic = std::move(t);
    }

    virtual void subscribe(const std::string& topic, IMQTTTopicListener* topicListener, int qos = 0)
    {
        functionCalls["subscribe"]++;
    }

    virtual void unsubscribe(IMQTTTopicListener* topicListener)
    {
        functionCalls["unsubscribe"]++;
    }

    std::shared_ptr<IMQTTTopic> publishedTopic = std::make_shared<MQTTTopic>("unset", "unset", -1, false);

    std::unordered_map<std::string, unsigned> functionCalls = {
        {"tick", 0},
        {"connect", 0},
        {"disconnect", 0},
        {"publish", 0},
        {"subscribe", 0},
        {"unsubscribe", 0}};
};

void MQTTTest::setUp()
{
    FGTestApi::setUp::initTestGlobals("MQTT");
}

void MQTTTest::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}

void MQTTTest::testPropertyPublisherRecursive()
{
    auto mockClient = std::make_shared<MockClient>();
    auto templatePublisher = std::make_shared<flightgear::MQTTPropertyPublisher>(
        mockClient,
        "/base",
        "/test/double-value",
        true, // recursive
        2,    // qos
        true  // retain
    );

    // setting the node should trigger a publish
    fgSetDouble("/test/double-value", 3.14);
    CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(1), mockClient->functionCalls["publish"]);
    CPPUNIT_ASSERT_EQUAL(std::string("3.14"), mockClient->publishedTopic->getPayload());
    CPPUNIT_ASSERT_EQUAL(std::string("/base/test/double-value"), mockClient->publishedTopic->getName());
    CPPUNIT_ASSERT_EQUAL(2, mockClient->publishedTopic->getQos());
    CPPUNIT_ASSERT_EQUAL(true, mockClient->publishedTopic->isRetained());

    // setting a child-node should trigger a publish
    fgSetDouble("/test/double-value/something", 42);
    CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(2), mockClient->functionCalls["publish"]);
    CPPUNIT_ASSERT_EQUAL(std::string("42"), mockClient->publishedTopic->getPayload());
    CPPUNIT_ASSERT_EQUAL(std::string("/base/test/double-value/something"), mockClient->publishedTopic->getName());
}


void MQTTTest::testPropertyPublisherNonrecursive()
{
    auto mockClient = std::make_shared<MockClient>();
    auto templatePublisher = std::make_shared<flightgear::MQTTPropertyPublisher>(
        mockClient,
        "/base",
        "/test/double-value",
        false, // recursive
        2,     // qos
        true   // retain
    );

    // setting the node should trigger a publish
    fgSetDouble("/test/double-value", 3.14);
    CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(1), mockClient->functionCalls["publish"]);
    CPPUNIT_ASSERT_EQUAL(std::string("3.14"), mockClient->publishedTopic->getPayload());
    CPPUNIT_ASSERT_EQUAL(std::string("/base/test/double-value"), mockClient->publishedTopic->getName());
    CPPUNIT_ASSERT_EQUAL(2, mockClient->publishedTopic->getQos());
    CPPUNIT_ASSERT_EQUAL(true, mockClient->publishedTopic->isRetained());

    // setting a child-node should NOT trigger a publish
    fgSetDouble("/test/double-value/something", 42);
    CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(1), mockClient->functionCalls["publish"]);
    CPPUNIT_ASSERT_EQUAL(std::string("3.14"), mockClient->publishedTopic->getPayload());
}

void MQTTTest::testTemplatePublisher()
{
    fgSetDouble("/test/double-value", 3.14);

    auto mockClient = std::make_shared<MockClient>();
    auto templatePublisher = std::make_shared<flightgear::MQTTTemplatePublisher>(
        mockClient,
        "test-topic",
        "Hello ${prop:/test/double-value}!", 2, true);
    templatePublisher->tick();

    CPPUNIT_ASSERT_EQUAL(std::string("Hello 3.14!"), mockClient->publishedTopic->getPayload());
    CPPUNIT_ASSERT_EQUAL(std::string("test-topic"), mockClient->publishedTopic->getName());
    CPPUNIT_ASSERT_EQUAL(2, mockClient->publishedTopic->getQos());
    CPPUNIT_ASSERT_EQUAL(true, mockClient->publishedTopic->isRetained());
}

void MQTTTest::testPropertySubscription()
{
    auto mockClient = std::make_shared<MockClient>();

    auto propertySubscription = std::make_shared<flightgear::MQTTPropertySubscription>(
        mockClient,
        "/test/#", //subscribe /everything below /test
        "/target", // target property
        0);

    propertySubscription->getTopicName(), propertySubscription.get();
    auto topic = std::make_shared<MQTTTopic>("/test/foo/bar", "42", 0, false);

    propertySubscription->onMessage(topic);

    CPPUNIT_ASSERT_DOUBLES_EQUAL(42.0, fgGetDouble("/target/foo/bar"), 0.0);
}

void MQTTTest::testTopicFilterMatcher()
{
    auto topic = std::make_shared<MQTTTopic>("/test/topic", "payload", 0, false);

    CPPUNIT_ASSERT_EQUAL(true, topic->matchesFilter("/test/topic"));
    CPPUNIT_ASSERT_EQUAL(true, topic->matchesFilter("/test/#"));
    CPPUNIT_ASSERT_EQUAL(true, topic->matchesFilter("/test/+"));
    CPPUNIT_ASSERT_EQUAL(false, topic->matchesFilter("/test/+/foo"));
    CPPUNIT_ASSERT_EQUAL(false, topic->matchesFilter("/test"));
    CPPUNIT_ASSERT_EQUAL(false, topic->matchesFilter("/test/"));
    CPPUNIT_ASSERT_EQUAL(false, topic->matchesFilter("/test/+foo"));
    CPPUNIT_ASSERT_EQUAL(false, topic->matchesFilter("/test/+/foo/bar"));
    CPPUNIT_ASSERT_EQUAL(false, topic->matchesFilter("/test/+/foo/bar/baz"));
    CPPUNIT_ASSERT_EQUAL(false, topic->matchesFilter("/test/+/foo/bar/baz/"));
    CPPUNIT_ASSERT_EQUAL(false, topic->matchesFilter("/test/+/foo/bar/baz/bla"));
}

void MQTTTest::testTopicFilterValidator()
{
    using namespace flightgear;
    CPPUNIT_ASSERT(MQTTPropertySubscription::isValidTopicFilter("sensor/temperature"));
    CPPUNIT_ASSERT(MQTTPropertySubscription::isValidTopicFilter("/home/livingroom"));
    CPPUNIT_ASSERT(MQTTPropertySubscription::isValidTopicFilter("/test/+/status"));
    CPPUNIT_ASSERT(MQTTPropertySubscription::isValidTopicFilter("/test/#"));
    CPPUNIT_ASSERT(MQTTPropertySubscription::isValidTopicFilter("+"));
    CPPUNIT_ASSERT(MQTTPropertySubscription::isValidTopicFilter("#"));
    CPPUNIT_ASSERT(MQTTPropertySubscription::isValidTopicFilter("/+"));
    CPPUNIT_ASSERT(MQTTPropertySubscription::isValidTopicFilter("/#"));
    CPPUNIT_ASSERT(MQTTPropertySubscription::isValidTopicFilter("foo/+"));
    CPPUNIT_ASSERT(MQTTPropertySubscription::isValidTopicFilter("+/bar"));
    CPPUNIT_ASSERT(MQTTPropertySubscription::isValidTopicFilter("foo/+/bar"));
    CPPUNIT_ASSERT(MQTTPropertySubscription::isValidTopicFilter("/foo/bar"));
    CPPUNIT_ASSERT(MQTTPropertySubscription::isValidTopicFilter("foo/bar/"));
    CPPUNIT_ASSERT(MQTTPropertySubscription::isValidTopicFilter("/foo/bar/"));
    CPPUNIT_ASSERT(MQTTPropertySubscription::isValidTopicFilter("//foo///bar//"));


    // invalid filters
    CPPUNIT_ASSERT(!MQTTPropertySubscription::isValidTopicFilter("foo/#extra"));
    CPPUNIT_ASSERT(!MQTTPropertySubscription::isValidTopicFilter("foo/extra#"));
    CPPUNIT_ASSERT(!MQTTPropertySubscription::isValidTopicFilter(""));
    CPPUNIT_ASSERT(!MQTTPropertySubscription::isValidTopicFilter("foo/#/#"));
    CPPUNIT_ASSERT(!MQTTPropertySubscription::isValidTopicFilter("#/#"));
}
