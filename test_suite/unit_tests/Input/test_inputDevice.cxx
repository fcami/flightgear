// SPDX-FileCopyrightText: (C) 2026 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "test_inputDevice.hxx"

#include <string>

#include "cppunit/TestAssert.h"
#include "test_suite/FGTestApi/testGlobals.hxx"

#include <Main/fg_os.hxx>
#include <Main/globals.hxx>

#include <Input/FGEventInput.hxx>

#include <simgear/structure/commands.hxx>

using namespace std::string_literals;

// ---------------------------------------------------------------------------
// Minimal concrete FGInputDevice used only for testing.
// TranslateEventName returns whatever name was last set via setEventName().
// ---------------------------------------------------------------------------
class TestInputDevice : public FGInputDevice
{
public:
    explicit TestInputDevice(const std::string& name) : FGInputDevice(name) {}

    bool Open() override { return true; }
    void Close() override {}
    void Send(const char* /*eventName*/, double /*value*/) override {}

    const char* TranslateEventName(FGEventData& /*eventData*/) override
    {
        return _translatedName.c_str();
    }

    void setEventName(const std::string& name) { _translatedName = name; }

    // Expose the protected deviceNode for test assertions
    SGPropertyNode* getDeviceNode() const { return deviceNode; }

private:
    std::string _translatedName;
};

// ---------------------------------------------------------------------------
// Helpers to build a device property node from an XML snippet and configure
// a TestInputDevice with it.  The device node lives in a detached property
// tree so tests don't pollute the global tree.
// ---------------------------------------------------------------------------
static SGSharedPtr<TestInputDevice> makeDevice(const std::string& name,
                                               const std::string& xmlSnippet)
{
    auto* device = new TestInputDevice(name);
    device->SetUniqueName(name);

    // Parse the XML snippet into a fresh, standalone property node
    SGPropertyNode_ptr node = FGTestApi::propsFromString(xmlSnippet);

    device->Configure(node);
    return SGSharedPtr<TestInputDevice>(device);
}

// ---------------------------------------------------------------------------
// Test fixture setUp / tearDown
// ---------------------------------------------------------------------------
void InputDeviceTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("FGInputDevice");

    auto* cmds = globals->get_commands();
    cmds->addCommand("test-input-cmd", &_simpleCmd, &TestCommandHandler::handle);
    cmds->addCommand("test-button-press-cmd", &_buttonPressCmd, &TestCommandHandler::handle);
    cmds->addCommand("test-button-release-cmd", &_buttonReleaseCmd, &TestCommandHandler::handle);
    cmds->addCommand("test-axis-cmd", &_axisCmd, &TestCommandHandler::handle);
    cmds->addCommand("test-rel-cmd", &_relCmd, &TestCommandHandler::handle);
}

void InputDeviceTests::tearDown()
{
    auto* cmds = globals->get_commands();
    cmds->removeCommand("test-input-cmd");
    cmds->removeCommand("test-button-press-cmd");
    cmds->removeCommand("test-button-release-cmd");
    cmds->removeCommand("test-axis-cmd");
    cmds->removeCommand("test-rel-cmd");

    FGTestApi::tearDown::shutdownTestGlobals();
}

// ---------------------------------------------------------------------------
// testConfigureDevice
//
// Verify that Configure() parses the event list from a property node and that
// a matching HandleEvent call fires the correct binding. Also confirm that an
// unregistered event name is silently ignored (no crash).
// ---------------------------------------------------------------------------
void InputDeviceTests::testConfigureDevice()
{
    auto device = makeDevice("test-device", R"(
        <PropertyList>
          <event>
            <name>some-event</name>
            <binding>
              <command>test-input-cmd</command>
            </binding>
          </event>
        </PropertyList>
    )");

    // Confirm last-event nodes exist after Configure
    auto* dn = device->getDeviceNode();
    CPPUNIT_ASSERT(dn != nullptr);
    CPPUNIT_ASSERT(dn->getNode("last-event/name") != nullptr);
    CPPUNIT_ASSERT(dn->getNode("last-event/value") != nullptr);

    // Fire an event that has no matching handler — must not crash
    device->setEventName("no-such-event");
    FGEventData ignored{0.5, 0.016, KEYMOD_NONE};
    device->HandleEvent(ignored);
    CPPUNIT_ASSERT_EQUAL(0, _simpleCmd.callCount);

    // Fire the registered event
    device->setEventName("some-event");
    FGEventData ed{1.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(ed);
    CPPUNIT_ASSERT_EQUAL(1, _simpleCmd.callCount);
}

// ---------------------------------------------------------------------------
// testSimpleEventBinding
//
// A plain (non-axis, non-button) event carries its value as the "setting"
// argument to the bound command.
// ---------------------------------------------------------------------------
void InputDeviceTests::testSimpleEventBinding()
{
    auto device = makeDevice("test-device", R"(
        <PropertyList>
          <event>
            <name>some-event</name>
            <binding>
              <command>test-input-cmd</command>
            </binding>
          </event>
        </PropertyList>
    )");

    device->setEventName("some-event");

    FGEventData ed{0.75, 0.016, KEYMOD_NONE};
    device->HandleEvent(ed);

    CPPUNIT_ASSERT_EQUAL(1, _simpleCmd.callCount);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.75, _simpleCmd.lastSetting, 1e-9);

    // Last-event property nodes should reflect what was fired
    CPPUNIT_ASSERT_EQUAL("some-event"s,
                         std::string(device->getDeviceNode()->getStringValue("last-event/name")));
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.75,
                                 device->getDeviceNode()->getDoubleValue("last-event/value"), 1e-9);

    // Fire again with a different value
    FGEventData ed2{-0.5, 0.016, KEYMOD_NONE};
    device->HandleEvent(ed2);
    CPPUNIT_ASSERT_EQUAL(2, _simpleCmd.callCount);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(-0.5, _simpleCmd.lastSetting, 1e-9);
}

