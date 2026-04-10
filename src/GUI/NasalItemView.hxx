// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 James Turner <james@flightgear.org>

#pragma once

#include <simgear/nasal/cppbind/NasalObject.hxx>

#include "ItemModel.hxx"

class NasalItemView : public nasal::Object
{
public:
    NasalItemView(naRef impl);
    ~NasalItemView();

    void setModel(ItemModelRef m);
    ItemModelRef model() const;

    static void setupGhost(nasal::Hash& ns);

    void setViewHeight(int h);
    void setViewOffset(int y);
    void setDelegateHeight(int h);
    void setMinimumScrollBarHeight(int h);

    int firstVisibleIndex() const;
    int lastVisibleIndex() const;

    int indexForViewPosition(int y) const;
    int yPositionForIndex(int index) const;

    int scrollBarHeight() const;
    int scrollBarPosition() const;

    // TODO: add selection support
    // allow selected row to be taller

    nasal::ObjectRef delegate(size_t index) const;

    int cacheHeight() const;
    void setCacheHeight(int h);

    void dumpDelegates() const;

private:
    void update();

    void onCallback(ItemModel::Change t, size_t row, size_t count);

    nasal::ObjectRef getOrCreateDelegate(size_t index);

    void unbindDelegate(size_t index);


    void setDelegateForIndex(size_t index, nasal::ObjectRef delegate);

    class NasalItemViewPrivate;
    std::unique_ptr<NasalItemViewPrivate> d;
};

using NasalItemViewRef = SGSharedPtr<NasalItemView>;
