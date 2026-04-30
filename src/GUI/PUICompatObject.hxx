// PUICompatDialog.hxx - XML dialog object without using PUI
// SPDX-FileCopyrightText: 2022 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "simgear/structure/SGSharedPtr.hxx"
#include <simgear/math/SGMath.hxx>
#include <simgear/nasal/cppbind/NasalHash.hxx>
#include <simgear/nasal/cppbind/NasalObject.hxx>
#include <simgear/props/propsfwd.hxx>
#include <simgear/structure/SGBinding.hxx>
#include <simgear/structure/SGReferenced.hxx>

#include <Translations/LanguageInfo.hxx>

class PUICompatObject;
class FGPUICompatDialog;

using PUICompatObjectRef = SGSharedPtr<PUICompatObject>;
using PUICompatObjectVec = std::vector<PUICompatObjectRef>;

using PUICompatDialogRef = SGSharedPtr<FGPUICompatDialog>;

class PUICompatObject : public nasal::Object, public SGPropertyChangeListener
{
public:
    static PUICompatObjectRef createForType(const std::string& type, SGPropertyNode_ptr config);

    static void setupGhost(nasal::Hash& guiModule);

    virtual ~PUICompatObject();

    virtual void init();

    virtual void update();

    virtual void apply();

    virtual void updateValue();

    naRef config() const;

    /// return the wrapped props,Node corresponding to our property
    naRef property() const;

    /// return the actual Nasal value of our property: this avoids the need to
    /// create a the property ghost and props.Node wrapper in common cases
    naRef propertyValue(naContext ctx) const;

    PUICompatObjectRef parent() const;

    PUICompatDialogRef dialog() const;

    PUICompatObjectVec children() const;

    naRef show(naRef viewParent);

    double getX() const;
    double getY() const;
    double width() const;
    double height() const;

    SGRectd geometry() const;

    // bool heightForWidth properties

    void setGeometry(const SGRectd& g);

    bool visible() const;
    bool enabled() const;
    const std::string& type() const;

    const std::string& name() const
    {
        return _name;
    }

    void setVisible(bool v);
    void setEnabled(bool e);

    /**
     * @brief find an object (which might be us, or a descendant) with the
     * corresponding name, or nullptr.
     *
     * @param name
     */
    PUICompatObjectRef widgetByName(const std::string& name) const;

    /**
     * @brief Return the value of a config property node
     *
     * @tparam T          type of the return value
     * @param nodeName    leaf name of the property node
     *
     * If the widget config has no child node with the given @a nodeName,
     * return a default-constructed @c T.
     */
    template<typename T>
    T configValue(const std::string& nodeName) const
        {
            const SGPropertyNode* node = _config->getChild(nodeName);
            return node ? node->getValue<T>() : T();
        }

        /**
     * @brief return the radio group ID associated with this widget
     (which is presumably a radio-button)
     *
     * @return std::string the radio-group ID, or an empty string
     */
        std::string radioGroupIdent() const;

        bool isLive() const
        {
            return _live != LiveValueMode::OnApply;
        }

    bool hasBindings() const;

    std::string translatePluralString(flightgear::LanguageInfo::intType cardinal,
                                      const std::string& key,
                                      const std::string& resource = {},
                                      const std::string& domain = {}) const;
    std::string translateString(const std::string& key,
                                const std::string& resource = {},
                                const std::string& domain = {}) const;
    /**
     * @brief Translate a string which may or may not have plural forms
     *
     * @param cardinalNumber  an integer corresponding to a number of
     *                        “things” (concrete or abstract)
     * @param key             basic ID of the translatable string (it is
     *                        subject to strutils::strip())
     * @param resource        translation context in which @a key is defined
     * @param domain          translation domain that @a resource belongs to
     *
     * If @a resource or @a domain is empty, use values suitable for strings
     * extracted from a PUI-compatible dialog XML file.
     *
     * The plural status of the string defined by (domain, resource, key) is
     * queried. If true, @a cardinalNumber determines which plural form to
     * use (via the @c LanguageInfo class); otherwise, this argument isn't
     * used.
     */
    std::string translateWithMaybePlural(
        flightgear::LanguageInfo::intType cardinalNumber,
        const std::string& key, const std::string& resource = {},
        const std::string& domain = {}) const;
protected:
    PUICompatObject(naRef impl, const std::string& type);

    virtual void activateBindings();

    virtual void updateGeometry(const SGRectd& newGeom);

    void valueChanged(SGPropertyNode* node) override;

    SGPropertyNode_ptr _config;

private:
    enum class LiveValueMode {
        OnApply,  ///< not live, only update on explicit apply()
        Listener, ///< live, via SGPropertyListener::valueChanged
        Polled    ///< live, used for tied, non-listener-safe properties
    };

    friend class FGPUICompatDialog;

    friend naRef f_makeCompatObjectPeer(const nasal::CallContext& ctx);

    naRef nasalGetConfigValue(const nasal::CallContext ctx) const;

    void setDialog(PUICompatDialogRef dialog);

    void recursiveUpdate(const std::string& objectName = {});
    void recursiveUpdateValues(const std::string& objectName = {});
    void recursiveApply(const std::string& objectName = {});
    void recursiveOnDelete();

    void doActivate();

    nasal::Hash gridLocation(const nasal::CallContext& ctx) const;

    SGWeakPtr<PUICompatObject> _parent;
    SGWeakPtr<FGPUICompatDialog> _dialog;

    PUICompatObjectVec _children; // owning references to children


    std::string _type;
    std::string _label;
    std::string _name;
    std::string _oldPolledValue;

    SGPropertyNode_ptr _value;
    SGRectd _geometry;

    LiveValueMode _live = LiveValueMode::OnApply;
    bool _valueChanged = false;
    bool _visible = true;
    bool _enabled = true;
    bool _labelChanged = false;

    SGConditionRef _visibleCondition;
    SGConditionRef _enableCondition;

    SGBindingList _bindings;
};
