/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006, 2010 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2011 Silvio Heinrich <plassy@web.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_CANVAS_H
#define KIS_CANVAS_H

#include <QObject>
#include <QWidget>
#include <QSize>
#include <QString>

#include <KoConfig.h>
#include <KoColorConversionTransformation.h>
#include <KoCanvasBase.h>
#include <kritaui_export.h>
#include <kis_types.h>
#include <KoPointerEvent.h>

#include "opengl/kis_opengl.h"

#include "kis_ui_types.h"
#include "kis_coordinates_converter.h"
#include "kis_canvas_decoration.h"
#include "kis_painting_assistants_decoration.h"
#include "input/KisInputActionGroup.h"
#include "KisReferenceImagesDecoration.h"
#include "KisWraparoundAxis.h"

class KoToolProxy;
class KisDisplayConfig;


class KisViewManager;
class KisFavoriteResourceManager;
class KisDisplayFilter;
class KisDisplayColorConverter;
struct KisExposureGammaCorrectionInterface;
class KisView;
class KisInputManager;
class KisCanvasAnimationState;
class KisShapeController;
class KisCoordinatesConverter;
class KoViewConverter;
class KisAbstractCanvasWidget;
class KisPopupPalette;


/**
 * KisCanvas2 is not an actual widget class, but rather an adapter for
 * the widget it contains, which may be either a QPainter based
 * canvas, or an OpenGL based canvas: that are the real widgets.
 */
class KRITAUI_EXPORT KisCanvas2 : public KoCanvasBase
{

    Q_OBJECT

public:

    /**
     * Create a new canvas. The canvas manages a widget that will do
     * the actual painting: the canvas itself is not a widget.
     *
     * @param viewConverter the viewconverter for converting between
     *                       window and document coordinates.
     */
    KisCanvas2(KisCoordinatesConverter *coordConverter, KoCanvasResourceProvider *resourceManager, KisMainWindow *mainWindow, KisView *view, KoShapeControllerBase *sc);

    ~KisCanvas2() override;

    void disconnectCanvasObserver(QObject *object) override;

public: // KoCanvasBase implementation

    bool canvasIsOpenGL() const override;

    KisOpenGL::FilterMode openGLFilterMode() const;

    void gridSize(QPointF *offset, QSizeF *spacing) const override;

    bool snapToGrid() const override;

    // This method only exists to support flake-related operations
    void addCommand(KUndo2Command *command) override;

    QPoint documentOrigin() const override;
    QPoint documentOffset() const;

    /**
     * Return the right shape manager for the current layer. That is
     * to say, if the current layer is a vector layer, return the shape
     * layer's canvas' shapemanager, else the shapemanager associated
     * with the global krita canvas.
     */
    KoShapeManager * shapeManager() const override;

    /**
     * Since shapeManager() may change, we need a persistent object where we can
     * connect to and thack the selection. See more comments in KoCanvasBase.
     */
    KoSelectedShapesProxy *selectedShapesProxy() const override;

    /**
     * Return the shape manager associated with this canvas
     */
    KoShapeManager *globalShapeManager() const;

    /**
     * Return shape manager associated with the currently active node.
     * If current node has no internal shape manager, return null.
     */
    KoShapeManager *localShapeManager() const;


    void updateCanvas(const QRectF& rc) override;

    const KisCoordinatesConverter* coordinatesConverter() const;
    const KoViewConverter *viewConverter() const override;
    KoViewConverter *viewConverter() override;

    QWidget* canvasWidget() override;

    const QWidget* canvasWidget() const override;

    KoUnit unit() const override;

    KoToolProxy* toolProxy() const override;

    // FIXME:
    // Temporary! Either get the current layer and image from the
    // resource provider, or use this, which gets them from the
    // current shape selection.
    KisImageWSP currentImage() const;

    /**
     * Filters events and sends them to canvas actions. Shared
     * among all the views/canvases
     *
     * NOTE: May be null while initialization!
     */
    KisInputManager* globalInputManager() const;

    KisPaintingAssistantsDecorationSP paintingAssistantsDecoration() const;
    KisReferenceImagesDecorationSP referenceImagesDecoration() const;

public: // KisCanvas2 methods

    KisImageWSP image() const;
    KisViewManager* viewManager() const;
    QPointer<KisView> imageView() const;

    /// @return true if the canvas image should be displayed in vertically mirrored mode
    void addDecoration(KisCanvasDecorationSP deco);
    KisCanvasDecorationSP decoration(const QString& id) const;

    void setDisplayFilter(QSharedPointer<KisDisplayFilter> displayFilter);
    QSharedPointer<KisDisplayFilter> displayFilter() const;

    KisDisplayColorConverter *displayColorConverter() const;
    KisExposureGammaCorrectionInterface* exposureGammaCorrectionInterface() const;

    /**
     * @brief fetchProofingOptions
     * Get the options for softproofing, and apply the view-specific state without affecting
     * the proofing options as stored inside the image.
     */
    void fetchProofingOptions();
    void updateProofingState();
    KisProofingConfigurationSP proofingConfiguration() const;

