/*
 *  SPDX-FileCopyrightText: 2008-2012 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_spray_paintop.h"
#include "kis_spray_paintop_settings.h"

#include <cmath>

#include <QRect>
#include <kis_global.h>
#include <kis_paint_device.h>
#include <kis_painter.h>
#include <kis_types.h>
#include <brushengine/kis_paintop.h>
#include <kis_node.h>

#include <kis_fixed_paint_device.h>
#include <kis_brush_option.h>
#include <kis_lod_transform.h>
#include <kis_paintop_plugin_utils.h>
#include <KoResourceLoadResult.h>


KisSprayPaintOp::KisSprayPaintOp(const KisPaintOpSettingsSP settings, KisPainter *painter, KisNodeSP node, KisImageSP image)
    : KisPaintOp(painter)
    , m_sprayOpOption(settings.data())
    , m_isPresetValid(true)
    , m_rotationOption(settings.data())
    , m_sizeOption(settings.data())
    , m_opacityOption(settings.data(), node)
    , m_rateOption(settings.data())
    , m_node(node)
    
{
    Q_ASSERT(settings);
    Q_ASSERT(painter);
    Q_UNUSED(image);

    m_airbrushData.read(settings.data());

    m_brushOption.readOptionSetting(settings, settings->resourcesInterface(), settings->canvasResourcesInterface());

    m_colorProperties.read(settings.data());
    // create the actual distribution objects
    m_sprayOpOption.updateDistributions();
    // first load tip properties as shape properties are dependent on diameter/scale/aspect
    m_shapeProperties.read(settings.data());

    // TODO: what to do with proportional sizes?
    m_shapeDynamicsProperties.read(settings.data());

    if (!m_shapeProperties.enabled && !m_brushOption.brush()) {
        // in case the preset does not contain the definition for KisBrush
        m_isPresetValid = false;
        dbgKrita << "Preset is not valid. Painting is not possible. Use the preset editor to fix current brush engine preset.";
    }

    m_sprayBrush.setProperties(&m_sprayOpOption.data, &m_colorProperties,
                               &m_shapeProperties, &m_shapeDynamicsProperties, m_brushOption.brush());

    m_sprayBrush.setFixedDab(cachedDab());

    // spacing
    if ((m_sprayOpOption.data.diameter * 0.5) > 1) {
        m_ySpacing = m_xSpacing = m_sprayOpOption.data.diameter * 0.5 * m_sprayOpOption.data.spacing;
    }
    else {
        m_ySpacing = m_xSpacing = 1.0;
    }
    m_spacing = m_xSpacing;
}

KisSprayPaintOp::~KisSprayPaintOp()
{
}

QList<KoResourceLoadResult> KisSprayPaintOp::prepareLinkedResources(const KisPaintOpSettingsSP settings, KisResourcesInterfaceSP resourcesInterface)
{
    KisBrushOptionProperties brushOption;
    return brushOption.prepareLinkedResources(settings, resourcesInterface);
}

KisSpacingInformation KisSprayPaintOp::paintAt(const KisPaintInformation& info)
{
    if (!painter() || !m_isPresetValid) {
        return KisSpacingInformation(m_spacing);
    }

    if (!m_dab) {
        m_dab = source()->createCompositionSourceDevice();
    }
    else {
        m_dab->clear();
    }

    qreal rotation = m_rotationOption.apply(info);
    m_opacityOption.apply(painter(), info);
    // Spray Brush is capable of working with zero scale,
    // so no additional checks for 'zero'ness are needed
    const qreal scale = m_sizeOption.apply(info);
    const qreal lodScale = KisLodTransform::lodToScale(painter()->device());


    m_sprayBrush.paint(m_dab,
                       m_node->paintDevice(),
                       info,
                       rotation,
                       scale, lodScale,
                       painter()->paintColor(),
                       painter()->backgroundColor());

    QRect rc = m_dab->extent();
    painter()->bitBlt(rc.topLeft(), m_dab, rc);
    painter()->renderMirrorMask(rc, m_dab);

    return computeSpacing(info, lodScale);
}

KisSpacingInformation KisSprayPaintOp::updateSpacingImpl(const KisPaintInformation &info) const
{
    return computeSpacing(info, KisLodTransform::lodToScale(painter()->device()));
}

KisTimingInformation KisSprayPaintOp::updateTimingImpl(const KisPaintInformation &info) const
{
    return KisPaintOpPluginUtils::effectiveTiming(&m_airbrushData, &m_rateOption, info);
}

KisSpacingInformation KisSprayPaintOp::computeSpacing(const KisPaintInformation &info,
                                                      qreal lodScale) const
{
    return KisPaintOpPluginUtils::effectiveSpacing(1.0, 1.0, true, 0.0, false,
                                                   m_spacing * lodScale, false, 1.0, lodScale,
                                                   &m_airbrushData, nullptr, info);
}
