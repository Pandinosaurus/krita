/*
 *  kis_tool_rectangle.cc - part of Krita
 *
 *  SPDX-FileCopyrightText: 2000 John Califf <jcaliff@compuzone.net>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2004 Clarence Dang <dang@k.org>
 *  SPDX-FileCopyrightText: 2009 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_rectangle.h"

#include <kis_debug.h>
#include <brushengine/kis_paintop_registry.h>
#include "KoCanvasBase.h"

#include <KisViewManager.h>
#include <canvas/kis_canvas2.h>
#include <kis_canvas_resource_provider.h>
#include "kis_shape_tool_helper.h"
#include "kis_figure_painting_tool_helper.h"

#include <KoCanvasController.h>
#include <KoShapeStroke.h>


KisToolRectangle::KisToolRectangle(KoCanvasBase * canvas)
        : KisToolRectangleBase(canvas, KisToolRectangleBase::PAINT, KisCursor::load("tool_rectangle_cursor.png", 6, 6))
{
    setSupportOutline(true);
    setObjectName("tool_rectangle");
    setIsOpacityPresetMode(true);

    KisCanvas2 *kritaCanvas = dynamic_cast<KisCanvas2*>(canvas);

    connect(kritaCanvas->viewManager()->canvasResourceProvider(), SIGNAL(sigEffectiveCompositeOpChanged()), SLOT(resetCursorStyle()));
}

KisToolRectangle::~KisToolRectangle()
{
}

void KisToolRectangle::resetCursorStyle()
{
    if (isEraser() && (nodePaintAbility() == NodePaintAbility::PAINT)) {
        useCursor(KisCursor::load("tool_rectangle_eraser_cursor.png", 6, 6));
    } else {
        KisToolRectangleBase::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

void KisToolRectangle::finishRect(const QRectF &rect, qreal roundCornersX, qreal roundCornersY)
{
    if (rect.isNull())
        return;

    const KisToolShape::ShapeAddInfo info =
        shouldAddShape(currentNode());

    if (!info.shouldAddShape) {
        KisFigurePaintingToolHelper helper(kundo2_i18n("Draw Rectangle"),
                                           image(),
                                           currentNode(),
                                           canvas()->resourceManager(),
                                           strokeStyle(),
                                           fillStyle(),
                                           fillTransform());

        QPainterPath path;

        if (roundCornersX > 0 || roundCornersY > 0) {
            path.addRoundedRect(rect, roundCornersX, roundCornersY);
        } else {
            path.addRect(rect);
        }
        getRotatedPath(path, rect.center(), getRotationAngle());
        helper.paintPainterPath(path);
    } else {
        const QRectF r = convertToPt(rect);
        const qreal docRoundCornersX = convertToPt(roundCornersX);
        const qreal docRoundCornersY = convertToPt(roundCornersY);
        KoShape* shape = KisShapeToolHelper::createRectangleShape(r, docRoundCornersX, docRoundCornersY);
        shape->rotate(qRadiansToDegrees(getRotationAngle()));

        KoShapeStrokeSP border;
        if (strokeStyle() != KisToolShapeUtils::StrokeStyleNone) {
            const QColor color = strokeStyle() == KisToolShapeUtils::StrokeStyleForeground ?
                        canvas()->resourceManager()->foregroundColor().toQColor() :
                        canvas()->resourceManager()->backgroundColor().toQColor();

            border = toQShared(new KoShapeStroke(currentStrokeWidth(), color));
        }
        shape->setStroke(border);

        info.markAsSelectionShapeIfNeeded(shape);

        addShape(shape);
    }
}

bool KisToolRectangle::supportsPaintingAssistants() const
{
    return true;
}
