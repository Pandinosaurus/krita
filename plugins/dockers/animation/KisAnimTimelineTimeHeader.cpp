/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2021 Eoin O'Neil <eoinoneill1991@gmail.com>
 *  SPDX-FileCopyrightText: 2021 Emmet O'Neill <emmetoneill.pdx@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisAnimTimelineTimeHeader.h"

#include <limits>

#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QPaintEvent>
#include <KisPlaybackEngine.h>

#include <klocalizedstring.h>

#include "KisTimeBasedItemModel.h"
#include "KisAnimTimelineColors.h"
#include "kis_action.h"
#include "kis_signal_compressor_with_param.h"
#include "kis_config.h"

#include "kis_debug.h"

struct KisAnimTimelineTimeHeader::Private
{
    Private()
        : model(nullptr)
        , actionMan(nullptr)
        , fps(12)
        , lastPressSectionIndex(-1)
    {
        // Compressed configuration writing..
        const int compressorDelayMS = 100;
        zoomSaveCompressor.reset(
                    new KisSignalCompressorWithParam<qreal>(compressorDelayMS,
                                                            [](qreal zoomValue){
                                                                KisConfig cfg(false);
                                                                cfg.setTimelineZoom(zoomValue);
                                                            },
                                                            KisSignalCompressor::POSTPONE)
                );
    }

    KisTimeBasedItemModel* model;
    KisActionManager* actionMan;
    QScopedPointer<KisSignalCompressorWithParam<qreal>> zoomSaveCompressor;

    int fps;
    int lastPressSectionIndex;

    qreal offset = 0.0f;
    const int minSectionSize = 4;
    const int maxSectionSize = 72;
    const int unitSectionSize = 18;
    qreal remainder = 0.0f;

    int calcSpanWidth(const int sectionWidth);
    QModelIndexList prepareFramesSlab(int startCol, int endCol);
};

KisAnimTimelineTimeHeader::KisAnimTimelineTimeHeader(QWidget *parent)
    : QHeaderView(Qt::Horizontal, parent)
    , m_d(new Private)
{
    setSectionResizeMode(QHeaderView::Fixed);
    setDefaultSectionSize(18);
    setMinimumSectionSize(8);
}

KisAnimTimelineTimeHeader::~KisAnimTimelineTimeHeader()
{
}

void KisAnimTimelineTimeHeader::setPixelOffset(qreal offset)
{
    m_d->offset = qMax(offset, qreal(0.f));
    setOffset(m_d->offset);
    viewport()->update();
}

void KisAnimTimelineTimeHeader::setActionManager(KisActionManager *actionManager)
{
    m_d->actionMan = actionManager;

    disconnect(this, &KisAnimTimelineTimeHeader::sigZoomChanged, this, &KisAnimTimelineTimeHeader::slotSaveThrottle);

    if (actionManager) {
        KisAction *action;

        action = actionManager->createAction("insert_column_left");
        connect(action, SIGNAL(triggered()), SIGNAL(sigInsertColumnLeft()));

        action = actionManager->createAction("insert_column_right");
        connect(action, SIGNAL(triggered()), SIGNAL(sigInsertColumnRight()));

        action = actionManager->createAction("insert_multiple_columns");
        connect(action, SIGNAL(triggered()), SIGNAL(sigInsertMultipleColumns()));

        action = actionManager->createAction("remove_columns_and_pull");
        connect(action, SIGNAL(triggered()), SIGNAL(sigRemoveColumnsAndShift()));

        action = actionManager->createAction("remove_columns");
        connect(action, SIGNAL(triggered()), SIGNAL(sigRemoveColumns()));

        action = actionManager->createAction("insert_hold_column");
        connect(action, SIGNAL(triggered()), SIGNAL(sigInsertHoldColumns()));

        action = actionManager->createAction("insert_multiple_hold_columns");
        connect(action, SIGNAL(triggered()), SIGNAL(sigInsertHoldColumnsCustom()));

        action = actionManager->createAction("remove_hold_column");
        connect(action, SIGNAL(triggered()), SIGNAL(sigRemoveHoldColumns()));

        action = actionManager->createAction("remove_multiple_hold_columns");
        connect(action, SIGNAL(triggered()), SIGNAL(sigRemoveHoldColumnsCustom()));

        action = actionManager->createAction("mirror_columns");
        connect(action, SIGNAL(triggered()), SIGNAL(sigMirrorColumns()));

        action = actionManager->createAction("clear_animation_cache");
        connect(action, SIGNAL(triggered()), SIGNAL(sigClearCache()));

        action = actionManager->createAction("copy_columns_to_clipboard");
        connect(action, SIGNAL(triggered()), SIGNAL(sigCopyColumns()));

        action = actionManager->createAction("cut_columns_to_clipboard");
        connect(action, SIGNAL(triggered()), SIGNAL(sigCutColumns()));

        action = actionManager->createAction("paste_columns_from_clipboard");
        connect(action, SIGNAL(triggered()), SIGNAL(sigPasteColumns()));

        KisConfig cfg(true);
        setZoom(cfg.timelineZoom());
        connect(this, &KisAnimTimelineTimeHeader::sigZoomChanged, this, &KisAnimTimelineTimeHeader::slotSaveThrottle);
    }
}