    /**
     * @brief setProofingConfigUpdated This function is to set whether the proofing config is updated,
     * this is needed for determining whether or not to generate a new proofing transform.
     * @param updated whether it's updated. Just set it to false in normal usage.
     */
    void setProofingConfigUpdated(bool updated);

    /**
     * @brief proofingConfigUpdated ask the canvas whether or not it updated the proofing config.
     * @return whether or not the proofing config is updated, if so, a new proofing transform needs to be made
     * in KisOpenGL canvas.
     */
    bool proofingConfigUpdated();

    void setCursor(const QCursor &cursor) override;
    KisAnimationFrameCacheSP frameCache() const;
    KisCanvasAnimationState *animationState() const;
    void refetchDataFromImage();

    /**
     * @return area of the image (in image coordinates) that is visible on the canvas
     * with a small margin selected by the user
     */
    QRect regionOfInterest() const;

    /**
     * Set artificial limit outside which the image will not be rendered
     * \p rc is measured in image pixels
     */
    void setRenderingLimit(const QRect &rc);

    /**
     * @return artificial limit outside which the image will not be rendered
     */
    QRect renderingLimit() const;

    KisPopupPalette* popupPalette();

    /**
     * @return a reference to alter this canvas' input action groups mask
     */
    KisInputActionGroupsMaskInterface::SharedInterface inputActionGroupsMaskInterface();
Q_SIGNALS:
    void sigCanvasEngineChanged();

    void sigCanvasCacheUpdated();
    void sigContinueResizeImage(qint32 w, qint32 h);

    void sigCanvasStateChanged();

    // emitted whenever the canvas widget thinks sketch should update
    void updateCanvasRequested(const QRect &rc);

    void sigRegionOfInterestChanged(const QRect &roi);

public Q_SLOTS:

    /// Update the entire canvas area
    void updateCanvas();

    void updateCanvasProjection(const QRectF &docRect);
    void updateCanvasDecorations();
    void updateCanvasDecorations(const QRectF &docRect);
    void updateCanvasToolOutlineDoc(const QRectF &docRect);
    void updateCanvasToolOutlineWdg(const QRect &widgetRect);
    void updateCanvasScene();

    void startResizingImage();
    void finishResizingImage(qint32 w, qint32 h);

    /// canvas rotation in degrees
    qreal rotationAngle() const;
    /// Bools indicating canvasmirroring.
    bool xAxisMirrored() const;
    bool yAxisMirrored() const;
    void slotSoftProofing();
    void slotGamutCheck();
    void slotChangeProofingConfig();
    void slotPopupPaletteRequestedZoomChange(int zoom);

    void channelSelectionChanged();

    void startUpdateInPatches(const QRect &imageRect);

    void slotTrySwitchShapeManager();

    /**
     * Called whenever the configuration settings change.
     */
    void slotConfigChanged();

    void slotScreenChanged(QScreen *screen);


private Q_SLOTS:

    /// The image projection has changed, now start an update
    /// of the canvas representation.
    void startUpdateCanvasProjection(const QRect & rc);
    void updateCanvasProjection();

    void slotBeginUpdatesBatch();
    void slotEndUpdatesBatch();
    void slotSetLodUpdatesBlocked(bool value);

    void slotEffectiveZoomChanged(qreal newZoom);
    void slotCanvasStateChanged();

    void viewportOffsetMoved(const QPointF &oldOffset, const QPointF &newOffset);

    void slotSelectionChanged();

    void slotDoCanvasUpdate();

    void bootstrapFinished();

    void slotUpdateRegionOfInterest();
    void slotUpdateReferencesBounds();

    void slotImageColorSpaceChanged();

public:
    // interface for KisCanvasController only
    void setWrapAroundViewingMode(bool value);
    bool wrapAroundViewingMode() const;

    void setWrapAroundViewingModeAxis(WrapAroundAxis value);
    WrapAroundAxis wrapAroundViewingModeAxis() const;

    void setLodPreferredInCanvas(bool value);
    bool lodPreferredInCanvas() const;

    void initializeImage();
    void disconnectImage();

    void setFavoriteResourceManager(KisFavoriteResourceManager* favoriteResourceManager);

private:
    Q_DISABLE_COPY(KisCanvas2)

    void requestCanvasUpdateMaybeCompressed();
    void connectCurrentCanvas();
    void createCanvas(bool useOpenGL);
    void createQPainterCanvas();
    void createOpenGLCanvas();
    void updateCanvasWidgetImpl(const QRect &rc = QRect());
    void setCanvasWidget(KisAbstractCanvasWidget *widget);
    void resetCanvas(bool useOpenGL);
    void setDisplayConfig(const KisDisplayConfig &config);

    void notifyLevelOfDetailChange();

    // Completes construction of canvas.
    // To be called by KisView in its constructor, once it has been setup enough
    // (to be defined what that means) for things KisCanvas2 expects from KisView
    // TODO: see to avoid that
    void setup();

    void initializeFpsDecoration();

private:
    friend class KisView; // calls setup()
    class KisCanvas2Private;
    KisCanvas2Private * const m_d;
};

#endif
