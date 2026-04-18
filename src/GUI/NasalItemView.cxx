// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 James Turner <james@flightgear.org>

#include "config.h"

#include "NasalItemView.hxx"

// std
#include <algorithm>
#include <any>
#include <cstddef>
#include <string>
#include <utility>

#include "GUI/ItemModel.hxx"

#include "simgear/nasal/cppbind/NasalCallContext.hxx"
#include "simgear/nasal/cppbind/NasalHash.hxx"
#include "simgear/nasal/cppbind/NasalObject.hxx"
#include "simgear/nasal/cppbind/to_nasal.hxx"
#include "simgear/structure/SGReferenced.hxx"
#include "simgear/structure/SGSharedPtr.hxx"
#include "simgear/structure/exception.hxx"


/**
 * @brief Object we can attach to Nasal-defined delegates so they can retrieve
 * their model data.
 *
 */
class DelegateModelData : public SGReferenced
{
public:
    DelegateModelData(NasalItemView* view, size_t i) : _index(i),
                                                       _view(view)
    {
    }

    bool _get(naContext c, const std::string& key, naRef& out) const
    {
        if (key == "yPosition") {
            out = nasal::to_nasal(c, yPosition());
            return true;
        } else if (key == "index") {
            out = nasal::to_nasal(c, _index);
            return true;
        } else if (key == "view") {
            out = nasal::to_nasal(c, _view);
            return true;
        }

        const auto& v = _view->model()->dataAt(_index, key);
        if (!v.has_value()) {
            return false;
        }

        out = nasal::any_to_nasal_helper(c, v);
        return true;
    }

    const int yPosition() const
    {
        return _view->yPositionForIndex(_index);
    }

    std::string label() const
    {
        return _view->model()->labelAt(_index);
    }

    std::any value() const
    {
        return _view->model()->valueAt(_index);
    }

    const size_t index() const { return _index; }

private:
    const size_t _index;
    NasalItemView* _view;
};

using DelegateModelDataPtr = SGSharedPtr<DelegateModelData>;
using NasalDelegateModelData = nasal::Ghost<DelegateModelDataPtr>;

/**
 * @brief storage data about each created delegate.
 *
 */
struct DelegateData {
    DelegateData(size_t i) : index(i) {}

    size_t index;
    nasal::ObjectRef delegate;
    DelegateModelDataPtr modelData;
};


class NasalItemView::NasalItemViewPrivate
{
public:
    NasalItemView* p;

    ItemModelRef m_model;
    mutable size_t m_cachedCount = 0;
    size_t m_callbackRef = 0;
    size_t m_viewHeight = 100;
    size_t m_minScrollBarHeight = 32;
    size_t m_delegateHeight = 32;
    size_t m_scrollOffset = 0;
    size_t m_cacheHeight = 512;                   // how many pixels worth of delegates to cache outside of the visible area.
    std::pair<size_t, size_t> m_lastExtent{0, 0}; // range of currently visible indices, for quick comparison on update

    using DelegateDataVec = std::vector<DelegateData>;
    DelegateDataVec m_delegates;

    using DelegateReuseVec = std::vector<nasal::ObjectRef>;
    DelegateReuseVec m_inactiveDelegates;

    DelegateDataVec::iterator findDelegate(size_t index)
    {
        return std::find_if(m_delegates.begin(), m_delegates.end(), [index](const DelegateData& dd) {
            return dd.index == index;
        });
    }

    DelegateDataVec::const_iterator findDelegate(size_t index) const
    {
        return std::find_if(m_delegates.cbegin(), m_delegates.cend(), [index](const DelegateData& dd) {
            return dd.index == index;
        });
    }

    // returns the y pixels covered by a model index
    std::pair<size_t, size_t> yExtentForIndex(size_t index) const
    {
        const auto y = p->yPositionForIndex(index);
        return std::make_pair(y, y + m_delegateHeight);
    }

    bool isVisibleOrCached(size_t index) const
    {
        assert(m_lastExtent.first != m_lastExtent.second);
        return index >= m_lastExtent.first && index <= m_lastExtent.second;
    }

    // this returns the cached model/row indices, not pixels
    std::pair<size_t, size_t> cachedExtent() const
    {
        const auto totalHeight = m_delegateHeight * m_cachedCount;
        const auto minCached = std::max(0, static_cast<int>(m_scrollOffset) - static_cast<int>(m_cacheHeight));
        const auto maxCached = std::min(totalHeight, m_scrollOffset + m_viewHeight + m_cacheHeight);
        const auto lastIndex = m_cachedCount - 1;
        return std::make_pair(minCached / m_delegateHeight, std::min(maxCached / m_delegateHeight, lastIndex));
    }

    void resetExtent()
    {
        m_lastExtent = std::make_pair(0, 0);
    }
};

NasalItemView::NasalItemView(naRef impl) : nasal::Object(impl),
                                           d(std::make_unique<NasalItemViewPrivate>())
{
    d->p = this;
}

