// FGMouseInput.cxx -- handle user input from mouse devices
//
// Written by Torsten Dreyer, started August 2009
// Based on work from David Megginson, started May 2001.
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2001 David Megginson <david@megginson.com>
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#include "config.h"

#include <memory>

#include "FGMouseInput.hxx"

#include <osgGA/GUIEventAdapter>

#include <simgear/scene/model/SGPickAnimation.hxx>
#include <simgear/scene/util/SGPickCallback.hxx>
#include <simgear/timing/timestamp.hxx>

#include "FGButton.hxx"
#include <GUI/MouseCursor.hxx>
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Viewer/FGEventHandler.hxx>
#include <Viewer/FGMouseCursor3D.hxx>
#include <Viewer/renderer.hxx>
#include <Viewer/sview.hxx>

using std::ios_base;

const int MAX_MICE = 1;
const int MAX_MOUSE_BUTTONS = 8;

typedef std::vector<SGSceneryPick> SGSceneryPicks;
typedef SGSharedPtr<SGPickCallback> SGPickCallbackPtr;
typedef std::list<SGPickCallbackPtr> SGPickCallbackList;

////////////////////////////////////////////////////////////////////////

/**
 * List of currently pressed mouse button events
 */
class ActivePickCallbacks : public std::map<int, SGPickCallbackList>
{
public:
    void update(double dt, unsigned int keyModState);
    void init(int button, const osgGA::GUIEventAdapter* ea,
              flightgear::FGMouseCursor3D* cursor3D = nullptr);
};


void ActivePickCallbacks::init(int button, const osgGA::GUIEventAdapter* ea,
                               flightgear::FGMouseCursor3D* cursor3D)
{
    SGSceneryPicks pickList;
    if (cursor3D) {
        // 3D pass through
        cursor3D->showCursor();
        pickList = cursor3D->pick();
    } else {
        osg::Vec2d windowPos;
        flightgear::eventToWindowCoords(ea, windowPos.x(), windowPos.y());

        // Get the list of hit callbacks. Take the first callback that
        // accepts the mouse button press and ignore the rest of them
        // That is they get sorted by distance and by scenegraph depth.
        // The nearest one is the first one and the deepest
        // (the most specialized one in the scenegraph) is the first.
        pickList = globals->get_renderer()->pick(windowPos);
    }

    if (pickList.empty()) {
        return;
    }

    for (const SGSceneryPick& pick : pickList) {
        if (!pick.callback)
            continue;
        if (pick.callback->buttonPressed(button, *ea, pick.info)) {
            (*this)[button].push_back(pick.callback);
            return;
        }
    }
}

void ActivePickCallbacks::update(double dt, unsigned int keyModState)
{
    // handle repeatable mouse press events
    for (iterator mi = begin(); mi != end(); ++mi) {
        SGPickCallbackList::iterator li;
        for (li = mi->second.begin(); li != mi->second.end(); ++li) {
            (*li)->update(dt, keyModState);
        }
    }
}

////////////////////////////////////////////////////////////////////////


/**
 * Settings for a mouse mode.
 */
struct mouse_mode {
    mouse_mode();

    FGMouseCursor::Cursor cursor;
    bool constrained;
    bool pass_through;
    bool _passThrough3D = false;
    std::unique_ptr<FGButton[]> buttons;
    SGBindingList x_bindings[KEYMOD_MAX];
    SGBindingList y_bindings[KEYMOD_MAX];
    SGConditionRef cursor3DCondition;
};


/**
 * Settings for a mouse.
 */
struct mouse {
    mouse();

    int x, y;
    SGPropertyNode_ptr mode_node;
    SGPropertyNode_ptr mouse_button_nodes[MAX_MOUSE_BUTTONS];
    int nModes;
    int current_mode;

    SGTimeStamp timeSinceLastMove;
    std::unique_ptr<mouse_mode[]> modes;

    osg::ref_ptr<flightgear::FGMouseCursor3D> cursor3D;
};

static const SGSceneryPick*
getPick(const SGSceneryPicks& pick_list,
        const SGPickCallback* cb)
{
    for (size_t i = 0; i < pick_list.size(); ++i)
        if (pick_list[i].callback == cb)
            return &pick_list[i];

    return 0;
}

////////////////////////////////////////////////////////////////////////