void KisAnimTimelineTimeHeader::paintEvent(QPaintEvent *e)
{
    QHeaderView::paintEvent(e);

    // Copied from Qt 4.8...

    if (count() == 0)
        return;

    QPainter painter(viewport());
    const QPoint offset = dirtyRegionOffset();
    QRect translatedEventRect = e->rect();
    translatedEventRect.translate(offset);

    int start = -1;
    int end = -1;
    if (orientation() == Qt::Horizontal) {
        start = visualIndexAt(translatedEventRect.left());
        end = visualIndexAt(translatedEventRect.right());
    } else {
        start = visualIndexAt(translatedEventRect.top());
        end = visualIndexAt(translatedEventRect.bottom());
    }

    const bool reverseImpl = orientation() == Qt::Horizontal && isRightToLeft();

    if (reverseImpl) {
        start = (start == -1 ? count() - 1 : start);
        end = (end == -1 ? 0 : end);
    } else {
        start = (start == -1 ? 0 : start);
        end = (end == -1 ? count() - 1 : end);
    }

    int tmp = start;
    start = qMin(start, end);
    end = qMax(tmp, end);

    ///////////////////////////////////////////////////
    /// Krita specific code. We should update in spans!

    const int spanStart = start - start % m_d->fps;
    const int spanEnd = end - end % m_d->fps + m_d->fps - 1;

    start = spanStart;
    end = qMin(count() - 1, spanEnd);

    /// End of Krita specific code
    ///////////////////////////////////////////////////

    QRect currentSectionRect;
    int logical;
    const int width = viewport()->width();
    const int height = viewport()->height();

    for (int i = start; i <= end; ++i) {
        // DK: cannot copy-paste easily...
        // if (d->isVisualIndexHidden(i))
        //     continue;
        painter.save();
        logical = logicalIndex(i);
        if (orientation() == Qt::Horizontal) {
            currentSectionRect.setRect(sectionViewportPosition(logical), 0, sectionSize(logical), height);
        } else {
            currentSectionRect.setRect(0, sectionViewportPosition(logical), width, sectionSize(logical));
        }
        currentSectionRect.translate(offset);

        QVariant variant = model()->headerData(logical, orientation(),
                                                Qt::FontRole);
        if (variant.isValid() && variant.canConvert<QFont>()) {
            QFont sectionFont = qvariant_cast<QFont>(variant);
            painter.setFont(sectionFont);
        }
        paintSection1(&painter, currentSectionRect, logical);
        painter.restore();
    }
}

void KisAnimTimelineTimeHeader::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    // Base paint event should paint nothing in the sections area

    Q_UNUSED(painter);
    Q_UNUSED(rect);
    Q_UNUSED(logicalIndex);
}

void KisAnimTimelineTimeHeader::paintSpan(QPainter *painter, int userFrameId,
                                    const QRect &spanRect,
                                    bool isIntegralLine,
                                    bool isPrevIntegralLine,
                                    QStyle *style,
                                    const QPalette &palette,
                                    const QPen &gridPen) const
{
    painter->fillRect(spanRect, palette.brush(QPalette::Button));

    int safeRight = spanRect.right();

    QPen oldPen = painter->pen();
    painter->setPen(gridPen);

    int adjustedTop = spanRect.top() + (!isIntegralLine ? spanRect.height() / 2 : 0);
    painter->drawLine(safeRight, adjustedTop, safeRight, spanRect.bottom());

    if (isPrevIntegralLine) {
        painter->drawLine(spanRect.left() + 1, spanRect.top(), spanRect.left() + 1, spanRect.bottom());
    }

    painter->setPen(oldPen);

    QString frameIdText = QString::number(userFrameId);
    QRect textRect(spanRect.topLeft() + QPoint(2, 0), QSize(spanRect.width() - 2, spanRect.height()));

    QStyleOptionHeader opt;
    initStyleOption(&opt);

    QStyle::State state = QStyle::State_None;
    if (isEnabled())
        state |= QStyle::State_Enabled;
    if (window()->isActiveWindow())
        state |= QStyle::State_Active;
    opt.state |= state;
    opt.selectedPosition = QStyleOptionHeader::NotAdjacent;

    opt.textAlignment = Qt::AlignLeft | Qt::AlignTop;
    opt.rect = textRect;
    opt.text = frameIdText;
    style->drawControl(QStyle::CE_HeaderLabel, &opt, painter, this);
}

