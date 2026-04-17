// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 James Turner <james@flightgear.org>

/**
 * @file
 * @brief Abstract models for the GUI, similar to QAbstractList Model
 */

// std
#include <any>
#include <cstddef>
#include <iterator>

#include <simgear/props/props.hxx>
#include <simgear/structure/exception.hxx>

#include <simgear/nasal/cppbind/NasalObject.hxx>

#include <GUI/AirportListModel.hxx>
#include <GUI/ItemModel.hxx>
#include <Scripting/NasalSys.hxx>

size_t ItemModel::addChangeCallback(ChangeCallback cb)
{
    const auto r = m_callbacks.size();
    m_callbacks.push_back(cb);
    return r;
}

void ItemModel::removeChangeCallback(size_t index)
{
    if (index >= m_callbacks.size()) {
        return;
    }

    m_callbacks.erase(m_callbacks.begin() + index);
}

void ItemModel::didChangeData(int row, int changedRows)
{
    assert(row >= 0);
    assert(row < (int)count());
    fireCallbacks(Change::Modified, row, changedRows);
}

void ItemModel::fireCallbacks(Change t, size_t row, size_t count)
{
    for (auto cb : m_callbacks) {
        cb(t, row, count);
    }
}

void ItemModel::didReset()
{
    m_cachedCount = count();
    fireCallbacks(Change::Reset, 0);
}

void ItemModel::beginAddRows(int row, int count)
{
    assert(!m_addRemoveActive);
    assert(row < m_cachedCount);
    m_addRemoveActive = true;
    m_addRemoveRow = row;
    m_addRemoveCount = count;
    fireCallbacks(Change::RowsWillBeAdded, row, count);
}

void ItemModel::endAddRows()
{
    assert(m_addRemoveActive);
    m_cachedCount += m_addRemoveCount;
    fireCallbacks(Change::RowsAdded, m_addRemoveRow, m_addRemoveCount);
    m_addRemoveActive = false;
}

void ItemModel::beginRemoveRows(int row, int count)
{
    assert(!m_addRemoveActive);
    assert((row + count) <= m_cachedCount);

    m_addRemoveActive = true;
    m_addRemoveRow = row;
    m_addRemoveCount = count;
    fireCallbacks(Change::RowsWillBeRemoved, row, count);
}

void ItemModel::endRemoveRows()
{
    assert(m_addRemoveActive);
    m_cachedCount -= m_addRemoveCount;
    fireCallbacks(Change::RowsRemoved, m_addRemoveRow, m_addRemoveCount);
    m_addRemoveActive = false;
}

std::string ItemModel::labelAt(size_t index) const
{
    return std::any_cast<std::string>(dataAt(index, "label"));
}

std::any ItemModel::valueAt(size_t index) const
{
    return dataAt(index, "value");
}

///////////////////////////////////////////////////////////////////////////////

static naRef f_makeNasalItemModel(const nasal::CallContext& ctx);

class NasalItemModel : public ItemModel, public nasal::Object
{
public:
    NasalItemModel(naRef impl) : Object(impl)
    {
    }

    ~NasalItemModel() = default;

    size_t count() const override
    {
        if (_cachedCount.has_value()) {
            return _cachedCount.value();
        }

        _cachedCount = const_cast<NasalItemModel*>(this)->callMethod<int>("count");
        return _cachedCount.value();
    }

    std::any dataAt(size_t index, const std::string& key) const override
    {
        if (!_cachedCount.has_value()) {
            count();
        }

        if (index >= _cachedCount.value()) {
            throw sg_range_exception("Index out of bounds");
        }

        return const_cast<NasalItemModel*>(this)->callMethod<std::any>("modelData", index, key);
    }

    void dataChanged(int row, int count = 1)
    {
        didChangeData(row, count);
    }

    void rowsAdded(int firstRow, int count)
    {
        _cachedCount.reset();
        beginAddRows(firstRow, count);
        endAddRows();
    }

    void rowsRemoved(int firstRow, int count)
    {
        _cachedCount.reset();
        beginRemoveRows(firstRow, count);
        endRemoveRows();
    }

    void doReset()
    {
        _cachedCount.reset();
        didReset();
    }

private:
    mutable std::optional<size_t> _cachedCount;
};

using NasalItemModelRef = SGSharedPtr<NasalItemModel>;

