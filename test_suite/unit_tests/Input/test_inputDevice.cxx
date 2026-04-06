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
    cmds->addCommand("test-low-btn-cmd", &_lowBtnCmd, &TestCommandHandler::handle);
    cmds->addCommand("test-low-btn-release-cmd", &_lowBtnReleaseCmd, &TestCommandHandler::handle);
    cmds->addCommand("test-high-btn-cmd", &_highBtnCmd, &TestCommandHandler::handle);
    cmds->addCommand("test-high-btn-release-cmd", &_highBtnReleaseCmd, &TestCommandHandler::handle);
    cmds->addCommand("test-switch-cmd", &_switchCmd, &TestCommandHandler::handle);
    cmds->addCommand("test-double-press-cmd", &_doublePressCmd, &TestCommandHandler::handle);
    cmds->addCommand("test-long-press-cmd", &_longPressCmd, &TestCommandHandler::handle);
}

void InputDeviceTests::tearDown()
{
    auto* cmds = globals->get_commands();
    cmds->removeCommand("test-input-cmd");
    cmds->removeCommand("test-button-press-cmd");
    cmds->removeCommand("test-button-release-cmd");
    cmds->removeCommand("test-axis-cmd");
    cmds->removeCommand("test-rel-cmd");
    cmds->removeCommand("test-low-btn-cmd");
    cmds->removeCommand("test-low-btn-release-cmd");
    cmds->removeCommand("test-high-btn-cmd");
    cmds->removeCommand("test-high-btn-release-cmd");
    cmds->removeCommand("test-switch-cmd");
    cmds->removeCommand("test-double-press-cmd");
    cmds->removeCommand("test-long-press-cmd");

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

// ---------------------------------------------------------------------------
// testHighLowThreshold
//
// An abs-axis event with <low> and <high> child nodes creates virtual button
// events that fire when the normalised axis value crosses the threshold.
//
// Config: range [-1000, 1000] (signed-normalized), thresholds at -0.8 / 0.8.
// The low button fires (press) when normalised value < -0.8, and releases
// when it returns above -0.8.  The high button fires when > 0.8 and releases
// when it drops back below 0.8.
// ---------------------------------------------------------------------------
void InputDeviceTests::testHighLowThreshold()
{
    auto device = makeDevice("test-device", R"(
        <PropertyList>
          <event>
            <name>abs-y-axis</name>
            <min-range type="double">-1000.0</min-range>
            <max-range type="double">1000.0</max-range>
            <tolerance type="double">0.0</tolerance>
            <low-threshold type="double">-0.8</low-threshold>
            <high-threshold type="double">0.8</high-threshold>
            <low>
              <name>button-low</name>
              <binding>
                <command>test-low-btn-cmd</command>
              </binding>
              <mod-up>
                <binding>
                  <command>test-low-btn-release-cmd</command>
                </binding>
              </mod-up>
            </low>
            <high>
              <name>button-high</name>
              <binding>
                <command>test-high-btn-cmd</command>
              </binding>
              <mod-up>
                <binding>
                  <command>test-high-btn-release-cmd</command>
                </binding>
              </mod-up>
            </high>
            <binding>
              <command>test-axis-cmd</command>
            </binding>
          </event>
        </PropertyList>
    )");

    device->setEventName("abs-y-axis");

    // --- 1. Centered value: neither threshold crossed ---
    FGEventData center{0.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(center);
    CPPUNIT_ASSERT_EQUAL(0, _lowBtnCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(0, _highBtnCmd.callCount);

    // --- 2. Move above high threshold (raw 900 → normalised 0.9 > 0.8) ---
    FGEventData high{900.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(high);
    CPPUNIT_ASSERT_EQUAL(1, _highBtnCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(true, _highBtnCmd.lastValue);
    CPPUNIT_ASSERT_EQUAL(0, _highBtnReleaseCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(0, _lowBtnCmd.callCount);

    // --- 3. Return to center: high button should release ---
    FGEventData backCenter{0.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(backCenter);
    CPPUNIT_ASSERT_EQUAL(1, _highBtnReleaseCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(false, _highBtnReleaseCmd.lastValue);
    // High press count unchanged
    CPPUNIT_ASSERT_EQUAL(1, _highBtnCmd.callCount);

    // --- 4. Move below low threshold (raw -900 → normalised -0.9 < -0.8) ---
    FGEventData low{-900.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(low);
    CPPUNIT_ASSERT_EQUAL(1, _lowBtnCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(true, _lowBtnCmd.lastValue);
    CPPUNIT_ASSERT_EQUAL(0, _lowBtnReleaseCmd.callCount);

    // --- 5. Return to center: low button should release ---
    FGEventData backCenter2{0.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(backCenter2);
    CPPUNIT_ASSERT_EQUAL(1, _lowBtnReleaseCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(false, _lowBtnReleaseCmd.lastValue);

    // --- 6. Cross high threshold again: should fire a second press ---
    FGEventData high2{950.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(high2);
    CPPUNIT_ASSERT_EQUAL(2, _highBtnCmd.callCount);
}

// ---------------------------------------------------------------------------
// testAxesOutputMode
//
// Verify the signed-normalized and direct output modes produce values in the
// expected ranges, and that the invert option reverses the output correctly.
//
// Range for all configs: [-1000, 1000].
//   signed-normalized: output = (2*(raw - min) / range) - 1  →  [-1, 1]
//     invert negates the result.
//   direct: output = raw (clamped to range)
//     invert mirrors: maxRange - (raw - minRange)
// ---------------------------------------------------------------------------
void InputDeviceTests::testAxesOutputMode()
{
    // ---- signed-normalized (default mode) ----
    {
        auto device = makeDevice("test-sn", R"(
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

        // Center (raw 0) → 0.0
        _axisCmd.reset();
        FGEventData ed0{0.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(ed0);
        CPPUNIT_ASSERT_EQUAL(1, _axisCmd.callCount);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, _axisCmd.lastSetting, 1e-9);

        // Full positive (raw 1000) → 1.0
        FGEventData ed1{1000.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(ed1);
        CPPUNIT_ASSERT_EQUAL(2, _axisCmd.callCount);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, _axisCmd.lastSetting, 1e-9);

        // Full negative (raw -1000) → -1.0
        FGEventData ed2{-1000.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(ed2);
        CPPUNIT_ASSERT_EQUAL(3, _axisCmd.callCount);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(-1.0, _axisCmd.lastSetting, 1e-9);

        // Quarter (raw 500) → 0.5
        FGEventData ed3{500.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(ed3);
        CPPUNIT_ASSERT_EQUAL(4, _axisCmd.callCount);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.5, _axisCmd.lastSetting, 1e-9);
    }

    // ---- signed-normalized + invert ----
    {
        auto device = makeDevice("test-sn-inv", R"(
            <PropertyList>
              <event>
                <name>abs-x-axis</name>
                <min-range type="double">-1000.0</min-range>
                <max-range type="double">1000.0</max-range>
                <tolerance type="double">0.0</tolerance>
                <invert type="bool">true</invert>
                <binding>
                  <command>test-axis-cmd</command>
                </binding>
              </event>
            </PropertyList>
        )");
        device->setEventName("abs-x-axis");

        // Center (raw 0) → -0.0 (negated 0.0)
        _axisCmd.reset();
        FGEventData ed0{0.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(ed0);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, _axisCmd.lastSetting, 1e-9);

        // Full positive (raw 1000) → inverted = -1.0
        FGEventData ed1{1000.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(ed1);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(-1.0, _axisCmd.lastSetting, 1e-9);

        // Full negative (raw -1000) → inverted = 1.0
        FGEventData ed2{-1000.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(ed2);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, _axisCmd.lastSetting, 1e-9);

        // Quarter (raw 500) → inverted = -0.5
        FGEventData ed3{500.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(ed3);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(-0.5, _axisCmd.lastSetting, 1e-9);
    }

    // ---- direct mode ----
    {
        auto device = makeDevice("test-direct", R"(
            <PropertyList>
              <event>
                <name>abs-x-axis</name>
                <min-range type="double">-1000.0</min-range>
                <max-range type="double">1000.0</max-range>
                <tolerance type="double">0.0</tolerance>
                <output-mode>direct</output-mode>
                <binding>
                  <command>test-axis-cmd</command>
                </binding>
              </event>
            </PropertyList>
        )");
        device->setEventName("abs-x-axis");

        // Raw value passes through unchanged
        _axisCmd.reset();
        FGEventData ed0{0.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(ed0);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, _axisCmd.lastSetting, 1e-9);

        FGEventData ed1{750.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(ed1);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(750.0, _axisCmd.lastSetting, 1e-9);

        FGEventData ed2{-500.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(ed2);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(-500.0, _axisCmd.lastSetting, 1e-9);

        // Values are clamped to range
        FGEventData ed3{2000.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(ed3);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(1000.0, _axisCmd.lastSetting, 1e-9);
    }

    // ---- direct mode + invert ----
    // invert formula: maxRange - (raw - minRange)
    //   raw 0    → 1000 - (0 - (-1000))    = 1000 - 1000 = 0
    //   raw 750  → 1000 - (750 - (-1000))   = 1000 - 1750 = -750
    //   raw -500 → 1000 - (-500 - (-1000))  = 1000 - 500  = 500
    {
        auto device = makeDevice("test-direct-inv", R"(
            <PropertyList>
              <event>
                <name>abs-x-axis</name>
                <min-range type="double">-1000.0</min-range>
                <max-range type="double">1000.0</max-range>
                <tolerance type="double">0.0</tolerance>
                <output-mode>direct</output-mode>
                <invert type="bool">true</invert>
                <binding>
                  <command>test-axis-cmd</command>
                </binding>
              </event>
            </PropertyList>
        )");
        device->setEventName("abs-x-axis");

        _axisCmd.reset();
        FGEventData ed0{0.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(ed0);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, _axisCmd.lastSetting, 1e-9);

        FGEventData ed1{750.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(ed1);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(-750.0, _axisCmd.lastSetting, 1e-9);

        FGEventData ed2{-500.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(ed2);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(500.0, _axisCmd.lastSetting, 1e-9);
    }

    // ---- unsigned-normalized mode ----
    // formula: (raw - min) / range  →  output in [0.0, 1.0]
    //   raw -1000 → (−1000 − (−1000)) / 2000 = 0.0
    //   raw     0 → (0     − (−1000)) / 2000 = 0.5
    //   raw  1000 → (1000  − (−1000)) / 2000 = 1.0
    {
        auto device = makeDevice("test-unsigned", R"(
            <PropertyList>
              <event>
                <name>abs-x-axis</name>
                <min-range type="double">-1000.0</min-range>
                <max-range type="double">1000.0</max-range>
                <tolerance type="double">0.0</tolerance>
                <output-mode>unsigned-normalized</output-mode>
                <binding>
                  <command>test-axis-cmd</command>
                </binding>
              </event>
            </PropertyList>
        )");
        device->setEventName("abs-x-axis");

        _axisCmd.reset();
        FGEventData edMin{-1000.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(edMin);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, _axisCmd.lastSetting, 1e-9);

        FGEventData edCenter{0.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(edCenter);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.5, _axisCmd.lastSetting, 1e-9);

        FGEventData edMax{1000.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(edMax);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, _axisCmd.lastSetting, 1e-9);

        FGEventData edQuarter{-500.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(edQuarter);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.25, _axisCmd.lastSetting, 1e-9);
    }

    // ---- unsigned-normalized + invert ----
    // invert formula: 1.0 - value
    //   raw -1000 → 1.0 - 0.0 = 1.0
    //   raw     0 → 1.0 - 0.5 = 0.5
    //   raw  1000 → 1.0 - 1.0 = 0.0
    //   raw  -500 → 1.0 - 0.25 = 0.75
    {
        auto device = makeDevice("test-unsigned-inv", R"(
            <PropertyList>
              <event>
                <name>abs-x-axis</name>
                <min-range type="double">-1000.0</min-range>
                <max-range type="double">1000.0</max-range>
                <tolerance type="double">0.0</tolerance>
                <output-mode>unsigned-normalized</output-mode>
                <invert type="bool">true</invert>
                <binding>
                  <command>test-axis-cmd</command>
                </binding>
              </event>
            </PropertyList>
        )");
        device->setEventName("abs-x-axis");

        _axisCmd.reset();
        FGEventData edMin{-1000.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(edMin);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, _axisCmd.lastSetting, 1e-9);

        FGEventData edCenter{0.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(edCenter);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.5, _axisCmd.lastSetting, 1e-9);

        FGEventData edMax{1000.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(edMax);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, _axisCmd.lastSetting, 1e-9);

        FGEventData edQuarter{-500.0, 0.016, KEYMOD_NONE};
        device->HandleEvent(edQuarter);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.75, _axisCmd.lastSetting, 1e-9);
    }
}

// ---------------------------------------------------------------------------
// testButtonSwitchMode
//
// In switch output-mode the same (non-mod-up) binding fires for every press
// AND every release:
//   - press  (value > 0) → binding called with value=true  (setting 1.0)
//   - release (value == 0) → binding called with value=false (setting 0.0)
//
// Unlike the default button mode, mod-up is never consulted, and there is no
// "already pressed" guard — each transition fires independently.
// ---------------------------------------------------------------------------
void InputDeviceTests::testButtonSwitchMode()
{
    auto device = makeDevice("test-device", R"(
        <PropertyList>
          <event>
            <name>button-toggle</name>
            <output-mode>switch</output-mode>
            <binding>
              <command>test-switch-cmd</command>
            </binding>
          </event>
        </PropertyList>
    )");

    device->setEventName("button-toggle");

    // --- 1. Press: binding should fire with value=true ---
    FGEventData press{1.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(press);
    CPPUNIT_ASSERT_EQUAL(1, _switchCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(true, _switchCmd.lastValue);

    // --- 2. Release: same binding fires with value=false (not a mod-up binding) ---
    FGEventData release{0.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(release);
    CPPUNIT_ASSERT_EQUAL(2, _switchCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(false, _switchCmd.lastValue);

    // --- 3. Second press: fires again with value=true ---
    device->HandleEvent(press);
    CPPUNIT_ASSERT_EQUAL(3, _switchCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(true, _switchCmd.lastValue);

    // --- 4. Second release: fires again with value=false ---
    device->HandleEvent(release);
    CPPUNIT_ASSERT_EQUAL(4, _switchCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(false, _switchCmd.lastValue);

    // --- 5. Non-zero values > 1 are still treated as press ---
    FGEventData pressHigh{255.0, 0.016, KEYMOD_NONE};
    device->HandleEvent(pressHigh);
    CPPUNIT_ASSERT_EQUAL(5, _switchCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(true, _switchCmd.lastValue);
}

// ---------------------------------------------------------------------------
// testDoublePress
//
// FGExtendedButtonEvent with a <mod-double-press> binding:
//   - The first press fires the regular press binding and opens a time window.
//   - A second press within that window fires the double-press binding and
//     suppresses the regular press binding.
//   - Every release fires the regular mod-up binding, regardless of whether
//     the press was single or double.
//   - Once the window expires (via device update), the next press is treated
//     as a fresh single press.
// ---------------------------------------------------------------------------
void InputDeviceTests::testDoublePress()
{
    auto device = makeDevice("test-device", R"(
        <PropertyList>
          <event>
            <name>button-action</name>
            <binding>
              <command>test-button-press-cmd</command>
            </binding>
            <mod-up>
              <binding>
                <command>test-button-release-cmd</command>
              </binding>
            </mod-up>
            <mod-double-press>
              <interval-sec type="double">0.5</interval-sec>
              <binding>
                <command>test-double-press-cmd</command>
              </binding>
            </mod-double-press>
          </event>
        </PropertyList>
    )");

    device->setEventName("button-action");

    FGEventData press{1.0, 0.016, KEYMOD_NONE};
    FGEventData release{0.0, 0.016, KEYMOD_NONE};

    // --- 1. First press fires the regular press binding ---
    device->HandleEvent(press);
    CPPUNIT_ASSERT_EQUAL(1, _buttonPressCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(0, _doublePressCmd.callCount);

    // --- 2. Release fires the regular mod-up binding ---
    device->HandleEvent(release);
    CPPUNIT_ASSERT_EQUAL(1, _buttonReleaseCmd.callCount);

    // --- 3. Second press within the double-press window fires the double-press
    //        binding and suppresses the regular press binding ---
    device->HandleEvent(press);
    CPPUNIT_ASSERT_EQUAL(1, _buttonPressCmd.callCount); // unchanged: suppressed
    CPPUNIT_ASSERT_EQUAL(1, _doublePressCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(true, _doublePressCmd.lastValue);

    // --- 4. Release after the double-press fires the regular mod-up binding ---
    device->HandleEvent(release);
    CPPUNIT_ASSERT_EQUAL(2, _buttonReleaseCmd.callCount);

    // --- 5. Once the double-press window closes, the next press is a normal
    //        single press and does not trigger another double-press ---
    device->update(0.6); // advance past the 0.5 s interval
    device->HandleEvent(press);
    CPPUNIT_ASSERT_EQUAL(2, _buttonPressCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(1, _doublePressCmd.callCount); // no new double-press
    device->HandleEvent(release);
    CPPUNIT_ASSERT_EQUAL(3, _buttonReleaseCmd.callCount);
}

// ---------------------------------------------------------------------------
// testLongPress
//
// FGExtendedButtonEvent with a <mod-long-press> binding:
//   - Pressing the button immediately fires the regular press binding.
//   - Once the button has been held past the configured threshold (advanced
//     via device update), the long-press binding fires.
//   - A release following a long-press does NOT fire the regular mod-up binding.
//   - A release before the threshold fires the regular mod-up binding normally.
// ---------------------------------------------------------------------------
void InputDeviceTests::testLongPress()
{
    auto device = makeDevice("test-device", R"(
        <PropertyList>
          <event>
            <name>button-action</name>
            <binding>
              <command>test-button-press-cmd</command>
            </binding>
            <mod-up>
              <binding>
                <command>test-button-release-cmd</command>
              </binding>
            </mod-up>
            <mod-long-press>
              <interval-sec type="double">1.0</interval-sec>
              <binding>
                <command>test-long-press-cmd</command>
              </binding>
            </mod-long-press>
          </event>
        </PropertyList>
    )");

    device->setEventName("button-action");

    FGEventData press{1.0, 0.016, KEYMOD_NONE};
    FGEventData release{0.0, 0.016, KEYMOD_NONE};

    // --- Scenario 1: long hold fires the long-press binding and suppresses mod-up ---

    // Press fires the regular binding immediately
    device->HandleEvent(press);
    CPPUNIT_ASSERT_EQUAL(1, _buttonPressCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(0, _longPressCmd.callCount);

    // Advance time below the 1.0 s threshold: long-press must not fire yet
    device->update(0.5);
    CPPUNIT_ASSERT_EQUAL(0, _longPressCmd.callCount);

    // Advance past the threshold (cumulative 1.1 s): long-press fires exactly once
    device->update(0.6);
    CPPUNIT_ASSERT_EQUAL(1, _longPressCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(true, _longPressCmd.lastValue);

    // Further updates while still pressed must not fire it a second time
    device->update(0.5);
    CPPUNIT_ASSERT_EQUAL(1, _longPressCmd.callCount);

    // Release after a long-press: mod-up fires normally
    device->HandleEvent(release);
    CPPUNIT_ASSERT_EQUAL(1, _buttonReleaseCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(false, _buttonReleaseCmd.lastValue);

    // --- Scenario 2: short press (released before threshold) fires mod-up normally ---

    device->HandleEvent(press);
    CPPUNIT_ASSERT_EQUAL(2, _buttonPressCmd.callCount);

    device->update(0.3);                              // below 1.0 s threshold
    CPPUNIT_ASSERT_EQUAL(1, _longPressCmd.callCount); // not fired again

    // Release before threshold: mod-up fires
    device->HandleEvent(release);
    CPPUNIT_ASSERT_EQUAL(2, _buttonReleaseCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(false, _buttonReleaseCmd.lastValue);
    CPPUNIT_ASSERT_EQUAL(1, _longPressCmd.callCount); // still just the one from scenario 1
}

// ---------------------------------------------------------------------------
// testRepeatableWithLongPress
//
// When a button is both repeatable and has a <mod-long-press> binding:
//   - The regular press binding fires immediately on press.
//   - While the button is held, the repeatable mechanism fires the regular
//     binding once on each device update() call (interval-sec defaults to 0).
//   - The long-press binding fires exactly once after the hold threshold.
//   - After the long-press fires, repeatable continues to fire the regular
//     binding on each update — they operate independently.
//   - A release always fires the mod-up binding, whether or not a long-press
//     occurred during the hold.
// ---------------------------------------------------------------------------
void InputDeviceTests::testRepeatableWithLongPress()
{
    auto device = makeDevice("test-device", R"(
        <PropertyList>
          <event>
            <name>button-action</name>
            <repeatable type="bool">true</repeatable>
            <binding>
              <command>test-button-press-cmd</command>
            </binding>
            <mod-up>
              <binding>
                <command>test-button-release-cmd</command>
              </binding>
            </mod-up>
            <mod-long-press>
              <interval-sec type="double">1.0</interval-sec>
              <binding>
                <command>test-long-press-cmd</command>
              </binding>
            </mod-long-press>
          </event>
        </PropertyList>
    )");

    device->setEventName("button-action");

    FGEventData press{1.0, 0.016, KEYMOD_NONE};
    FGEventData release{0.0, 0.016, KEYMOD_NONE};

    // --- Scenario 1: hold past the long-press threshold ---

    // Press fires the regular binding once
    device->HandleEvent(press);
    CPPUNIT_ASSERT_EQUAL(1, _buttonPressCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(0, _longPressCmd.callCount);

    // Two update ticks while still below the 1.0 s threshold: repeatable fires
    // once per tick; long-press must not trigger yet
    device->update(0.016);
    CPPUNIT_ASSERT_EQUAL(2, _buttonPressCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(0, _longPressCmd.callCount);

    device->update(0.016);
    CPPUNIT_ASSERT_EQUAL(3, _buttonPressCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(0, _longPressCmd.callCount);

    // Advance past the long-press threshold (cumulative > 1.0 s): long-press
    // fires exactly once; the same update also triggers one more repeatable fire
    device->update(1.1);
    CPPUNIT_ASSERT_EQUAL(1, _longPressCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(true, _longPressCmd.lastValue);
    const int repeatCountAfterLongPress = _buttonPressCmd.callCount;
    CPPUNIT_ASSERT(repeatCountAfterLongPress > 3); // at least one more repeatable fire

    // Further updates: repeatable keeps firing; long-press does NOT re-fire
    device->update(0.016);
    CPPUNIT_ASSERT_EQUAL(1, _longPressCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(repeatCountAfterLongPress + 1, _buttonPressCmd.callCount);

    // Release after long-press: mod-up fires normally
    device->HandleEvent(release);
    CPPUNIT_ASSERT_EQUAL(1, _buttonReleaseCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(false, _buttonReleaseCmd.lastValue);

    // --- Scenario 2: short hold (released before threshold) ---
    _buttonPressCmd.reset();
    _buttonReleaseCmd.reset();
    _longPressCmd.reset();

    device->HandleEvent(press);

    // A couple of update ticks while held
    device->update(0.016);
    device->update(0.016);
    CPPUNIT_ASSERT_EQUAL(3, _buttonPressCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(0, _longPressCmd.callCount); // not re-fired

    // Release before threshold: mod-up fires normally
    device->HandleEvent(release);
    CPPUNIT_ASSERT_EQUAL(1, _buttonReleaseCmd.callCount);
    CPPUNIT_ASSERT_EQUAL(false, _buttonReleaseCmd.lastValue);
    CPPUNIT_ASSERT_EQUAL(0, _longPressCmd.callCount); // still only from scenario 1
}