void KisAnimTimelineTimeHeader::slotSaveThrottle(qreal value)
{
    m_d->zoomSaveCompressor->start(value);
}

int KisAnimTimelineTimeHeader::Private::calcSpanWidth(const int sectionWidth) {
    const int minWidth = 36;

    int spanWidth = this->fps;

    while (spanWidth * sectionWidth < minWidth) {
        spanWidth *= 2;
    }

    bool splitHappened = false;

    do {
        splitHappened = false;

        if (!(spanWidth & 0x1) &&
            spanWidth * sectionWidth / 2 > minWidth) {

            spanWidth /= 2;
            splitHappened = true;

        } else if (!(spanWidth % 3) &&
                   spanWidth * sectionWidth / 3 > minWidth) {

            spanWidth /= 3;
            splitHappened = true;

        } else if (!(spanWidth % 5) &&
                   spanWidth * sectionWidth / 5 > minWidth) {

            spanWidth /= 5;
            splitHappened = true;
        }

    } while (splitHappened);


    if (sectionWidth > minWidth) {
        spanWidth = 1;
    }

    return spanWidth;
}

void KisAnimTimelineTimeHeader::paintSection1(QPainter *painter, const QRect &rect, int logicalIndex) const
{

    if (!rect.isValid())
        return;

    QFontMetrics metrics(this->font());
    const int textHeight = metrics.height();

    QPoint p1 = rect.topLeft() + QPoint(0, textHeight);
    QPoint p2 = rect.topRight() + QPoint(0, textHeight);

    QRect frameRect = QRect(p1, QSize(rect.width(), rect.height() - textHeight));

    const int width = rect.width();

    int spanWidth = m_d->calcSpanWidth(width);

    const int internalIndex = logicalIndex % spanWidth;
    const int userFrameId = logicalIndex;

    const int spanEnd = qMin(count(), logicalIndex + spanWidth);
    QRect spanRect(rect.topLeft(), QSize(width * (spanEnd - logicalIndex), textHeight));

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QStyleOptionViewItem option = viewOptions();
#else
    QStyleOptionViewItem option;
    initViewItemOption(&option);
#endif
    const int gridHint = style()->styleHint(QStyle::SH_Table_GridLineColor, &option, this);
    const QColor gridColor = static_cast<QRgb>(gridHint);
    const QPen gridPen = QPen(gridColor);

    if (!internalIndex) {
        bool isIntegralLine = (logicalIndex + spanWidth) % m_d->fps == 0;
        bool isPrevIntegralLine = logicalIndex % m_d->fps == 0;
        paintSpan(painter, userFrameId, spanRect, isIntegralLine, isPrevIntegralLine, style(), palette(), gridPen);
    }

    {
        QBrush fillColor = KisAnimTimelineColors::instance()->headerEmpty();

        QVariant activeValue = model()->headerData(logicalIndex, orientation(),
                                                   KisTimeBasedItemModel::ActiveFrameRole);

        QVariant cachedValue = model()->headerData(logicalIndex, orientation(),
                                                   KisTimeBasedItemModel::FrameCachedRole);

        QVariant withinRangeValue = model()->headerData(logicalIndex, orientation(),
                                                        KisTimeBasedItemModel::WithinClipRange);

        const bool isActive = activeValue.isValid() && activeValue.toBool();
        const bool isCached = cachedValue.isValid() && cachedValue.toBool();
        const bool isWithinRange = withinRangeValue.isValid() && withinRangeValue.toBool();

        if (isActive) {
            fillColor = KisAnimTimelineColors::instance()->headerActive();
        } else if (isCached && isWithinRange) {
            fillColor = KisAnimTimelineColors::instance()->headerCachedFrame();
        }

        painter->fillRect(frameRect, fillColor);

        QVector<QLine> lines;
        lines << QLine(p1, p2);
        lines << QLine(frameRect.topRight(), frameRect.bottomRight());
        lines << QLine(frameRect.bottomLeft(), frameRect.bottomRight());

        QPen oldPen = painter->pen();
        painter->setPen(gridPen);
        painter->drawLines(lines);
        painter->setPen(oldPen);
    }
}