class FGMouseInput::FGMouseInputPrivate : public SGPropertyChangeListener
{
public:
    FGMouseInputPrivate()
        : haveWarped(false),
          xSizeNode(fgGetNode("/sim/startup/xsize", false)),
          ySizeNode(fgGetNode("/sim/startup/ysize", false)),
          xAccelNode(fgGetNode("/devices/status/mice/mouse/accel-x", true)),
          yAccelNode(fgGetNode("/devices/status/mice/mouse/accel-y", true)),
          mouseXNode(fgGetNode("/devices/status/mice/mouse/x", true)),
          mouseYNode(fgGetNode("/devices/status/mice/mouse/y", true))
    {
        tooltipTimeoutDone = false;
        hoverPickScheduled = false;

        fgGetNode("/sim/mouse/hide-cursor", true)->addChangeListener(this, true);
        fgGetNode("/sim/mouse/cursor-timeout-sec", true)->addChangeListener(this, true);
        fgGetNode("/sim/mouse/right-button-mode-cycle-enabled", true)->addChangeListener(this, true);
        fgGetNode("/sim/mouse/tooltip-delay-msec", true)->addChangeListener(this, true);
        fgGetNode("/sim/mouse/click-shows-tooltip", true)->addChangeListener(this, true);
        fgGetNode("/sim/mouse/tooltips-enabled", true)->addChangeListener(this, true);
        fgGetNode("/sim/mouse/tooltip-commands-registered", true)->addChangeListener(this, true);
        fgGetNode("/sim/mouse/drag-sensitivity", true)->addChangeListener(this, true);
        fgGetNode("/sim/mouse/invert-mouse-wheel", true)->addChangeListener(this, true);
        fgGetNode("/sim/vr/cursors/mouse[0]/dx", true)->addChangeListener(this, true);
        fgGetNode("/sim/vr/cursors/mouse[0]/dy", true)->addChangeListener(this, true);
    }

    bool areTooltipsEnabled() const
    {
        return _tooltipsEnabled && _tooltipsCommandsRegistered;
    }

    void centerMouseCursor(mouse& m)
    {
        // center the cursor
        m.x = (xSizeNode ? xSizeNode->getIntValue() : 800) / 2;
        m.y = (ySizeNode ? ySizeNode->getIntValue() : 600) / 2;
        fgWarpMouse(m.x, m.y);
        haveWarped = true;
    }

    void constrainMouse(int x, int y)
    {
        int new_x = x, new_y = y;
        int xsize = xSizeNode ? xSizeNode->getIntValue() : 800;
        int ysize = ySizeNode ? ySizeNode->getIntValue() : 600;

        bool need_warp = false;
        if (x <= (xsize * .25) || x >= (xsize * .75)) {
            new_x = int(xsize * .5);
            need_warp = true;
        }

        if (y <= (ysize * .25) || y >= (ysize * .75)) {
            new_y = int(ysize * .5);
            need_warp = true;
        }

        if (need_warp) {
            fgWarpMouse(new_x, new_y);
            haveWarped = true;
        }
    }

    void scheduleHoverPick(const osg::Vec2d& windowPos)
    {
        hoverPickScheduled = true;
        hoverPos = windowPos;
    }

    void doHoverPick(const osg::Vec2d& windowPos, bool passThrough3D)
    {
        FGMouseCursor::Cursor cur = FGMouseCursor::CURSOR_ARROW;
        bool explicitCursor = false;
        bool didPick = false;

        const auto& m = mice[0];

        SGPickCallback::Priority priority = SGPickCallback::PriorityScenery;
        SGSceneryPicks pickList;
        if (passThrough3D) {
            m.cursor3D->showCursor();
            pickList = m.cursor3D->pick();
        } else {
            pickList = globals->get_renderer()->pick(windowPos);

            // Make the 3D cursor target the closest surface under the 2D mouse.
            if (!pickList.empty() &&
                m.modes[m.current_mode].cursor3DCondition &&
                m.modes[m.current_mode].cursor3DCondition->test()) {
                m.cursor3D->setTargetGlobal(pickList.front().info.wgs84);
                m.cursor3D->showCursor();
            }
        }

        for (const SGSceneryPick& pick : pickList) {
            if (!pick.callback)
                continue;

            bool done = pick.callback->hover(windowPos, pick.info);
            std::string curName(pick.callback->getCursor());
            if (!curName.empty()) {
                explicitCursor = true;
                cur = FGMouseCursor::cursorFromString(curName.c_str());
            }

            // if the callback is of higher priority (lower enum index),
            // record that.
            if (pick.callback->getPriority() < priority) {
                priority = pick.callback->getPriority();
            }

            if (done) {
                didPick = true;
                break;
            }
        } // of picks iteration

        // Check if any pick from the previous iteration has disappeared. If so
        // notify the callback that the mouse has left its element.
        for (const SGSceneryPick& pick : _previous_picks) {
            if (pick.callback && !getPick(pickList, pick.callback))
                pick.callback->mouseLeave(windowPos);
        }
        _previous_picks = pickList;

        if (!explicitCursor && (priority == SGPickCallback::PriorityPanel)) {
            cur = FGMouseCursor::CURSOR_HAND;
        }

        FGMouseCursor::instance()->setCursor(cur);
        if (!didPick && areTooltipsEnabled()) {
            SGPropertyNode_ptr args(new SGPropertyNode);
            globals->get_commands()->execute("update-hover", args, nullptr);
        }
    }

