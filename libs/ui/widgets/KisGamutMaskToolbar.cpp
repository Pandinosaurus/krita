/*
 *  SPDX-FileCopyrightText: 2018 Anna Medonosova <anna.medonosova@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QWidget>
#include "KisGamutMaskToolbar.h"
#include <kis_icon_utils.h>
#include <kis_canvas_resource_provider.h>
#include <kis_signals_blocker.h>

KisGamutMaskToolbar::KisGamutMaskToolbar(QWidget* parent) : QWidget(parent)
  , m_selectedMask(nullptr)
  , m_selfUpdate(false)
{
    m_ui.reset(new Ui_wdgGamutMaskToolbar());
    m_ui->setupUi(this);

    m_iconMaskOff = KisIconUtils::loadIcon("gamut-mask-off");
    m_iconMaskOn = KisIconUtils::loadIcon("gamut-mask-on");

    m_textNoMask = i18n("Select a mask in \"Gamut Masks\" docker");
    m_textMaskDisabled = i18n("Mask is disabled");

    m_ui->bnToggleMask->setChecked(false);
    m_ui->bnToggleMask->setIcon(m_iconMaskOn);
    m_ui->bnToggleMask->setEnabled(false);

    m_ui->rotationAngleSelector->setDecimals(0);
    m_ui->rotationAngleSelector->setIncreasingDirection(KisAngleGauge::IncreasingDirection_Clockwise);
    m_ui->rotationAngleSelector->hide();

    // gamut mask connections
    connect(m_ui->bnToggleMask, SIGNAL(toggled(bool)), SLOT(slotGamutMaskToggle(bool)));
    connect(m_ui->rotationAngleSelector, SIGNAL(angleChanged(qreal)), SLOT(slotGamutMaskRotate(qreal)));
}

void KisGamutMaskToolbar::connectMaskSignals(KisCanvasResourceProvider* resourceProvider)
{
    connect(resourceProvider, SIGNAL(sigGamutMaskChanged(KoGamutMaskSP)),
            this, SLOT(slotGamutMaskSet(KoGamutMaskSP)), Qt::UniqueConnection);

    connect(resourceProvider, SIGNAL(sigGamutMaskUnset()),
            this, SLOT(slotGamutMaskUnset()), Qt::UniqueConnection);

    connect(this, SIGNAL(sigGamutMaskChanged(KoGamutMaskSP)),
            resourceProvider, SLOT(slotGamutMaskActivated(KoGamutMaskSP)), Qt::UniqueConnection);

    connect(this, SIGNAL(sigGamutMaskDeactivated()),
            resourceProvider, SLOT(slotGamutMaskDeactivate()), Qt::UniqueConnection);

    connect(resourceProvider, SIGNAL(sigGamutMaskDeactivated()),
            this, SLOT(slotGamutMaskDeactivate()), Qt::UniqueConnection);

}

void KisGamutMaskToolbar::slotGamutMaskSet(KoGamutMaskSP mask)
{
    if (!mask) {
        return;
    }

    if (m_selfUpdate) {
        return;
    }

    m_selectedMask = mask;

    if (m_selectedMask) {
        updateMaskState(true, false);
    } else {
        updateMaskState(false, false);
    }
}

void KisGamutMaskToolbar::slotGamutMaskUnset()
{
    m_selectedMask = nullptr;
    m_ui->rotationAngleSelector->hide();
    m_ui->labelMaskName->show();
    m_ui->labelMaskName->setText(m_textNoMask);
    m_ui->bnToggleMask->setIcon(m_iconMaskOn);
    m_ui->bnToggleMask->setEnabled(false);
}

void KisGamutMaskToolbar::slotGamutMaskDeactivate()
{
    if (m_selfUpdate) {
        return;
    }

    updateMaskState(false, false);
}

void KisGamutMaskToolbar::slotGamutMaskToggle(bool state)
{
    updateMaskState(state, true);
}

void KisGamutMaskToolbar::slotGamutMaskRotate(qreal angle)
{
    if (!m_selectedMask) {
        return;
    }

    m_selectedMask->setRotation(angle);
    m_selfUpdate = true;
    Q_EMIT sigGamutMaskChanged(m_selectedMask);
    m_selfUpdate = false;
}

void KisGamutMaskToolbar::updateMaskState(bool maskEnabled, bool internalChange)
{
    bool enabled = (m_selectedMask) ? maskEnabled : false;

    m_ui->bnToggleMask->setChecked(enabled);

    if (enabled) {
        m_ui->bnToggleMask->setEnabled(true);
        m_ui->bnToggleMask->setIcon(m_iconMaskOn);
        m_ui->labelMaskName->hide();
        m_ui->rotationAngleSelector->show();

        m_ui->rotationAngleSelector->blockSignals(true);
        m_ui->rotationAngleSelector->setAngle(static_cast<qreal>(m_selectedMask->rotation()));
        m_ui->rotationAngleSelector->blockSignals(false);

        if (internalChange) {
            m_selfUpdate = true;
            Q_EMIT sigGamutMaskChanged(m_selectedMask);
            m_selfUpdate = false;
        }

    } else {
        m_ui->bnToggleMask->setIcon(m_iconMaskOff);
        m_ui->rotationAngleSelector->hide();
        m_ui->labelMaskName->show();
        m_ui->labelMaskName->setText(m_textMaskDisabled);

        if (internalChange) {
            m_selfUpdate = true;
            Q_EMIT sigGamutMaskDeactivated();
            m_selfUpdate = false;
        }
    }
}
