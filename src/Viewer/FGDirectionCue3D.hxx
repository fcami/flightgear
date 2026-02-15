// SPDX-FileCopyrightText: 2025 James Hogan <james@albanarts.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Defines 3D direction cue visible in the scene.
 */

#pragma once

#include <osg/MatrixTransform>

#include <simgear/math/SGVec2.hxx>
#include <simgear/math/SGVec3.hxx>

#include "renderer.hxx"

#include <vector>

namespace flightgear {

/**
 * Represents a direction cue visible in the scene.
 * It will place itself in the scene graph when the target is positioned.
 */
class FGDirectionCue3D : public osg::MatrixTransform
{
public:
    FGDirectionCue3D(osg::Node* target);
    virtual ~FGDirectionCue3D() = default;

    /**
     * Show the cue.
     */
    void showCue()
    {
        _visible = true;
        updateModel();
    }

    /// Hide the cue.
    void hideCue()
    {
        _visible = false;
        updateModel();
    }

    /// Update the position of the direction cue.
    void update();

protected:
    /// Update the cue model.
    void updateModel();

protected:
    /// Cue currently visible.
    bool _visible = false;

    /// Whether the target can currently be pointed at.
    bool _targetValid = true;

    /// Target object the cue should point at.
    osg::observer_ptr<osg::Node> _target;
};

} // namespace flightgear