    void doMouseMoveWithCallbacks(const osgGA::GUIEventAdapter* ea)
    {
        FGMouseCursor::Cursor cur = FGMouseCursor::CURSOR_CLOSED_HAND;

        osg::Vec2d windowPos;
        const bool ok = flightgear::eventToWindowCoords(ea, windowPos.x(), windowPos.y());
        if (!ok) {
            SG_LOG(SG_GUI, SG_WARN, "doMouseMoveWithCallbacks: ignoring mouse move with missing context/traits");
            return;
        }

        // Do not compute scenery picks unless a callback requests it, as it is costly.
        SGSceneryPicks pickList;
        bool did_pick = false;

        for (ActivePickCallbacks::iterator mi = activePickCallbacks.begin();
             mi != activePickCallbacks.end();
             ++mi) {
            SGPickCallbackList::iterator li;
            for (li = mi->second.begin(); li != mi->second.end(); ++li) {
                if (!did_pick && (*li)->needsDragPosition()) {
                    pickList = globals->get_renderer()->pick(windowPos);
                    did_pick = true;
                }

                const SGSceneryPick* pick = getPick(pickList, *li);
                (*li)->mouseMoved(*ea, pick ? &pick->info : 0);

                std::string curName((*li)->getCursor());
                if (!curName.empty())
                    cur = FGMouseCursor::cursorFromString(curName.c_str());
            }
        }

        FGMouseCursor::instance()->setCursor(cur);
    }

    // implement the property-change-listener interfacee
    void valueChanged(SGPropertyNode* node) override
    {
        if (node->getNameString() == "drag-sensitivity") {
            SGKnobAnimation::setDragSensitivity(node->getDoubleValue());
        } else if (node->getNameString() == "invert-mouse-wheel") {
            SGKnobAnimation::setAlternateMouseWheelDirection(node->getBoolValue());
        } else if (node->getNameString() == "hide-cursor") {
            hideCursor = node->getBoolValue();
        } else if (node->getNameString() == "cursor-timeout-sec") {
            cursorTimeoutMsec = node->getDoubleValue() * 1000;
        } else if (node->getNameString() == "tooltip-delay-msec") {
            tooltipDelayMsec = node->getIntValue();
        } else if (node->getNameString() == "right-button-mode-cycle-enabled") {
            rightClickModeCycle = node->getBoolValue();
        } else if (node->getNameString() == "click-shows-tooltip") {
            clickTriggersTooltip = node->getBoolValue();
        } else if (node->getNameString() == "tooltips-enabled") {
            _tooltipsEnabled = node->getBoolValue();
        } else if (node->getNameString() == "tooltip-commands-registered") {
            _tooltipsCommandsRegistered = node->getBoolValue();
        } else if (node->getNameString() == "dx") {
            cursor3DMotion.x() += node->getDoubleValue();
        } else if (node->getNameString() == "dy") {
            cursor3DMotion.y() += node->getDoubleValue();
        }
    }

    ActivePickCallbacks activePickCallbacks;
    SGSceneryPicks _previous_picks;

    mouse mice[MAX_MICE];

    bool hideCursor, haveWarped;
    bool tooltipTimeoutDone;
    bool clickTriggersTooltip;
    int tooltipDelayMsec, cursorTimeoutMsec;
    bool rightClickModeCycle;

    bool _tooltipsEnabled = false;
    bool _tooltipsCommandsRegistered = false; ///< avoid errors if the mouse moves before Nasal init