void KisAnimTimelineTimeHeader::changeEvent(QEvent *event)
{
    Q_UNUSED(event);

    updateMinimumSize();
}

void KisAnimTimelineTimeHeader::setFramePerSecond(int fps)
{
    m_d->fps = fps;
    update();
}

bool KisAnimTimelineTimeHeader::setZoom(qreal zoom)
{
    qreal newSectionSize = zoom * m_d->unitSectionSize;

    if (newSectionSize < m_d->minSectionSize) {
        newSectionSize = m_d->minSectionSize;
        zoom = qreal(newSectionSize) / m_d->unitSectionSize;
    } else if (newSectionSize > m_d->maxSectionSize) {
        newSectionSize = m_d->maxSectionSize;
        zoom = qreal(newSectionSize) / m_d->unitSectionSize;
    }

    m_d->remainder = newSectionSize - floor(newSectionSize);

    if (newSectionSize != defaultSectionSize()) {
        setDefaultSectionSize(newSectionSize);
        Q_EMIT sigZoomChanged(zoom);
        return true;
    }

    return false;
}

qreal KisAnimTimelineTimeHeader::zoom() {
    return  (qreal(defaultSectionSize() + m_d->remainder) / m_d->unitSectionSize);
}

void KisAnimTimelineTimeHeader::updateMinimumSize()
{
    QFontMetrics metrics(this->font());
    const int textHeight = metrics.height();

    setMinimumSize(0, 1.5 * textHeight);
}

void KisAnimTimelineTimeHeader::setModel(QAbstractItemModel *model)
{
    KisTimeBasedItemModel *framesModel = qobject_cast<KisTimeBasedItemModel*>(model);
    m_d->model = framesModel;

    QHeaderView::setModel(model);
}

int getColumnCount(const QModelIndexList &indexes, int *leftmostCol, int *rightmostCol)
{
    QVector<int> columns;
    int leftmost = std::numeric_limits<int>::max();
    int rightmost = std::numeric_limits<int>::min();

    Q_FOREACH (const QModelIndex &index, indexes) {
        leftmost = qMin(leftmost, index.column());
        rightmost = qMax(rightmost, index.column());
        if (!columns.contains(index.column())) {
            columns.append(index.column());
        }
    }

    if (leftmostCol) *leftmostCol = leftmost;
    if (rightmostCol) *rightmostCol = rightmost;

    return columns.size();
}

void KisAnimTimelineTimeHeader::mousePressEvent(QMouseEvent *e)
{
    int logical = logicalIndexAt(e->pos());
    if (logical != -1) {
        QModelIndexList selectedIndexes = selectionModel()->selectedIndexes();
        int numSelectedColumns = getColumnCount(selectedIndexes, 0, 0);

        if (e->button() == Qt::RightButton) {
            if (numSelectedColumns <= 1) {
                model()->setHeaderData(logical, orientation(), true, KisTimeBasedItemModel::ActiveFrameRole);
                model()->setHeaderData(logical, orientation(), QVariant(int(SEEK_FINALIZE | SEEK_PUSH_AUDIO)), KisTimeBasedItemModel::ScrubToRole);
            }

            /* Fix for safe-assert involving kis_animation_curve_docker.
             * There should probably be a more elegant way for dealing
             * with reused timeline_ruler_header instances in other
             * timeline views instead of simply animation_frame_view.
             *
             * This works for now though... */
            if(!m_d->actionMan){
                return;
            }

            QMenu menu;

            menu.addSection(i18n("Edit Columns:"));
            menu.addSeparator();

            KisActionManager::safePopulateMenu(&menu, "cut_columns_to_clipboard", m_d->actionMan);
            KisActionManager::safePopulateMenu(&menu, "copy_columns_to_clipboard", m_d->actionMan);
            KisActionManager::safePopulateMenu(&menu, "paste_columns_from_clipboard", m_d->actionMan);

            menu.addSeparator();

            {   //Frame Columns Submenu
                QMenu *frames = menu.addMenu(i18nc("@item:inmenu", "Keyframe Columns"));
                KisActionManager::safePopulateMenu(frames, "insert_column_left", m_d->actionMan);
                KisActionManager::safePopulateMenu(frames, "insert_column_right", m_d->actionMan);
                frames->addSeparator();
                KisActionManager::safePopulateMenu(frames, "insert_multiple_columns", m_d->actionMan);
            }

            {   //Hold Columns Submenu
                QMenu *hold = menu.addMenu(i18nc("@item:inmenu", "Hold Frame Columns"));
                KisActionManager::safePopulateMenu(hold, "insert_hold_column", m_d->actionMan);
                KisActionManager::safePopulateMenu(hold, "remove_hold_column", m_d->actionMan);
                hold->addSeparator();
                KisActionManager::safePopulateMenu(hold, "insert_multiple_hold_columns", m_d->actionMan);
                KisActionManager::safePopulateMenu(hold, "remove_multiple_hold_columns", m_d->actionMan);
            }

            menu.addSeparator();

            KisActionManager::safePopulateMenu(&menu, "remove_columns", m_d->actionMan);
            KisActionManager::safePopulateMenu(&menu, "remove_columns_and_pull", m_d->actionMan);

            if (numSelectedColumns > 1) {
                menu.addSeparator();
                KisActionManager::safePopulateMenu(&menu, "mirror_columns", m_d->actionMan);
            }

            menu.addSeparator();

            KisActionManager::safePopulateMenu(&menu, "clear_animation_cache", m_d->actionMan);

            menu.exec(e->globalPos());

            return;

        } else if (e->button() == Qt::LeftButton) {
            m_d->lastPressSectionIndex = logical;
            model()->setHeaderData(logical, orientation(), true, KisTimeBasedItemModel::ActiveFrameRole);
        }
    }

    QHeaderView::mousePressEvent(e);
}

