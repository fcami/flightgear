// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 James Turner <james@flightgear.org>

/**
 * @file
 * @brief Abstract models for the GUI, similar to QAbstractList Model
 */

#pragma once

#include "simgear/props/props.hxx"
#include <any>
#include <functional>

namespace nasal {
class Hash;
}

class ItemModel : virtual public SGVirtualWeakReferenced
{
public:
    virtual ~ItemModel() = default;

    std::string labelAt(size_t index) const;
    std::any valueAt(size_t index) const;

    virtual size_t count() const = 0;
    virtual std::any dataAt(size_t index, const std::string& key) const = 0;

    enum class Change {
        Reset,
        Modified,
        RowsWillBeAdded,
        RowsAdded,
        RowsWillBeRemoved,
        RowsRemoved
    };

    using ChangeCallback = std::function<void(Change t, size_t row, size_t count)>;

    size_t addChangeCallback(ChangeCallback cb);
    void removeChangeCallback(size_t index);

    static void setupGhosts(nasal::Hash& ns);

protected:
    // helpers for implementation classes to use, to generate
    // change callback events

    void didChangeData(int row, int count);
    void didReset();

    void beginAddRows(int row, int count);
    void endAddRows();

    void beginRemoveRows(int row, int count);
    void endRemoveRows();

private:
    void fireCallbacks(Change t, size_t row, size_t count = 1);

    bool m_addRemoveActive = false;
    size_t m_addRemoveRow = 0,
           m_addRemoveCount = 0;
    std::vector<ChangeCallback> m_callbacks;
    mutable size_t m_cachedCount = 0;
};

using ItemModelRef = SGSharedPtr<ItemModel>;

class PropertyItemModel : public ItemModel
{
public:
    PropertyItemModel(SGPropertyNode_ptr root);
    virtual ~PropertyItemModel();

    void setItemName(const std::string& s);
    void setLabelPath(const std::string& s);
    void setValuePath(const std::string& s);

    size_t count() const override;
    std::any dataAt(size_t index, const std::string& key) const override;

private:
    class PropertyItemModelPrivate;
    friend class PropertyItemModelPrivate;
    std::unique_ptr<PropertyItemModelPrivate> d;
};