    SGPropertyNode_ptr xSizeNode;
    SGPropertyNode_ptr ySizeNode;
    SGPropertyNode_ptr xAccelNode;
    SGPropertyNode_ptr yAccelNode;
    SGPropertyNode_ptr mouseXNode, mouseYNode;

    bool hoverPickScheduled;
    osg::Vec2d hoverPos;
    bool leaveScheduled = false;
    SGVec2d cursor3DMotion = SGVec2d(0.0, 0.0);
};


////////////////////////////////////////////////////////////////////////
// The Mouse Input Implementation
////////////////////////////////////////////////////////////////////////

static FGMouseInput* global_mouseInput = nullptr;

static void mouseClickHandler(int button, int updown, int x, int y, bool mainWindow, const osgGA::GUIEventAdapter* ea)
{
    if (global_mouseInput)
        global_mouseInput->doMouseClick(button, updown, x, y, mainWindow, ea);
}

static void mouseMotionHandler(int x, int y, const osgGA::GUIEventAdapter* ea)
{
    if (global_mouseInput != 0)
        global_mouseInput->doMouseMotion(x, y, ea);
}

static void mouseLeaveHandler(const osgGA::GUIEventAdapter* ea)
{
    if (global_mouseInput)
        global_mouseInput->doMouseLeave(ea);
}

FGMouseInput::FGMouseInput() = default;

void FGMouseInput::init()
{
    SG_LOG(SG_INPUT, SG_DEBUG, "Initializing mouse bindings");

    d.reset(new FGMouseInputPrivate());
    std::string module = "";

    SGPropertyNode* mouse_nodes = fgGetNode("/input/mice");
    if (mouse_nodes == 0) {
        SG_LOG(SG_INPUT, SG_WARN, "No mouse bindings (/input/mice)!!");
        mouse_nodes = fgGetNode("/input/mice", true);
    }

    int j;
    for (int i = 0; i < MAX_MICE; i++) {
        SGPropertyNode* mouse_node = mouse_nodes->getChild("mouse", i, true);
        mouse& m = d->mice[i];

        // Grab node pointers
        std::ostringstream buf;
        buf << "/devices/status/mice/mouse[" << i << "]/mode";
        m.mode_node = fgGetNode(buf.str().c_str());
        if (m.mode_node == NULL) {
            m.mode_node = fgGetNode(buf.str().c_str(), true);
            m.mode_node->setIntValue(0);
        }
        for (j = 0; j < MAX_MOUSE_BUTTONS; j++) {
            // According to <https://stackoverflow.com/a/12112642/4756009> and
            // other similar questions on stackoverflow.com, it seems safer not
            // to try to reuse the 'buf' variable we have above.
            std::ostringstream buf;
            buf << "/devices/status/mice/mouse[" << i << "]/button[" << j << "]";
            m.mouse_button_nodes[j] = fgGetNode(buf.str().c_str(), true);
            m.mouse_button_nodes[j]->setBoolValue(false);
        }

        // Read all the modes
        m.nModes = mouse_node->getIntValue("mode-count", 1);
        m.modes.reset(new mouse_mode[m.nModes]);

        for (int j = 0; j < m.nModes; j++) {
            int k;
            SGPropertyNode* mode_node = mouse_node->getChild("mode", j, true);

            // Read the mouse cursor for this mode
            m.modes[j].cursor = FGMouseCursor::cursorFromString(mode_node->getStringValue("cursor", "inherit").c_str());

            // Read other properties for this mode
            m.modes[j].constrained = mode_node->getBoolValue("constrained", false);
            m.modes[j].pass_through = mode_node->getBoolValue("pass-through", false);
            m.modes[j]._passThrough3D = mode_node->getBoolValue("vr-cursor/pass-through", false);

            // 3D pass-through needs similar code paths to 2D pass-through in
            // this class
            if (m.modes[j]._passThrough3D)
                m.modes[j].pass_through = true;

            // Read the 3D mouse cursor condition
            SGPropertyNode* cursor3DCondition = mode_node->getNode("vr-cursor/condition");
            if (cursor3DCondition)
                m.modes[j].cursor3DCondition = sgReadCondition(globals->get_props(),
                                                               cursor3DCondition);

            // Read the button bindings for this mode
            m.modes[j].buttons.reset(new FGButton[MAX_MOUSE_BUTTONS]);
            for (k = 0; k < MAX_MOUSE_BUTTONS; k++) {
                std::ostringstream buf;
                buf << "mouse button " << k;
                m.modes[j].buttons[k].init(mode_node->getChild("button", k), buf.str(), module);
            }

            // Read the axis bindings for this mode
            read_bindings(mode_node->getChild("x-axis", 0, true), m.modes[j].x_bindings, KEYMOD_NONE, module);
            read_bindings(mode_node->getChild("y-axis", 0, true), m.modes[j].y_bindings, KEYMOD_NONE, module);

            if (mode_node->hasChild("x-axis-ctrl")) {
                read_bindings(mode_node->getChild("x-axis-ctrl"), m.modes[j].x_bindings, KEYMOD_CTRL, module);
            }
            if (mode_node->hasChild("x-axis-shift")) {
                read_bindings(mode_node->getChild("x-axis-shift"), m.modes[j].x_bindings, KEYMOD_SHIFT, module);
            }
            if (mode_node->hasChild("x-axis-ctrl-shift")) {
                read_bindings(mode_node->getChild("x-axis-ctrl-shift"), m.modes[j].x_bindings, KEYMOD_CTRL | KEYMOD_SHIFT, module);
            }

            if (mode_node->hasChild("y-axis-ctrl")) {
                read_bindings(mode_node->getChild("y-axis-ctrl"), m.modes[j].y_bindings, KEYMOD_CTRL, module);
            }
            if (mode_node->hasChild("y-axis-shift")) {
                read_bindings(mode_node->getChild("y-axis-shift"), m.modes[j].y_bindings, KEYMOD_SHIFT, module);
            }
            if (mode_node->hasChild("y-axis-ctrl-shift")) {
                read_bindings(mode_node->getChild("y-axis-ctrl-shift"), m.modes[j].y_bindings, KEYMOD_CTRL | KEYMOD_SHIFT, module);
            }
        } // of modes iteration

        m.cursor3D = new flightgear::FGMouseCursor3D;
    }

    fgRegisterMouseClickHandler(mouseClickHandler);
    fgRegisterMouseMotionHandler(mouseMotionHandler);
    fgRegisterMouseLeaveHandler(mouseLeaveHandler);
    global_mouseInput = this;
}