NasalItemView::~NasalItemView()
{
    if (d->m_model) {
        d->m_model->removeChangeCallback(d->m_callbackRef);
    }
}

void NasalItemView::setModel(ItemModelRef m)
{
    if (d->m_model) {
        d->m_model->removeChangeCallback(d->m_callbackRef);
    }

    d->m_model = m;

    auto cb = [this](ItemModel::Change t, size_t row, size_t count) {
        onCallback(t, row, count);
    };

    d->m_cachedCount = d->m_model->count();
    d->m_callbackRef = d->m_model->addChangeCallback(cb);
    callMethod<void>("modelReset");

    d->resetExtent();
    update();
}

ItemModelRef NasalItemView::model() const
{
    return d->m_model;
}

void NasalItemView::setViewHeight(int h)
{
    if (d->m_viewHeight == h) {
        return;
    }

    d->m_viewHeight = h;
    d->resetExtent();
    update();
}

void NasalItemView::setMinimumScrollBarHeight(int h)
{
    d->m_minScrollBarHeight = h;
    callMethod<void>("scrollBarChanged");
}

void NasalItemView::setViewOffset(int y)
{
    if (d->m_scrollOffset == y) {
        return;
    }

    d->m_scrollOffset = y;
    update();
}

void NasalItemView::setDelegateHeight(int h)
{
    if (d->m_delegateHeight == h) {
        return;
    }

    d->m_delegateHeight = h;
    // FIXME : update all delegate view positions
    d->resetExtent();
    update();
}

int NasalItemView::firstVisibleIndex() const
{
    return d->m_lastExtent.first;
}

int NasalItemView::lastVisibleIndex() const
{
    return std::min(d->m_lastExtent.second, d->m_cachedCount - 1);
}

int NasalItemView::indexForViewPosition(int y) const
{
    return static_cast<int>(floor((y + d->m_scrollOffset) / d->m_delegateHeight));
}

int NasalItemView::yPositionForIndex(int index) const
{
    // later: add header offset, etc
    return index * d->m_delegateHeight;
}

int NasalItemView::scrollBarHeight() const
{
    return std::max(d->m_minScrollBarHeight,
                    (d->m_cachedCount * d->m_delegateHeight) - d->m_viewHeight);
}

int NasalItemView::scrollBarPosition() const
{
    const auto lastScrollOffset = (d->m_cachedCount * d->m_delegateHeight) - d->m_viewHeight;
    const auto scrollRange = d->m_viewHeight - scrollBarHeight();
    return scrollRange * d->m_scrollOffset / lastScrollOffset;
}

int NasalItemView::cacheHeight() const
{
    return d->m_cacheHeight;
}

void NasalItemView::setCacheHeight(int h)
{
    if (d->m_cacheHeight == h) {
        return;
    }

    d->m_cacheHeight = h;
    d->resetExtent();
    update();
}

void NasalItemView::update()
{
    const auto cachedExtent = d->cachedExtent();
    if (d->m_lastExtent == cachedExtent) {
        return;
    }

    d->m_lastExtent = cachedExtent;
    // remove any stale delegates that are no longer visible or in the cached area
    auto it = std::remove_if(d->m_delegates.begin(), d->m_delegates.end(), [this, cachedExtent](const DelegateData& dd) {
        if (dd.index < cachedExtent.first || dd.index > cachedExtent.second) {
            dd.delegate->callMethod<void>("unbind");
            d->m_inactiveDelegates.push_back(dd.delegate);
            return true;
        }
        return false;
    });

    d->m_delegates.erase(it, d->m_delegates.end());

    // create new delegates as needed
    // would be faster if we kept m_delegates sorted
    for (auto index = cachedExtent.first; index <= cachedExtent.second; ++index) {
        auto it = d->findDelegate(index);
        if (it == d->m_delegates.end()) {
            getOrCreateDelegate(index);
        }
    }

    callMethod<void>("visibleRowsChanged");
    callMethod<void>("scrollBarChanged");
}

