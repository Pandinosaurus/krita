/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2011 Silvio Heinrich <plassyqweb.de>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kis_paintop_options_model.h"

#include <klocalizedstring.h>


KisPaintOpOptionListModel::KisPaintOpOptionListModel(QObject *parent)
    : BaseOptionCategorizedListModel(parent)
{
    connect(&m_stateSignalsMapper, &QSignalMapper::mappedInt,
            this, &KisPaintOpOptionListModel::slotCheckedEnabledStateChanged);
}

QString KisPaintOpOptionListModel::categoryName(KisPaintOpOption::PaintopCategory categoryType)
{
    QString category;
    switch(categoryType) {
    case KisPaintOpOption::GENERAL:
        category = i18nc("option category", "General");
        break;
    case KisPaintOpOption::COLOR:
        category = i18nc("option category", "Color");
        break;
    case KisPaintOpOption::TEXTURE:
        category = i18nc("option category", "Texture");
        break;
    case KisPaintOpOption::FILTER:
        category = i18nc("option category", "Filter");
        break;
    case KisPaintOpOption::MASKING_BRUSH:
        category = i18nc("option category", "Masked Brush");
        break;
    };
    return category;
}

void KisPaintOpOptionListModel::addPaintOpOption(KisPaintOpOption *option, int widgetIndex, const QString &label, const QString &category) {

    DataItem *item = categoriesMapper()->addEntry(category, KisOptionInfo(option, widgetIndex, label));

    if (option->isCheckable()) {
        item->setCheckable(true);
        item->setChecked(option->isChecked());
        connect(option, &KisPaintOpOption::sigCheckedChanged,
                &m_stateSignalsMapper, qOverload<>(&QSignalMapper::map));
    }

    item->setEnabled(option->isEnabled());
    connect(option, &KisPaintOpOption::sigEnabledChanged,
            &m_stateSignalsMapper, qOverload<>(&QSignalMapper::map));
    m_stateSignalsMapper.setMapping(option, categoriesMapper()->rowFromItem(item));

    categoriesMapper()->expandAllCategories();
}

QVariant KisPaintOpOptionListModel::data(const QModelIndex& idx, int role) const
{
    return BaseOptionCategorizedListModel::data(idx, role);
}

bool KisPaintOpOptionListModel::setData(const QModelIndex& idx, const QVariant& value, int role)
{
    if (!idx.isValid()) return false;

    DataItem *item = categoriesMapper()->itemFromRow(idx.row());
    Q_ASSERT(item);

    if (role == Qt::CheckStateRole && item->isCheckable()) {
        item->data()->option->setChecked(value.toInt() == Qt::Checked);
    }

    return BaseOptionCategorizedListModel::setData(idx, value, role);
}

bool operator==(const KisOptionInfo& a, const KisOptionInfo& b)
{
    if (a.index != b.index) return false;
    if (a.option->objectName() == b.option->objectName())
    if (a.option->category() != b.option->category()) return false;
    if (a.option->isCheckable() != b.option->isCheckable()) return false;
    if (a.option->isChecked() != b.option->isChecked()) return false;
    return true;
}
void KisPaintOpOptionListModel::signalDataChanged(const QModelIndex& index)
{
    Q_EMIT dataChanged(index,index);
}

void KisPaintOpOptionListModel::slotCheckedEnabledStateChanged(int row)
{
    QModelIndex idx(index(row));

    DataItem *item = categoriesMapper()->itemFromRow(row);
    KIS_SAFE_ASSERT_RECOVER_RETURN(item);

    if (item->data()->option->isEnabled() != item->isEnabled()) {
        item->setEnabled(item->data()->option->isEnabled());
    }

    if (item->data()->option->isChecked() != item->isChecked()) {
        item->setChecked(item->data()->option->isChecked());
    }

    Q_EMIT dataChanged(idx, idx);
}