void FGMouseInput::shutdown()
{
    SG_LOG(SG_INPUT, SG_DEBUG, "Shutting down mouse bindings");

    // This ensures that mouseClickHandler and mouseMotionHandler are no-ops.
    global_mouseInput = nullptr;
    // Reset the Pimpl
    d.reset();
}

void FGMouseInput::reinit()
{
    shutdown();
    init();
}

void FGMouseInput::update(double dt)
{
    if (!d) {
        SG_LOG(SG_INPUT, SG_WARN, "update of mouse before init");
    }

    mouse& m = d->mice[0];
    int mode = m.mode_node->getIntValue();
    const bool modeValid = (mode >= 0 && mode < m.nModes);
    if (mode != m.current_mode) {
        // current mode has changed
        m.current_mode = mode;
        m.timeSinceLastMove.stamp();

        if (modeValid) {
            FGMouseCursor::instance()->setCursor(m.modes[mode].cursor);
            d->centerMouseCursor(m);

            // Show or hide 3D cursor on mode change depending on condition
            if (m.modes[mode].cursor3DCondition &&
                m.modes[mode].cursor3DCondition->test()) {
                m.cursor3D->showCursor();
            } else {
                m.cursor3D->hideCursorUntilMotion();
                // discard any pending motion
                d->cursor3DMotion = SGVec2d(0.0, 0.0);
            }

            // Hide desktop mouse cursor while in 360 mouse mode
            FGMouseCursor::instance()->setCursorVisible(!m.modes[mode]._passThrough3D);
        } else {
            SG_LOG(SG_INPUT, SG_WARN, "Mouse mode " << mode << " out of range");
            FGMouseCursor::instance()->setCursor(FGMouseCursor::CURSOR_ARROW);
        }
    }

    if (modeValid && m.modes[mode].pass_through && d->hoverPickScheduled) {
        d->doHoverPick(d->hoverPos, m.modes[mode]._passThrough3D);
        d->hoverPickScheduled = false;
    }
    // Leave *after* hover pick, there's no point hiding the cursor only to make
    // it visible again.
    if (d->leaveScheduled) {
        // Hide 3D cursor if mouse leaves window
        m.cursor3D->hideCursorUntilMotion();
        d->leaveScheduled = false;
    }

    m.cursor3D->setCursor(FGMouseCursor::instance()->getCursor());
    // Hide 3D cursor if condition no longer passes
    if (modeValid && m.modes[mode].cursor3DCondition &&
        !m.modes[mode].cursor3DCondition->test()) {
        m.cursor3D->hideCursorUntilMotion();
    }

    // Handle 360 mouse motion
    if (d->cursor3DMotion != SGVec2d(0.0, 0.0)) {
        m.cursor3D->showCursor();
        m.cursor3D->add2dMotion(d->cursor3DMotion);
        d->cursor3DMotion = SGVec2d(0.0, 0.0);
    }
    m.cursor3D->update();

    if (!d->tooltipTimeoutDone &&
        d->areTooltipsEnabled() &&
        (m.timeSinceLastMove.elapsedMSec() > d->tooltipDelayMsec)) {
        d->tooltipTimeoutDone = true;
        SGPropertyNode_ptr arg(new SGPropertyNode);
        globals->get_commands()->execute("tooltip-timeout", arg, nullptr);
    }

    if (d->hideCursor) {
        if (m.timeSinceLastMove.elapsedMSec() > d->cursorTimeoutMsec) {
            FGMouseCursor::instance()->hideCursorUntilMouseMove();
            m.cursor3D->hideCursorUntilMotion();
            m.timeSinceLastMove.stamp();
        }
    }

    d->activePickCallbacks.update(dt, fgGetKeyModifiers());
}