void NasalItemView::onCallback(ItemModel::Change t, size_t row, size_t count)
{
    using Change = ItemModel::Change;
    switch (t) {
    case Change::Reset:
        for (auto& dd : d->m_delegates) {
            dd.delegate->callMethod<void>("unbind");
            d->m_inactiveDelegates.push_back(dd.delegate);
        }
        d->m_delegates.clear();
        d->m_cachedCount = d->m_model->count();
        d->resetExtent();
        callMethod<void>("modelReset");
        update();
        break;

    case Change::Modified:
        for (int r = row; r < (row + count); ++r) {
            callMethod<void>("dataChanged", r);
            auto it = d->findDelegate(r);
            if (it != d->m_delegates.end()) {
                it->delegate->callMethod<void>("dataChanged");
            }
        }
        break;

    case Change::RowsWillBeAdded:

        break;

    case Change::RowsAdded:
        d->m_cachedCount = d->m_model->count();
        d->resetExtent();

        // modify existing delegates indices
        for (int r = row + count; r < d->m_cachedCount; ++r) {
            auto it = d->findDelegate(r);
            if (it != d->m_delegates.end()) {
                it->index = r;
                it->delegate->callMethod<void>("moved");
            }
        }

        update();
        break;

    case Change::RowsWillBeRemoved:
        break;

    case Change::RowsRemoved:

        // remove existing delegates first
        for (int r = row; r < (row + count - 1); ++r) {
            unbindDelegate(r);
        }

        // move delegates after the removed row(s)
        for (int r = row + count; r < d->m_cachedCount; ++r) {
            auto it = d->findDelegate(r);
            if (it != d->m_delegates.end()) {
                it->index = r;
                it->delegate->callMethod<void>("moved");
            }
        }

        // now update our cached count
        d->m_cachedCount = d->m_model->count();
        d->resetExtent();
        update();

        break;
    } // of Change type
}

nasal::ObjectRef NasalItemView::getOrCreateDelegate(size_t index)
{
    nasal::ObjectRef delegate;
    if (!d->m_inactiveDelegates.empty()) {
        delegate = d->m_inactiveDelegates.back();
        d->m_inactiveDelegates.pop_back();
    } else {
        auto r = callMethod<naRef>("createDelegate");
        delegate = new nasal::Object(r);
    }

    setDelegateForIndex(index, delegate);
    return delegate;
}

void NasalItemView::unbindDelegate(size_t index)
{
    auto it = d->findDelegate(index);
    if (it == d->m_delegates.end()) {
        return;
    }

    nasal::ObjectRef delegate = it->delegate;
    delegate->callMethod<void>("unbind");
    d->m_inactiveDelegates.push_back(delegate);
    d->m_delegates.erase(it);
}

nasal::ObjectRef NasalItemView::delegate(size_t index) const
{
    auto it = d->findDelegate(index);
    if (it == d->m_delegates.end()) {
        return {};
    }

    return it->delegate;
}

void NasalItemView::setDelegateForIndex(size_t index, nasal::ObjectRef delegate)
{
    auto it = d->findDelegate(index);
    if (it != d->m_delegates.end()) {
        throw sg_exception("Setting duplicate delegate for index:" + std::to_string(index));
    }

    // TODO: sort by index for faster lookup
    d->m_delegates.emplace_back(DelegateData{index});
    auto& dd = d->m_delegates.back();
    dd.delegate = delegate;
    dd.index = index;
    dd.modelData = new DelegateModelData(this, index);
    // set model and index directly?
    delegate->callMethod<void>("bind", index, dd.modelData);
}

void NasalItemView::dumpDelegates() const
{
    SG_LOG(SG_GUI, SG_INFO, "Visible range:" << d->m_lastExtent.first << "-" << d->m_lastExtent.second);
    SG_LOG(SG_GUI, SG_INFO, "Current delegates:");
    for (const auto& dd : d->m_delegates) {
        SG_LOG(SG_GUI, SG_INFO, "  index:" << dd.index);
    }
    SG_LOG(SG_GUI, SG_INFO, "Inactive delegates:" << d->m_inactiveDelegates.size());
}

static naRef f_makeNasalItemView(const nasal::CallContext& ctx)
{
    naRef peer = ctx.requireArg<naRef>(0);
    ItemModelRef model = ctx.getArg<ItemModelRef>(1);
    NasalItemViewRef view(new NasalItemView(peer));
    if (model) {
        view->setModel(model);
    }
    return ctx.to_nasal(view);
}

void NasalItemView::setupGhost(nasal::Hash& ns)
{
    nasal::Ghost<NasalItemViewRef>::init("gui.ItemView")
        .bases<nasal::ObjectRef>()
        .member("viewHeight", &NasalItemView::setViewHeight)
        .member("viewOffset", &NasalItemView::setViewOffset)
        .member("delegateHeight", &NasalItemView::setDelegateHeight)
        .member("minimumScrollBarHeight", &NasalItemView::setMinimumScrollBarHeight)
        .member("model", &NasalItemView::model, &NasalItemView::setModel)
        .member("cacheHeight", &NasalItemView::cacheHeight, &NasalItemView::setCacheHeight)
        .method("delegateForIndex", &NasalItemView::delegate)
        .method("indexForViewPosition", &NasalItemView::indexForViewPosition)
        .method("dumpDelegates", &NasalItemView::dumpDelegates);

    nasal::Ghost<DelegateModelDataPtr>::init("gui.ItemDelegateModelData")
        .member("label", &DelegateModelData::label)
        .member("value", &DelegateModelData::value)
        .member("index", &DelegateModelData::index)
        .member("yPosition", &DelegateModelData::yPosition)
        ._get(&DelegateModelData::_get);


    nasal::Hash view_hash = ns.createHash("ItemView");
    view_hash.set("new", &f_makeNasalItemView);
}
