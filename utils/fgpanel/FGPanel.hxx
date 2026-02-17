// SPDX-FileComment: generic support classes for a 2D panel.
// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifndef __cplusplus
# error This library requires C++
#endif


#include <simgear/props/propsfwd.hxx>
#include <simgear/structure/subsystem_mgr.hxx>

#include "FGCroppedTexture.hxx"
#include "FGPanelInstrument.hxx"


////////////////////////////////////////////////////////////////////////
// Top-level panel.
////////////////////////////////////////////////////////////////////////

/**
 * Instrument panel class.
 *
 * The panel is a container that has a background texture and holds
 * zero or more instruments.  The panel will order the instruments to
 * redraw themselves when necessary, and will pass mouse clicks on to
 * the appropriate instruments for processing.
 */
class FGPanel : public SGSubsystem {
public:
    FGPanel (const SGPropertyNode_ptr root);
    virtual ~FGPanel ();

    // Subsystem API.
    void init() override;
    void bind() override;
    void unbind() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "panel"; }

    // transfer pointer ownership!!!
    virtual void addInstrument (FGPanelInstrument * const instrument);

    // Background texture.
    virtual void setBackground (const FGCroppedTexture_ptr texture);
    void setBackgroundWidth (const double d);
    void setBackgroundHeight (const double d);

    // Background multiple textures.
    virtual void setMultiBackground (const FGCroppedTexture_ptr texture , const int idx);

    // Full width of panel.
    virtual void setWidth (const int width);
    virtual int getWidth () const;

    // Full height of panel.
    virtual void setHeight (const int height);
    virtual int getHeight () const;

private:
    typedef std::vector <FGPanelInstrument *> instrument_list_type;
    int m_width;
    int m_height;

    SGPropertyNode_ptr m_flipx;

    FGCroppedTexture_ptr m_bg;
    double m_bg_width;
    double m_bg_height;
    FGCroppedTexture_ptr m_mbg[8];
    // List of instruments in panel.
    instrument_list_type m_instruments;

    void getInitDisplayList ();

    static GLuint Textured_Layer_Program_Object;
    static GLint Textured_Layer_Position_Loc;
    static GLint Textured_Layer_Tex_Coord_Loc;
    static GLint Textured_Layer_MVP_Loc;
    static GLint Textured_Layer_Sampler_Loc;
};
