/*
 *  SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "widgets/kis_multi_integer_filter_widget.h"
#include <QLabel>
#include <QLayout>
#include <QTimer>
#include <QSpinBox>
#include <QGridLayout>

#include <filter/kis_filter_configuration.h>
#include <klocalizedstring.h>
#include <KisGlobalResourcesInterface.h>
#include "kis_slider_spin_box.h"
#include "kis_aspect_ratio_locker.h"

KisDelayedActionIntegerInput::KisDelayedActionIntegerInput(QWidget * parent, const QString & name)
    : KisSliderSpinBox(parent)
{
    setObjectName(name);
    m_timer = new QTimer(this);
    m_timer->setObjectName(name);
    m_timer->setSingleShot(true);
    connect(m_timer, SIGNAL(timeout()), SLOT(slotValueChanged()));
    connect(this, SIGNAL(valueChanged(int)), SLOT(slotTimeToUpdate()));
}

void KisDelayedActionIntegerInput::slotTimeToUpdate()
{
    m_timer->start(50);
}

void KisDelayedActionIntegerInput::slotValueChanged()
{
    Q_EMIT valueChangedDelayed(value());
}

void KisDelayedActionIntegerInput::cancelDelayedSignal()
{
    m_timer->stop();
}

KisIntegerWidgetParam::KisIntegerWidgetParam(qint32 nmin, qint32 nmax, qint32 ninitvalue, const QString & label, const QString & nname, const QString & lockerName)
    : min(nmin)
    , max(nmax)
    , initvalue(ninitvalue)
    , label(label)
    , name(nname)
    , lockerName(lockerName)
{
}

KisMultiIntegerFilterWidget::KisMultiIntegerFilterWidget(const QString& filterid,
                                                         QWidget* parent,
                                                         const QString& caption,
                                                         vKisIntegerWidgetParam iwparam)
    : KisConfigWidget(parent)
    , m_filterid(filterid)
    , m_config(new KisFilterConfiguration(filterid, 0, KisGlobalResourcesInterface::instance()))
{
    this->setWindowTitle(caption);

    QGridLayout *widgetLayout = new QGridLayout(this);
    widgetLayout->setColumnStretch(1, 1);
    widgetLayout->setContentsMargins(0,0,0,0);
    widgetLayout->setHorizontalSpacing(0);

    for (uint i = 0; i < iwparam.size(); ++i) {
        KisDelayedActionIntegerInput *widget = new KisDelayedActionIntegerInput(this, iwparam[i].name);

        widget->setRange(iwparam[i].min, iwparam[i].max);
        widget->setValue(iwparam[i].initvalue);
        widget->cancelDelayedSignal();

        connect(widget, SIGNAL(valueChangedDelayed(int)), SIGNAL(sigConfigurationItemChanged()));

        QLabel* lbl = new QLabel(iwparam[i].label + ':', this);
        widgetLayout->addWidget(lbl, i , 0);

        widgetLayout->addWidget(widget, i , 1);

        m_integerWidgets.append(widget);

        // Add an aspect ratio locker if requested;
        // it must be requested by the second paired spinbox only.
        if (!iwparam[i].lockerName.isEmpty()) {
            KoAspectButton *aspectButton = new KoAspectButton(this);
            aspectButton->setObjectName(iwparam[i].lockerName);
            widgetLayout->addWidget(aspectButton, i-1, 2, 2, 1);
            KisAspectRatioLocker *aspectLocker = new KisAspectRatioLocker(this);
            aspectLocker->connectSpinBoxes(dynamic_cast<KisSliderSpinBox*>(m_integerWidgets[i-1]), dynamic_cast<KisSliderSpinBox*>(widget), aspectButton);

            m_aspectButtons.append(aspectButton);
        }


    }
    widgetLayout->setRowStretch(iwparam.size(),1);

    QSpacerItem * sp = new QSpacerItem(1, 1);
    widgetLayout->addItem(sp, iwparam.size(), 0);
}

KisMultiIntegerFilterWidget::~KisMultiIntegerFilterWidget()
{
}

void KisMultiIntegerFilterWidget::setConfiguration(const KisPropertiesConfigurationSP config)
{
    if (!config) return;

    if (!m_config) {
        m_config = new KisFilterConfiguration(m_filterid, 0, KisGlobalResourcesInterface::instance());
    }

    m_config->fromXML(config->toXML());
    for (int i = 0; i < nbValues(); ++i) {

    // Keep aspect must be unset first if false, to not interfere with the values
    Q_FOREACH(KoAspectButton *aspectButton, m_aspectButtons) {
        if (!config->getBool(aspectButton->objectName(), true)) {
            aspectButton->setKeepAspectRatio(false);
        }
    }

        KisDelayedActionIntegerInput*  w = m_integerWidgets[i];
        if (w) {
            int val = config->getInt(m_integerWidgets[i]->objectName());
            m_integerWidgets[i]->setValue(val);
            m_integerWidgets[i]->cancelDelayedSignal();
        }
    }

    // Keep aspect must be set last if true, to not interfere with the values
    Q_FOREACH(KoAspectButton *aspectButton, m_aspectButtons) {
        if (config->getBool(aspectButton->objectName(), true)) {
            aspectButton->setKeepAspectRatio(true);
        }
    }

}

KisPropertiesConfigurationSP KisMultiIntegerFilterWidget::configuration() const
{
    KisFilterConfigurationSP config = new KisFilterConfiguration(m_filterid, 0, KisGlobalResourcesInterface::instance());
    if (m_config) {
        config->fromXML(m_config->toXML());
    }

    for (int i = 0; i < nbValues(); ++i) {
        KisDelayedActionIntegerInput*  w = m_integerWidgets[i];
        if (w) {
            config->setProperty(w->objectName(), w->value());
        }
    }

    Q_FOREACH(KoAspectButton *aspectButton, m_aspectButtons) {
        config->setProperty(aspectButton->objectName(), aspectButton->keepAspectRatio());
    }

    return config;
}

qint32 KisMultiIntegerFilterWidget::nbValues() const {
    return m_integerWidgets.size();
}

qint32 KisMultiIntegerFilterWidget::valueAt(qint32 i) {
    if (i < m_integerWidgets.size()) {
        return m_integerWidgets[i]->value();
    }
    else {
        warnKrita << "Trying to access integer widget" << i << "but there are only" << m_integerWidgets.size() << "widgets";
        return 0;
    }
}


