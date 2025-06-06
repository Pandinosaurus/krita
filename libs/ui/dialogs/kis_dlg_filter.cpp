/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdysa.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_dlg_filter.h"



#include <KoResourcePaths.h>
#include <filter/kis_filter.h>
#include <filter/kis_filter_configuration.h>
#include <kis_filter_mask.h>
#include <kis_node.h>
#include <kis_layer.h>
#include <kis_paint_layer.h>
#include <KisViewManager.h>
#include <kis_config.h>

#include "kis_selection.h"
#include "kis_node_commands_adapter.h"
#include "kis_filter_manager.h"
#include "ui_wdgfilterdialog.h"
#include "kis_canvas2.h"
#include "kis_signal_compressor.h"
#include <kis_icon_utils.h>

#include <kstandardguiitem.h>

struct KisDlgFilter::Private {
    Private(KisFilterManager *_filterManager, KisViewManager *_view)
            : currentFilter(0)
            , resizeCount(0)
            , view(_view)
            , filterManager(_filterManager)
            , blockModifyingActionsGuard(new KisInputActionGroupsMaskGuard(view->canvasBase()->inputActionGroupsMaskInterface(), ViewTransformActionGroup))
            , updateCompressor(200, KisSignalCompressor::FIRST_ACTIVE)
    {
        updateCompressor.setDelay(
            [this] () {
                return filterManager->isIdle();
            },
            20, 200);
    }

    KisFilterSP currentFilter;
    Ui_FilterDialog uiFilterDialog;
    KisNodeSP node;
    int resizeCount;
    KisViewManager *view;
    KisFilterManager *filterManager;

    // a special guard object that blocks all the painting input actions while the
    // dialog is open
    QScopedPointer<KisInputActionGroupsMaskGuard> blockModifyingActionsGuard;
    KisSignalCompressor updateCompressor;
};

KisDlgFilter::KisDlgFilter(KisViewManager *view, KisNodeSP node, KisFilterManager *filterManager, QWidget *parent) :
        QDialog(parent),
        d(new Private(filterManager, view))
{
    setModal(false);

    d->uiFilterDialog.setupUi(this);
    d->node = node;

    d->uiFilterDialog.filterSelection->setView(view);
    d->uiFilterDialog.filterSelection->showFilterGallery(KisConfig(true).showFilterGallery());

    d->uiFilterDialog.pushButtonCreateMaskEffect->show();
    connect(d->uiFilterDialog.pushButtonCreateMaskEffect, SIGNAL(pressed()), SLOT(createMask()));

    d->uiFilterDialog.filterGalleryToggle->setChecked(d->uiFilterDialog.filterSelection->isFilterGalleryVisible());
    d->uiFilterDialog.filterGalleryToggle->setIcon(KisIconUtils::loadIcon("sidebaricon"));
    d->uiFilterDialog.filterGalleryToggle->setMaximumWidth(d->uiFilterDialog.filterGalleryToggle->height());
    connect(d->uiFilterDialog.filterSelection, SIGNAL(sigFilterGalleryToggled(bool)), d->uiFilterDialog.filterGalleryToggle, SLOT(setChecked(bool)));
    connect(d->uiFilterDialog.filterGalleryToggle, SIGNAL(toggled(bool)), d->uiFilterDialog.filterSelection, SLOT(showFilterGallery(bool)));
    connect(d->uiFilterDialog.filterSelection, SIGNAL(sigSizeChanged()), this, SLOT(slotFilterWidgetSizeChanged()));

    if (node->inherits("KisMask")) {
        d->uiFilterDialog.pushButtonCreateMaskEffect->setVisible(false);
    }

    d->uiFilterDialog.filterSelection->setPaintDevice(true, d->node->paintDevice());

    KGuiItem::assign(d->uiFilterDialog.buttonBox->button(QDialogButtonBox::Ok), KStandardGuiItem::ok());
    KGuiItem::assign(d->uiFilterDialog.buttonBox->button(QDialogButtonBox::Cancel), KStandardGuiItem::cancel());

    connect(d->uiFilterDialog.buttonBox, SIGNAL(accepted()), SLOT(accept()));
    connect(d->uiFilterDialog.buttonBox, SIGNAL(rejected()), SLOT(reject()));
    connect(d->uiFilterDialog.checkBoxPreview, SIGNAL(toggled(bool)), SLOT(enablePreviewToggled(bool)));
    connect(d->uiFilterDialog.filterSelection, SIGNAL(configurationChanged()), SLOT(filterSelectionChanged()));

    connect(this, SIGNAL(accepted()), SLOT(slotOnAccept()));
    connect(this, SIGNAL(accepted()), d->uiFilterDialog.filterSelection, SLOT(slotBookMarkCurrentFilter()));
    connect(this, SIGNAL(rejected()), SLOT(slotOnReject()));

    KConfigGroup group( KSharedConfig::openConfig(), "filterdialog");
    d->uiFilterDialog.checkBoxPreview->setChecked(group.readEntry("showPreview", true));

    d->uiFilterDialog.chkFilterSelectedFrames->setChecked(d->filterManager->filterAllSelectedFrames());

    //Handle create mask toggle based on state of chkFilterSelectedFrames
    connect(d->uiFilterDialog.chkFilterSelectedFrames, &QCheckBox::toggled, [this](const bool state){
        if (d->currentFilter) {
            d->uiFilterDialog.pushButtonCreateMaskEffect->setEnabled(!state && d->currentFilter->supportsAdjustmentLayers());
        }
    });

    d->uiFilterDialog.chkFilterSelectedFrames->setToolTip(i18n("In addition to filtering the currently visible frame, \nfilter all other keyframe selected in the Animation Timeline docker."));

    restoreGeometry(KisConfig(true).readEntry("filterdialog/geometry", QByteArray()));
    connect(&d->updateCompressor, SIGNAL(timeout()), this, SLOT(updatePreview()));

}

