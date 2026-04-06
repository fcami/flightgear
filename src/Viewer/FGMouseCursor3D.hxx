// SPDX-FileCopyrightText: 2022 James Hogan <james@albanarts.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Defines 3D mouse cursor visible in the scene.
 */

#pragma once

#include <osg/MatrixTransform>

#include <simgear/math/SGVec2.hxx>
#include <simgear/math/SGVec3.hxx>
#include <simgear/props/propertyObject.hxx>

#include <GUI/MouseCursor.hxx>

#include "renderer.hxx"

#include <vector>

namespace flightgear {

class FGDirectionCue3D;

/**
 * Represents a 3D mouse cursor visible in the scene.
 * It will place itself in the scene graph when the target is positioned.
 */
class FGMouseCursor3D : public osg::MatrixTransform
{
public:
    FGMouseCursor3D();
    virtual ~FGMouseCursor3D() = default;

    /// Set the cursor type.
    void setCursor(FGMouseCursor::Cursor cursor)
    {
        _cursor = cursor;
        updateModel();
    }

    /**
     * Show the cursor and optionally recenter.
     * @param[in] recenter Whether to recenter if cursor previously invisible.
     */
    void showCursor(bool recenter = false)
    {
        if (_visible)
            return;

        if (recenter)
            centerCursor();
        _visible = true;
        updateModel();
    }

    /**
     * Show the cursor and recenter.
     * Convenience function for calling showCursor with recenter=true
     */
    void showCursorCentered()
    {
        showCursor(true);
    }

    /// Hide the cursor until showCursor() or relative mouse motion.
    void hideCursorUntilMotion()
    {
        _visible = false;
        updateModel();
    }

    /**
     * Add relative 2D cursor motion to be applied on update.
     * @param[in] motion 2D relative cursor motion to apply.
     */
    void add2dMotion(const SGVec2d& motion)
    {
        _motion2d += motion;
    }

    /**
     * Set the 3D global position of the mouse cursor target.
     * Additionally the target is updated if @p commit is true, which should be
     * used for intentional mouse motion. Unintentional view motion may occlude
     * the target, bringing the cursor closer without immediately altering the
     * target.
     * @param[in] target Target position in global space.
     * @param[in] commit Whether to also update the saved target position.
     */
    void setTargetGlobal(const SGVec3d& target, bool commit = true);

    /**
     * Reset the 3D cursor position to the center of the current view.
     * The cursor will be reset on the next update.
     */
    void centerCursor()
    {
        _recenterPending = true;
    }

    /**
     * Update the position of the cursor.
     * This handles 360 cursor motion based on the 2D mouse motion, and updates
     * the 3D cursor position by picking into the scene towards the target.
     * @param[in] forcePick Whether to force a repick even without 2D motion,
     *                      recenter, or cursor visibility.
     * @returns PickList of scenery picks.
     */
    FGRenderer::PickList update(bool forcePick = false);

    /**
     * Perform a forced pick to update the cursor position.
     * Convenience function for calling update with forcePick=true
     * @returns PickList of scenery picks.
     */
    FGRenderer::PickList pick()
    {
        return update(true);
    }

protected:
    /// Update the cursor model.
    void updateModel();

protected:
    /// Reach distance.
    SGPropObjDouble _propReachM;
    /// 360 motion pixel angle / sensitivity (degrees).
    SGPropObjDouble _propPxAngleDeg;

    /**
     * Map of cursor types to switch indices (model IDs).
     * -1 indicates that no model exists.
     */
    std::vector<int> _modelMapping;

    /// Current cursor type.
    FGMouseCursor::Cursor _cursor = FGMouseCursor::CURSOR_NONE;

    /// Cursor currently visible.
    bool _visible = false;

    /// Current model ID.
    int _curModelId = -1;

    /**
     * The 3D cursor target relative to aircraft.
     * The cursor rests on the first surface towards this target from the
     * current view.
     */
    SGVec3d _targetAircraft = SGVec3d(1.0, 0.0, 0.0);

    /**
     * 2D mouse motion that should be applied on next update.
     * On update, this vector is applied to the mouse ray relative to the
     * current view, then cleared.
     */
    SGVec2d _motion2d;

    /// Whether a recenter is pending.
    bool _recenterPending = true;

    /// Direction cue object.
    osg::ref_ptr<FGDirectionCue3D> _cue;
};

} // namespace flightgear