mouse::mouse()
    : x(-1),
      y(-1),
      nModes(1),
      current_mode(0),
      modes()
{
}

mouse_mode::mouse_mode()
    : cursor(FGMouseCursor::CURSOR_ARROW),
      constrained(false),
      pass_through(false),
      buttons()
{
}

void FGMouseInput::doMouseClick(int b, int updown, int x, int y, bool mainWindow, const osgGA::GUIEventAdapter* ea)
{
    if (!d) {
        // can occur during reset
        return;
    }

    int modifiers = fgGetKeyModifiers();

    mouse& m = d->mice[0];
    mouse_mode& mode = m.modes[m.current_mode];
    // Let the property manager know.
    if (b >= 0 && b < MAX_MOUSE_BUTTONS)
        m.mouse_button_nodes[b]->setBoolValue(updown == MOUSE_BUTTON_DOWN);

    // in spring-loaded look mode, ignore right clicks entirely here
    if (d->rightClickModeCycle || (b != 2)) {
        // Pass on to PUI and the panel if requested, and return if one of them
        // consumes the event.

        osg::Vec2d windowPos;
        bool ok = flightgear::eventToWindowCoords(ea, windowPos.x(), windowPos.y());
        if (!ok) {
            SG_LOG(SG_GUI, SG_WARN, "Ignoring mouse click with null context/traits");
            return;
        }

        SGSceneryPicks pickList;

        if (isRightDragLookActive() && (updown == MOUSE_BUTTON_DOWN)) {
            // when spring-loaded mode is active, don't do scene selection for picks
            // https://sourceforge.net/p/flightgear/codetickets/2108/
        } else {
            pickList = globals->get_renderer()->pick(windowPos);
        }

        if (updown == MOUSE_BUTTON_UP) {
            // Execute the mouse up event in any case, may be we should
            // stop processing here?

            SGPickCallbackList& callbacks = d->activePickCallbacks[b];

            while (!callbacks.empty()) {
                SGPickCallbackPtr& cb = callbacks.front();
                const SGSceneryPick* pick = getPick(pickList, cb);
                cb->buttonReleased(ea->getModKeyMask(), *ea, pick ? &pick->info : nullptr);

                callbacks.pop_front();
            }

            if (ea->getHandled()) {
                // for https://sourceforge.net/p/flightgear/codetickets/2347/
                // we cleared the active picks, but don't do further processing
                return;
            }
        }

        if (mode.pass_through) {
            // compute a scenegraph intersection point corresponding to the mouse
            // click
            if (updown == MOUSE_BUTTON_DOWN) {
                d->activePickCallbacks.init(b, ea,
                                            mode._passThrough3D ? m.cursor3D : nullptr);

                if (d->clickTriggersTooltip && d->areTooltipsEnabled()) {
                    SGPropertyNode_ptr args(new SGPropertyNode);
                    args->setStringValue("reason", "click");
                    globals->get_commands()->execute("tooltip-timeout", args, nullptr);
                    d->tooltipTimeoutDone = true;
                }
            } else {
                // do a hover pick now, to fix up cursor
                d->doHoverPick(windowPos, mode._passThrough3D);
            } // mouse button was released
        } // of pass-through mode
    }

    if (b >= MAX_MOUSE_BUTTONS) {
        SG_LOG(SG_INPUT, SG_ALERT, "Mouse button " << b << " where only " << MAX_MOUSE_BUTTONS << " expected");
        return;
    }

    m.modes[m.current_mode].buttons[b].update(modifiers, 0 != updown, x, y);
}

