/*
 * SPDX-FileName: QmlStringListModel.hxx
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: Copyright (C) 2025 James Turner
 */

#pragma once

#include <QAbstractListModel>
#include <memory>

class QmlStringListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QStringList values READ values WRITE setValues NOTIFY valuesChanged)
    Q_PROPERTY(bool empty READ empty NOTIFY valuesChanged)
public:
    QmlStringListModel(QObject* parent = nullptr);
    ~QmlStringListModel() override;

    void setValues(QStringList v);
    QStringList values() const;


    int rowCount(const QModelIndex& parent) const override;

    QHash<int, QByteArray> roleNames() const override;

    QVariant data(const QModelIndex& m, int role) const override;

    void clear();

    bool empty() const;
signals:
    void valuesChanged();

private:
    QStringList m_values;
};