std::any propertyValueToAny(SGPropertyNode_ptr p)
{
    if (!p || !p->hasValue()) {
        return {};
    }

    switch (p->getType()) {
    case simgear::props::BOOL:
        return p->getBoolValue();
    case simgear::props::INT:
    case simgear::props::LONG:
        return p->getIntValue();
    case simgear::props::FLOAT:
    case simgear::props::DOUBLE:
        return p->getDoubleValue();
    case simgear::props::STRING:
        return p->getStringValue();
    default:
        throw sg_exception("Add support for extended property types to propertyValueToAny");
    }
}

///////////////////////////////////////////////////////////////////////////////

class PropertyItemModel::PropertyItemModelPrivate : public SGPropertyChangeListener
{
public:
    PropertyItemModelPrivate(PropertyItemModel* outer) : p(outer) {}

    // implement SGPropertyChangeListener interfaces
    void valueChanged(SGPropertyNode* node) override
    {
        // because we recursively observe below our root, we might see
        // value changed for grandchild (or deeper) properties. Walk up
        // the parent chain to find our root, so we know the model index
        // which is changing.
        SGPropertyNode_ptr childOfRoot = node;
        while (childOfRoot && childOfRoot->getParent() != m_root) {
            childOfRoot = childOfRoot->getParent();
        }

        if (!childOfRoot) {
            // broken logic
            SG_LOG(SG_GUI, SG_DEV_ALERT, "Failed to find PropertyItemModel root '" << m_root->getPath() << "' as parent of node:" << node->getPath());
            return;
        }

        // node changed, but it fails our filter
        if (childOfRoot->getNameString() != m_itemName) {
            return;
        }

        validateItemCache();
        auto it = std::find(m_itemCache.begin(), m_itemCache.end(), childOfRoot);
        if (it == m_itemCache.end()) {
            return;
        }

        const auto index = std::distance(m_itemCache.begin(), it);
        p->didChangeData(index, 1);
    }

    void childAdded(SGPropertyNode* parent, SGPropertyNode* child) override
    {
        if (parent != m_root) {
            return;
        }

        if (child->getNameString() != m_itemName) {
            return;
        }

        validateItemCache();
        const auto newChildIndex = child->getIndex();
        auto it = std::find_if(m_itemCache.begin(), m_itemCache.end(), [newChildIndex](SGPropertyNode* c) {
            return newChildIndex < c->getIndex();
        });

        const auto row = std::distance(m_itemCache.begin(), it);
        p->beginAddRows(row, 1);
        m_itemCache.insert(it, child);
        p->endAddRows();
    }

    void childRemoved(SGPropertyNode* parent, SGPropertyNode* child) override
    {
        if (parent != m_root) {
            return;
        }

        if (child->getNameString() != m_itemName) {
            return;
        }

        validateItemCache();
        auto it = std::find(m_itemCache.begin(), m_itemCache.end(), child);
        if (it == m_itemCache.end()) {
            return;
        }

        const auto row = std::distance(m_itemCache.begin(), it);
        p->beginRemoveRows(row, 1);
        m_itemCache.erase(it);
        p->endRemoveRows();
    }

    void validateItemCache() const
    {
        if (m_cacheValid) {
            return;
        }

        simgear::PropertyList cache;
        cache = m_root->getChildren(m_itemName);
        m_itemCache = cache;
        m_cacheValid = true;
    }

    void rebuildItemCache() const
    {
        m_cacheValid = false;
        p->didReset();
    }

    void resetPropAttributes()
    {
        const string_list dataNodes = {m_labelPath, m_valuePath};
        for (auto c : m_root->getChildren(m_itemName)) {
            c->setAttribute(SGPropertyNode::VALUE_CHANGED_DOWN, true);
            c->setAttribute(SGPropertyNode::VALUE_CHANGED_UP, true);
            // if there's existing child/leaf nodes, ensure they fire valueChanged up
            for (auto p : dataNodes) {
                auto dn = c->getNode(p);
                if (dn) {
                    dn->setAttribute(SGPropertyNode::VALUE_CHANGED_UP, true);
                }
            }
        }
    }

    PropertyItemModel* p;
    SGPropertyNode_ptr m_root;
    std::string m_labelPath; ///< relative path to label property, inside each item. (can be '.' for direct value)
    std::string m_valuePath; ///< relative path to value property, inside each item.
    std::string m_itemName;  ///< child node name of the root, we include in the model.

    mutable bool m_cacheValid = false;
    /**
     * cached, ordered array of the nodes we are using for the model
     *
     */
    mutable simgear::PropertyList m_itemCache;
};

