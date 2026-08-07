/*
 * SPDX-FileName: QmlStringListModel.cxx
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: Copyright (C) 2025 James Turner
 */

#include "QmlStringListModel.hxx"

#include <algorithm>


QmlStringListModel::QmlStringListModel(QObject* parent) : QAbstractListModel(parent)
{
}

QmlStringListModel::~QmlStringListModel() = default;

void QmlStringListModel::setValues(QStringList v)
{
    beginResetModel();
    m_values = v;
    endResetModel();
    emit valuesChanged();
}

QStringList QmlStringListModel::values() const
{
    return m_values;
}

void QmlStringListModel::setKeys(QVariantList v)
{
    beginResetModel();
    m_keys = v;
    endResetModel();
    emit keysChanged();
}

QVariantList QmlStringListModel::keys() const
{
    return m_keys;
}

int QmlStringListModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return m_values.size();
}

QHash<int, QByteArray> QmlStringListModel::roleNames() const
{
    auto roles = QAbstractListModel::roleNames();
    return roles;
}

QVariant QmlStringListModel::data(const QModelIndex& m, int role) const
{
    auto s = m_values.at(m.row());
    if (role == Qt::DisplayRole) {
        return s;
    }

    return {};
}

void QmlStringListModel::clear()
{
    beginResetModel();
    m_values.clear();
    m_keys.clear();
    endResetModel();
    emit valuesChanged();
    emit keysChanged();
}

bool QmlStringListModel::empty() const
{
    return m_values.isEmpty();
}

int QmlStringListModel::indexOfKey(const QVariant& key) const
{
    return m_keys.indexOf(key);
}