// ---------------------------------------------------------------------------
// testButtonPressRelease
//
// Button events (name starts with "button-"):
//   - Press  (value > 0)  fires the default binding with value=true.
//   - Release (value == 0) fires the mod-up binding with value=false.
// ---------------------------------------------------------------------------
void InputDeviceTests::testButtonPressRelease()
{
    auto device = makeDevice("test-device", R"(
        <PropertyList>
          <event>
            <name>button-fire</name>
            <binding>
              <command>test-button-press-cmd</command>
            </binding>
            <mod-up>
              <binding>
                <command>test-button-release-cmd</command>
              </binding>
            </mod-up>
          </event>
        </PropertyList>
    )");

    device->setEventName("button-fire");

    // Press
    FGEventData press{1.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(press);
    CPPUNIT_ASSERT_EQUAL(1, _buttonPressCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(true, _buttonPressCmd.lastValue);
    CPPUNIT_ASSERT_EQUAL(0, _buttonReleaseCmd.callCount);

    // Release
    FGEventData release{0.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(release);
    CPPUNIT_ASSERT_EQUAL(1, _buttonReleaseCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(false, _buttonReleaseCmd.lastValue);

    // A second press should fire again
    device->HandleEvent(press);
    CPPUNIT_ASSERT_EQUAL(2, _buttonPressCmd.callCount);
}

// ---------------------------------------------------------------------------
// testAbsAxisEvent
//
// An abs- axis event normalises its raw value to the configured range and
// passes it as the "setting" argument to the bound command.
//
// With min-range=-1000, max-range=1000 and SignedNormalized output:
//   raw value 500  → normalised ≈ 0.5
//   raw value -1000 → normalised = -1.0
// ---------------------------------------------------------------------------
void InputDeviceTests::testAbsAxisEvent()
{
    auto device = makeDevice("test-device", R"(
        <PropertyList>
          <event>
            <name>abs-x-axis</name>
            <min-range type="double">-1000.0</min-range>
            <max-range type="double">1000.0</max-range>
            <tolerance type="double">0.0</tolerance>
            <binding>
              <command>test-axis-cmd</command>
            </binding>
          </event>
        </PropertyList>
    )");

    device->setEventName("abs-x-axis");

    // Raw 500 → (2*(500 - -1000) / 2000) - 1 = (2*1500/2000) - 1 = 1.5 - 1 = 0.5
    FGEventData ed{500.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(ed);
    CPPUNIT_ASSERT_EQUAL(1, _axisCmd.callCount);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.5, _axisCmd.lastSetting, 1e-9);

    // Raw -1000 → -1.0
    FGEventData ed2{-1000.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(ed2);
    CPPUNIT_ASSERT_EQUAL(2, _axisCmd.callCount);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(-1.0, _axisCmd.lastSetting, 1e-9);
}

// ---------------------------------------------------------------------------
// testRelAxisEvent
//
// A rel- axis event (relative motion) passes its computed value as the
// "offset" argument (scaled by 1/max = 1/1.0) to the bound command.
//
// With direct output-mode and range [-100, 100], raw value 5 stays 5;
// the binding receives offset = 5 / 1.0 = 5.
// ---------------------------------------------------------------------------
void InputDeviceTests::testRelAxisEvent()
{
    auto device = makeDevice("test-device", R"(
        <PropertyList>
          <event>
            <name>rel-x-rotate</name>
            <min-range type="double">-100.0</min-range>
            <max-range type="double">100.0</max-range>
            <output-mode>direct</output-mode>
            <binding>
              <command>test-rel-cmd</command>
            </binding>
          </event>
        </PropertyList>
    )");

    device->setEventName("rel-x-rotate");

    // Rel-axis tolerance is forced to 0, so even tiny values fire
    FGEventData ed{5.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(ed);
    CPPUNIT_ASSERT_EQUAL(1, _relCmd.callCount);
    // FGRelAxisEvent::fire(binding, ed) calls binding->fire(ed.value, 1.0)
    // → _arg->offset = ed.value / 1.0 = 5.0
    CPPUNIT_ASSERT_DOUBLES_EQUAL(5.0, _relCmd.lastOffset, 1e-9);

    // Negative delta
    FGEventData ed2{-3.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(ed2);
    CPPUNIT_ASSERT_EQUAL(2, _relCmd.callCount);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(-3.0, _relCmd.lastOffset, 1e-9);
}