PropertyItemModel::PropertyItemModel(SGPropertyNode_ptr root) : d(new PropertyItemModelPrivate(this))
{
    assert(root);
    d->m_root = root;
    d->m_root->setAttribute(SGPropertyNode::VALUE_CHANGED_DOWN, true);

    root->addChangeListener(d.get());
}

PropertyItemModel::~PropertyItemModel()
{
    d->m_root->removeChangeListener(d.get());
}

size_t PropertyItemModel::count() const
{
    d->validateItemCache();
    return d->m_itemCache.size();
}

void PropertyItemModel::setItemName(const std::string& s)
{
    d->m_itemName = s;
    d->resetPropAttributes();
    d->rebuildItemCache();
}

void PropertyItemModel::setLabelPath(const std::string& s)
{
    d->m_labelPath = s;
    d->resetPropAttributes();
    d->rebuildItemCache();
}

void PropertyItemModel::setValuePath(const std::string& s)
{
    d->m_valuePath = s;
    d->resetPropAttributes();
    d->rebuildItemCache();
}

std::any PropertyItemModel::dataAt(size_t index, const std::string& key) const
{
    d->validateItemCache();
    if (index >= d->m_itemCache.size()) {
        throw sg_range_exception("Invalid model index");
    }

    const auto& node = d->m_itemCache.at(index);
    if (key == "label") {
        if (d->m_labelPath == ".") {
            return propertyValueToAny(node);
        }
        return propertyValueToAny(node->getNode(d->m_labelPath));
    } else if (key == "value") {
        // allow specifying the index as the value, for easy list-of-strings models
        if (d->m_valuePath == "#") {
            return std::any{index};
        }
        return propertyValueToAny(node->getNode(d->m_valuePath));
    }

    auto n = node->getNode(key);
    return propertyValueToAny(n);
}

static naRef f_makePropetyItemModel(const nasal::CallContext& ctx)
{
    const auto itemName = ctx.getArg<std::string>(1);
    const auto rootNode = ghostToPropNode(ctx.requireArg<naRef>(0));
    PropertyItemModel* pi = new PropertyItemModel(rootNode);
    if (!itemName.empty()) {
        pi->setItemName(itemName);
    }
    return ctx.to_nasal(ItemModelRef(pi));
}

static naRef f_makeNasalItemModel(const nasal::CallContext& ctx)
{
    return ctx.to_nasal(NasalItemModelRef(
        new NasalItemModel(ctx.requireArg<naRef>(0))));
}

static naRef f_makeAirportsModel(const nasal::CallContext& ctx)
{
    return ctx.to_nasal(SGSharedPtr<AirportListModel>(
        new AirportListModel()));
}

void ItemModel::setupGhosts(nasal::Hash& ns)
{
    nasal::Ghost<ItemModelRef>::init("gui.AbstractItemModel")
        .member("count", &ItemModel::count)
        .method("dataAt", &ItemModel::dataAt);
    nasal::Ghost<SGSharedPtr<NasalItemModel>>::init("gui.NasalItemModel")
        .bases<ItemModelRef>()
        .bases<nasal::ObjectRef>()
        .method("modelDataChanged", &NasalItemModel::dataChanged)
        .method("rowsAdded", &NasalItemModel::rowsAdded)
        .method("rowsRemoved", &NasalItemModel::rowsRemoved)
        .method("reset", &NasalItemModel::doReset);

    nasal::Hash nimodel_hash = ns.createHash("NasalItemModel");
    nimodel_hash.set("new", &f_makeNasalItemModel);

    nasal::Ghost<SGSharedPtr<PropertyItemModel>>::init("gui.PropertyItemModel")
        .bases<ItemModelRef>()
        .member("itemName", &PropertyItemModel::setItemName)
        .member("labelPath", &PropertyItemModel::setLabelPath)
        .member("valuePath", &PropertyItemModel::setValuePath);

    nasal::Hash pmodel_hash = ns.createHash("PropertyItemModel");
    pmodel_hash.set("new", &f_makePropetyItemModel);

    nasal::Ghost<SGSharedPtr<AirportListModel>>::init("gui.AirportsModel")
        .bases<ItemModelRef>()
        .member("search", &AirportListModel::setSearchTerm)
        .member("heliports", &AirportListModel::setQueryHeliports);

    nasal::Hash aptmodel_hash = ns.createHash("AirportsModel");
    aptmodel_hash.set("new", &f_makeAirportsModel);
}