void FGMouseInput::processMotion(int x, int y, const osgGA::GUIEventAdapter* ea)
{
    if (!d->activePickCallbacks[0].empty()) {
        d->doMouseMoveWithCallbacks(ea);
        return;
    }

    if (SviewMouseMotion(x, y, *ea)) {
        return;
    }

    mouse& m = d->mice[0];
    int modeIndex = m.current_mode;

    if (isRightDragLookActive()) {
        // right mouse is down, force look mode
        modeIndex = 3;
    }

    mouse_mode& mode = m.modes[modeIndex];

    if (mode.pass_through) {
        osg::Vec2d windowPos;
        flightgear::eventToWindowCoords(ea, windowPos.x(), windowPos.y());

        // omly do hover picks if no buttons are down
        if (ea->getButtonMask() == 0) {
            d->scheduleHoverPick(windowPos);
        }

        // mouse has moved, so we may need to issue tooltip-timeout command again
        d->tooltipTimeoutDone = false;
    }

    if (d->haveWarped) {
        // don't fire mouse-movement events at the first update after warping
        // the mouse, just remember the new mouse position
        d->haveWarped = false;
    } else {
        int modifiers = fgGetKeyModifiers();
        int xsize = d->xSizeNode ? d->xSizeNode->getIntValue() : 800;
        int ysize = d->ySizeNode ? d->ySizeNode->getIntValue() : 600;

        // OK, PUI didn't want the event,
        // so we can play with it.
        if (x != m.x) {
            int delta = x - m.x;
            d->xAccelNode->setIntValue(delta);
            for (unsigned int i = 0; i < mode.x_bindings[modifiers].size(); i++)
                mode.x_bindings[modifiers][i]->fire(double(delta), double(xsize));
        }
        if (y != m.y) {
            int delta = y - m.y;
            d->yAccelNode->setIntValue(-delta);
            for (unsigned int i = 0; i < mode.y_bindings[modifiers].size(); i++)
                mode.y_bindings[modifiers][i]->fire(double(delta), double(ysize));
        }
    }

    // Constrain the mouse if requested
    if (mode.constrained) {
        d->constrainMouse(x, y);
    }
}

void FGMouseInput::doMouseMotion(int x, int y, const osgGA::GUIEventAdapter* ea)
{
    if (!d) {
        // can occur during reset
        return;
    }

    mouse& m = d->mice[0];

    if (m.current_mode < 0 || m.current_mode >= m.nModes) {
        m.x = x;
        m.y = y;
        return;
    }

    m.timeSinceLastMove.stamp();
    FGMouseCursor::instance()->mouseMoved();

    // TODO Get rid of this as soon as soon as cursor hide timeout works globally

    if (!ea->getHandled()) {
        processMotion(x, y, ea);
    } else {
        // Hide 3D cursor if mouse over GUI
        m.cursor3D->hideCursorUntilMotion();
    }

    m.x = x;
    m.y = y;
    d->mouseXNode->setIntValue(x);
    d->mouseYNode->setIntValue(y);
}

void FGMouseInput::doMouseLeave(const osgGA::GUIEventAdapter* ea)
{
    if (!d) {
        return;
    }

    d->leaveScheduled = true;
}

bool FGMouseInput::isRightDragToLookEnabled() const
{
    if (!d) {
        return false;
    }

    return (d->rightClickModeCycle == false);
}

bool FGMouseInput::isRightDragLookActive() const
{
    if (!d) {
        return false;
    }

    const auto& m = d->mice[0];
    if (!d->rightClickModeCycle && m.nModes > 3) {
        return m.mouse_button_nodes[2]->getBoolValue();
    }

    return false;
}


// Register the subsystem.
SGSubsystemMgr::Registrant<FGMouseInput> registrantFGMouseInput;