KisDlgFilter::~KisDlgFilter()
{
    KisConfig(false).writeEntry("filterdialog/geometry", saveGeometry());
    delete d;
}

void KisDlgFilter::setFilter(KisFilterSP f, KisFilterConfigurationSP overrideDefaultConfig)
{
    Q_ASSERT(f);
    setDialogTitle(f);
    d->uiFilterDialog.filterSelection->setFilter(f, overrideDefaultConfig);
    const bool multiframeEnabled = d->uiFilterDialog.chkFilterSelectedFrames->isChecked();
    d->uiFilterDialog.pushButtonCreateMaskEffect->setEnabled(f->supportsAdjustmentLayers() && !multiframeEnabled);
    d->currentFilter = f;
    d->updateCompressor.start();
}

void KisDlgFilter::setDialogTitle(KisFilterSP filter)
{
    setWindowTitle(filter.isNull() ? i18nc("@title:window", "Filter") : i18nc("@title:window", "Filter: %1", filter->name()));
}

void KisDlgFilter::startApplyingFilter(KisFilterConfigurationSP config)
{
    if (!d->uiFilterDialog.filterSelection->configuration()) return;

    if (d->node->inherits("KisPaintLayer")) {
        config->setChannelFlags(qobject_cast<KisPaintLayer*>(d->node.data())->channelLockFlags());
    }

    d->filterManager->apply(config);
}

void KisDlgFilter::updatePreview()
{
    KisFilterConfigurationSP config = d->uiFilterDialog.filterSelection->configuration();
    if (!config) return;

    bool maskCreationAllowed = !d->currentFilter || d->currentFilter->configurationAllowedForMask(config);
    const bool multiframeEnabled = d->uiFilterDialog.chkFilterSelectedFrames->isChecked();
    d->uiFilterDialog.pushButtonCreateMaskEffect->setEnabled(maskCreationAllowed && !multiframeEnabled);

    if (d->uiFilterDialog.checkBoxPreview->isChecked()) {
        KisFilterConfigurationSP config(d->uiFilterDialog.filterSelection->configuration());
        startApplyingFilter(config);
    }

    d->uiFilterDialog.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
}

void KisDlgFilter::adjustSize()
{
    QWidget::adjustSize();
}

void KisDlgFilter::slotFilterWidgetSizeChanged()
{
    QMetaObject::invokeMethod(this, "adjustSize", Qt::QueuedConnection);
}

void KisDlgFilter::slotOnAccept()
{
    if (!d->filterManager->isStrokeRunning()) {
        KisFilterConfigurationSP config(d->uiFilterDialog.filterSelection->configuration());
        startApplyingFilter(config);
    }

    d->filterManager->setFilterAllSelectedFrames(d->uiFilterDialog.chkFilterSelectedFrames->isChecked());
    d->filterManager->finish();

    d->uiFilterDialog.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    KisConfig(false).setShowFilterGallery(d->uiFilterDialog.filterSelection->isFilterGalleryVisible());
}

void KisDlgFilter::slotOnReject()
{
    if (d->filterManager->isStrokeRunning()) {
        d->filterManager->cancelRunningStroke();
    }

    KisConfig(false).setShowFilterGallery(d->uiFilterDialog.filterSelection->isFilterGalleryVisible());
}

void KisDlgFilter::createMask()
{
    if (d->node->inherits("KisMask")) return;

    if (d->filterManager->isStrokeRunning()) {
        d->filterManager->cancelRunningStroke();
        if (!d->view->blockUntilOperationsFinished(d->view->image())) {
            updatePreview();
            return;
        }
    }

    KisLayer *layer = qobject_cast<KisLayer*>(d->node.data());
    KisFilterMaskSP mask = new KisFilterMask(d->view->image(), i18n("Filter Mask"));
    mask->setName(d->currentFilter->name());
    mask->initSelection(d->view->selection(), layer);
    mask->setFilter(d->uiFilterDialog.filterSelection->configuration()->cloneWithResourcesSnapshot());

    Q_ASSERT(layer->allowAsChild(mask));

    KisNodeCommandsAdapter adapter(d->view);
    adapter.addNode(mask, layer, layer->lastChild());

    close();
}

void KisDlgFilter::enablePreviewToggled(bool checked)
{
    if (checked) {
        d->updateCompressor.start();
    } else if (d->filterManager->isStrokeRunning()) {
        d->filterManager->cancelRunningStroke();
    }

    KConfigGroup group( KSharedConfig::openConfig(), "filterdialog");
    group.writeEntry("showPreview", checked);

    group.config()->sync();
}

void KisDlgFilter::filterSelectionChanged()
{
    KisFilterSP filter = d->uiFilterDialog.filterSelection->currentFilter();
    setDialogTitle(filter);
    d->currentFilter = filter;
    const bool multiframeEnabled = d->uiFilterDialog.chkFilterSelectedFrames->isChecked();
    d->uiFilterDialog.pushButtonCreateMaskEffect->setEnabled(filter.isNull() ? false : (filter->supportsAdjustmentLayers() && !multiframeEnabled));
    d->updateCompressor.start();
}