void KisAnimTimelineTimeHeader::mouseMoveEvent(QMouseEvent *e)
{
    int logical = logicalIndexAt(e->pos());
    if (logical != -1) {

        if (e->buttons() & Qt::LeftButton) {

            m_d->model->setScrubState(true);
            QVariant activeValue = model()->headerData(logical, orientation(), KisTimeBasedItemModel::ActiveFrameRole);
            KIS_ASSERT(activeValue.type() == QVariant::Bool);
            if (activeValue.toBool() != true) {
                model()->setHeaderData(logical, orientation(), true, KisTimeBasedItemModel::ActiveFrameRole);
                model()->setHeaderData(logical, orientation(), QVariant(int(SEEK_PUSH_AUDIO)), KisTimeBasedItemModel::ScrubToRole);
            }

            if (m_d->lastPressSectionIndex >= 0 &&
                logical != m_d->lastPressSectionIndex &&
                e->modifiers() & Qt::ShiftModifier) {

                const int minCol = qMin(m_d->lastPressSectionIndex, logical);
                const int maxCol = qMax(m_d->lastPressSectionIndex, logical);

                QItemSelection sel(m_d->model->index(0, minCol), m_d->model->index(0, maxCol));
                selectionModel()->select(sel,
                                         QItemSelectionModel::Columns |
                                         QItemSelectionModel::SelectCurrent);
            }

        }

    }

    QHeaderView::mouseMoveEvent(e);
}

int KisAnimTimelineTimeHeader::estimateLastVisibleColumn()
{
    const int sectionWidth = defaultSectionSize();
    return (m_d->offset + width() - 1) / sectionWidth;
}

int KisAnimTimelineTimeHeader::estimateFirstVisibleColumn()
{
    const int sectionWidth = defaultSectionSize();
    return ceil(qreal(m_d->offset) / sectionWidth);
}

void KisAnimTimelineTimeHeader::zoomToFitFrameRange(int start, int end)
{
    const int PADDING = 2;
    qreal lengthSections = (end + PADDING) - start;
    qreal desiredZoom = width() / lengthSections;

    setZoom(desiredZoom / m_d->unitSectionSize);
}

void KisAnimTimelineTimeHeader::mouseReleaseEvent(QMouseEvent *e)
{
    if (!m_d->model)
        return;

    if (e->button() == Qt::LeftButton) {
        int timeUnderMouse = qMax(logicalIndexAt(e->pos()), 0);
        model()->setHeaderData(timeUnderMouse, orientation(), true, KisTimeBasedItemModel::ActiveFrameRole);
        if (timeUnderMouse != m_d->model->currentTime()) {
            model()->setHeaderData(timeUnderMouse, orientation(), QVariant(int(SEEK_PUSH_AUDIO | SEEK_FINALIZE)), KisTimeBasedItemModel::ScrubToRole);
        }
        m_d->model->setScrubState(false);
    }

    QHeaderView::mouseReleaseEvent(e);
}

QModelIndexList KisAnimTimelineTimeHeader::Private::prepareFramesSlab(int startCol, int endCol)
{
    QModelIndexList frames;

    const int numRows = model->rowCount();

    for (int i = 0; i < numRows; i++) {
        for (int j = startCol; j <= endCol; j++) {
            QModelIndex index = model->index(i, j);
            const bool exists = model->data(index, KisTimeBasedItemModel::FrameExistsRole).toBool();
            if (exists) {
                frames << index;
            }
        }
    }

    return frames;
}
