/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include <QMouseEvent>
#include <QStylePainter>
#include <QGraphicsItem>
#include <QDomDocument>
#include <QScrollBar>
#include <QApplication>
#include <QInputDialog>

#include <KDebug>
#include <KLocale>
#include <KUrl>
#include <KIcon>
#include <KCursor>

#include "customtrackview.h"
#include "customtrackscene.h"
#include "docclipbase.h"
#include "clipitem.h"
#include "definitions.h"
#include "moveclipcommand.h"
#include "movetransitioncommand.h"
#include "resizeclipcommand.h"
#include "editguidecommand.h"
#include "addtimelineclipcommand.h"
#include "addeffectcommand.h"
#include "editeffectcommand.h"
#include "moveeffectcommand.h"
#include "addtransitioncommand.h"
#include "edittransitioncommand.h"
#include "editkeyframecommand.h"
#include "changespeedcommand.h"
#include "addmarkercommand.h"
#include "razorclipcommand.h"
#include "kdenlivesettings.h"
#include "transition.h"
#include "clipitem.h"
#include "customtrackview.h"
#include "clipmanager.h"
#include "renderer.h"
#include "markerdialog.h"
#include "mainwindow.h"
#include "ui_keyframedialog_ui.h"
#include "clipdurationdialog.h"
#include "abstractgroupitem.h"


//TODO:
// disable animation if user asked it in KDE's global settings
// http://lists.kde.org/?l=kde-commits&m=120398724717624&w=2
// needs something like below (taken from dolphin)
// #include <kglobalsettings.h>
// const bool animate = KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects;
// const int duration = animate ? 1500 : 1;

CustomTrackView::CustomTrackView(KdenliveDoc *doc, CustomTrackScene* projectscene, QWidget *parent)
        : QGraphicsView(projectscene, parent), m_scene(projectscene), m_cursorPos(0), m_cursorLine(NULL), m_operationMode(NONE), m_dragItem(NULL), m_visualTip(NULL), m_moveOpMode(NONE), m_animation(NULL), m_projectDuration(0), m_clickPoint(QPoint()), m_document(doc), m_autoScroll(KdenliveSettings::autoscroll()), m_tracksHeight(KdenliveSettings::trackheight()), m_tool(SELECTTOOL), m_dragGuide(NULL), m_findIndex(0), m_menuPosition(QPoint()), m_blockRefresh(false), m_selectionGroup(NULL), m_selectedTrack(0), m_copiedItems(QList<AbstractClipItem *> ()) {
    if (doc) m_commandStack = doc->commandStack();
    else m_commandStack == NULL;
    setMouseTracking(true);
    setAcceptDrops(true);
    m_animationTimer = new QTimeLine(800);
    m_animationTimer->setFrameRange(0, 5);
    m_animationTimer->setUpdateInterval(100);
    m_animationTimer->setLoopCount(0);
    m_tipColor = QColor(0, 192, 0, 200);
    QColor border = QColor(255, 255, 255, 100);
    m_tipPen.setColor(border);
    m_tipPen.setWidth(3);
    setContentsMargins(0, 0, 0, 0);
    if (projectscene) {
        m_cursorLine = projectscene->addLine(0, 0, 0, m_tracksHeight);
        m_cursorLine->setZValue(1000);
    }

    KIcon razorIcon("edit-cut");
    m_razorCursor = QCursor(razorIcon.pixmap(22, 22));
    verticalScrollBar()->setTracking(true);
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(slotRefreshGuides()));
}

CustomTrackView::~CustomTrackView() {
    qDeleteAll(m_guides);
}

void CustomTrackView::setDocumentModified() {
    m_document->setModified(true);
}

void CustomTrackView::setContextMenu(QMenu *timeline, QMenu *clip, QMenu *transition) {
    m_timelineContextMenu = timeline;
    m_timelineContextClipMenu = clip;
    m_timelineContextTransitionMenu = transition;
}

void CustomTrackView::checkAutoScroll() {
    m_autoScroll = KdenliveSettings::autoscroll();
}

QList <TrackInfo> CustomTrackView::tracksList() const {
    return m_scene->m_tracksList;
}

void CustomTrackView::checkTrackHeight() {
    if (m_tracksHeight == KdenliveSettings::trackheight()) return;
    m_tracksHeight = KdenliveSettings::trackheight();
    emit trackHeightChanged();
    QList<QGraphicsItem *> itemList = items();
    ClipItem *item;
    Transition *transitionitem;
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            item = (ClipItem*) itemList.at(i);
            item->setRect(0, 0, item->rect().width() - 0.02, m_tracksHeight - 1);
            item->setPos((qreal) item->startPos().frames(m_document->fps()), (qreal) item->track() * m_tracksHeight + 1);
            item->resetThumbs();
        } else if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            transitionitem = (Transition*) itemList.at(i);
            transitionitem->setRect(0, 0, transitionitem->rect().width() - 0.02, m_tracksHeight / 3 * 2 - 1);
            transitionitem->setPos((qreal) transitionitem->startPos().frames(m_document->fps()), (qreal) transitionitem->track() * m_tracksHeight + m_tracksHeight / 3 * 2);
        }
    }
    m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), m_tracksHeight * m_scene->m_tracksList.count());

    for (int i = 0; i < m_guides.count(); i++) {
        QLineF l = m_guides.at(i)->line();
        l.setP2(QPointF(l.x2(), m_tracksHeight * m_scene->m_tracksList.count()));
        m_guides.at(i)->setLine(l);
    }

    setSceneRect(0, 0, sceneRect().width(), m_tracksHeight * m_scene->m_tracksList.count());
    verticalScrollBar()->setMaximum(m_tracksHeight * m_scene->m_tracksList.count());
    update();
}

// virtual
void CustomTrackView::resizeEvent(QResizeEvent * event) {
    QGraphicsView::resizeEvent(event);
}

// virtual
/** Zoom or move viewport on mousewheel
 *
 * If mousewheel+Ctrl, zooms in/out on the timeline.
 *
 * With Ctrl, moves viewport towards end of timeline if down/back,
 * opposite on up/forward.
 *
 * See also http://www.kdenlive.org/mantis/view.php?id=265 */
void CustomTrackView::wheelEvent(QWheelEvent * e) {
    if (e->modifiers() == Qt::ControlModifier) {
        if (e->delta() > 0) emit zoomIn();
        else emit zoomOut();
    } else {
        if (e->delta() <= 0) horizontalScrollBar()->setValue(horizontalScrollBar()->value() + horizontalScrollBar()->singleStep());
        else  horizontalScrollBar()->setValue(horizontalScrollBar()->value() - horizontalScrollBar()->singleStep());
    }
}

int CustomTrackView::getPreviousVideoTrack(int track) {
    track = m_scene->m_tracksList.count() - track - 1;
    track --;
    for (int i = track; i > -1; i--) {
        if (m_scene->m_tracksList.at(i).type == VIDEOTRACK) return i + 1;
    }
    return 0;
}

// virtual

void CustomTrackView::mouseMoveEvent(QMouseEvent * event) {
    int pos = event->x();
    emit mousePosition((int)(mapToScene(event->pos()).x()));
    if (event->buttons() & Qt::MidButton) return;
    if (event->buttons() != Qt::NoButton) {
        if (m_dragItem && m_tool == SELECTTOOL) {
            if (m_operationMode == MOVE) {
                if ((event->pos() - m_clickEvent).manhattanLength() >= QApplication::startDragDistance()) QGraphicsView::mouseMoveEvent(event);

                /*&& (event->pos() - m_clickEvent).manhattanLength() >= QApplication::startDragDistance()) {
                           double snappedPos = getSnapPointForPos(mapToScene(event->pos()).x() - m_clickPoint.x());
                           //kDebug() << "///////  MOVE CLIP, EVENT Y: "<<m_clickPoint.y();//<<event->scenePos().y()<<", SCENE HEIGHT: "<<scene()->sceneRect().height();
                           int moveTrack = (int)  mapToScene(event->pos() - QPoint(0, (m_dragItem->type() == TRANSITIONWIDGET ? m_dragItem->boundingRect().height() / 2 : 0))).y() / m_tracksHeight;
                           int currentTrack = m_dragItem->track();

                           if (moveTrack > 1000) moveTrack = 0;
                           else if (moveTrack > m_scene->m_tracksList.count() - 1) moveTrack = m_scene->m_tracksList.count() - 1;
                           else if (moveTrack < 0) moveTrack = 0;

                           int offset = moveTrack - currentTrack;
                           if (m_selectedClipList.count() == 1) m_dragItem->moveTo((int)(snappedPos / m_scale), m_scale, offset * m_tracksHeight, moveTrack);
                           else {
                               int moveOffset = (int)(snappedPos / m_scale) - m_dragItem->startPos().frames(m_document->fps());
                               if (canBeMoved(m_selectedClipList, GenTime(moveOffset, m_document->fps()), offset)) {
                                   for (int i = 0; i < m_selectedClipList.count(); i++) {
                                       AbstractClipItem *item = m_selectedClipList.at(i);
                                       item->moveTo(item->startPos().frames(m_document->fps()) + moveOffset, m_scale, offset * m_tracksHeight, item->track() + offset, false);
                                   }
                               }
                           }*/

            } else if (m_operationMode == RESIZESTART) {
                double snappedPos = getSnapPointForPos(mapToScene(event->pos()).x());
                m_dragItem->resizeStart((int)(snappedPos));
            } else if (m_operationMode == RESIZEEND) {
                double snappedPos = getSnapPointForPos(mapToScene(event->pos()).x());
                m_dragItem->resizeEnd((int)(snappedPos));
            } else if (m_operationMode == FADEIN) {
                int pos = (int)(mapToScene(event->pos()).x());
                ((ClipItem*) m_dragItem)->setFadeIn((int)(pos - m_dragItem->startPos().frames(m_document->fps())));
            } else if (m_operationMode == FADEOUT) {
                int pos = (int)(mapToScene(event->pos()).x());
                ((ClipItem*) m_dragItem)->setFadeOut((int)(m_dragItem->endPos().frames(m_document->fps()) - pos));
            } else if (m_operationMode == KEYFRAME) {
                GenTime keyFramePos = GenTime((int)(mapToScene(event->pos()).x()), m_document->fps()) - m_dragItem->startPos() + m_dragItem->cropStart();
                double pos = mapToScene(event->pos()).toPoint().y();
                QRectF br = m_dragItem->sceneBoundingRect();
                double maxh = 100.0 / br.height();
                pos = (br.bottom() - pos) * maxh;
                m_dragItem->updateKeyFramePos(keyFramePos, pos);
            }

            if (m_animation) delete m_animation;
            m_animation = NULL;
            if (m_visualTip) delete m_visualTip;
            m_visualTip = NULL;
            return;
        } else if (m_operationMode == MOVEGUIDE) {
            if (m_animation) delete m_animation;
            m_animation = NULL;
            if (m_visualTip) delete m_visualTip;
            m_visualTip = NULL;
            QGraphicsView::mouseMoveEvent(event);
            return;
        }
    }

    if (m_tool == RAZORTOOL) {
        setCursor(m_razorCursor);
        //QGraphicsView::mouseMoveEvent(event);
        //return;
    }

    QList<QGraphicsItem *> itemList = items(event->pos());
    QGraphicsRectItem *item = NULL;
    OPERATIONTYPE opMode = NONE;

    if (itemList.count() == 1 && itemList.at(0)->type() == GUIDEITEM) {
        opMode = MOVEGUIDE;
    } else for (int i = 0; i < itemList.count(); i++) {
            if (itemList.at(i)->type() == AVWIDGET || itemList.at(i)->type() == TRANSITIONWIDGET) {
                item = (QGraphicsRectItem*) itemList.at(i);
                break;
            }
        }

    if (item && event->buttons() == Qt::NoButton) {
        AbstractClipItem *clip = static_cast <AbstractClipItem*>(item);
        if (m_tool == RAZORTOOL) {
            // razor tool over a clip, display current frame in monitor
            if (!m_blockRefresh && item->type() == AVWIDGET) {
                //TODO: solve crash when showing frame when moving razor over clip
                //emit showClipFrame(((ClipItem *) item)->baseClip(), mapToScene(event->pos()).x() / m_scale - (clip->startPos() - clip->cropStart()).frames(m_document->fps()));
            }
            event->accept();
            return;
        }
        opMode = clip->operationMode(mapToScene(event->pos()));
        double size = 5;
        if (opMode == m_moveOpMode) {
            QGraphicsView::mouseMoveEvent(event);
            return;
        } else {
            if (m_visualTip) {
                if (m_animation) delete m_animation;
                m_animation = NULL;
                m_animationTimer->stop();
                delete m_visualTip;
                m_visualTip = NULL;
            }
        }
        m_moveOpMode = opMode;
        if (opMode == MOVE) {
            setCursor(Qt::OpenHandCursor);
        } else if (opMode == RESIZESTART) {
            setCursor(KCursor("left_side", Qt::SizeHorCursor));
            if (m_visualTip == NULL) {
                QRectF rect = clip->sceneBoundingRect();
                QPolygon polygon;
                polygon << QPoint(0, rect.height() / 2 - size * 2);
                polygon << QPoint(size * 2, (int)(rect.height() / 2));
                polygon << QPoint(0, (int)(rect.height() / 2 + size * 2));
                polygon << QPoint(0, (int)(rect.height() / 2 - size * 2));

                m_visualTip = new QGraphicsPolygonItem(polygon);
                ((QGraphicsPolygonItem*) m_visualTip)->setBrush(m_tipColor);
                ((QGraphicsPolygonItem*) m_visualTip)->setPen(m_tipPen);
                m_visualTip->setPos(rect.x(), rect.y());
                m_visualTip->setFlags(QGraphicsItem::ItemIgnoresTransformations);
                m_visualTip->setZValue(100);
                m_animation = new QGraphicsItemAnimation;
                m_animation->setItem(m_visualTip);
                m_animation->setTimeLine(m_animationTimer);
                double scale = 2.0;
                m_animation->setScaleAt(.5, scale, 1);
                //m_animation->setPosAt(.5, QPointF(rect.x() - rect.x() * scale, 0));
                scale = 1.0;
                m_animation->setScaleAt(1, scale, 1);
                //m_animation->setPosAt(1, QPointF(rect.x() - rect.x() * scale, 0));
                scene()->addItem(m_visualTip);
                m_animationTimer->start();
            }
        } else if (opMode == RESIZEEND) {
            setCursor(KCursor("right_side", Qt::SizeHorCursor));
            if (m_visualTip == NULL) {
                QRectF rect = clip->sceneBoundingRect();
                QPolygon polygon;
                polygon << QPoint(0, (int)(rect.height() / 2 - size * 2));
                polygon << QPoint(- size * 2, (int)(rect.height() / 2));
                polygon << QPoint(0, (int)(rect.height() / 2 + size * 2));
                polygon << QPoint(0, (int)(rect.height() / 2 - size * 2));

                m_visualTip = new QGraphicsPolygonItem(polygon);
                ((QGraphicsPolygonItem*) m_visualTip)->setBrush(m_tipColor);
                ((QGraphicsPolygonItem*) m_visualTip)->setPen(m_tipPen);
                m_visualTip->setFlags(QGraphicsItem::ItemIgnoresTransformations);
                m_visualTip->setPos(rect.right(), rect.y());
                m_visualTip->setZValue(100);
                m_animation = new QGraphicsItemAnimation;
                m_animation->setItem(m_visualTip);
                m_animation->setTimeLine(m_animationTimer);
                double scale = 2.0;
                m_animation->setScaleAt(.5, scale, 1);
                scale = 1.0;
                m_animation->setScaleAt(1, scale, 1);
                scene()->addItem(m_visualTip);
                m_animationTimer->start();
            }
        } else if (opMode == FADEIN) {
            if (m_visualTip == NULL) {
                ClipItem *item = (ClipItem *) clip;
                QRectF rect = clip->sceneBoundingRect();
                m_visualTip = new QGraphicsEllipseItem(-size, -size, size * 2, size * 2);
                ((QGraphicsEllipseItem*) m_visualTip)->setBrush(m_tipColor);
                ((QGraphicsEllipseItem*) m_visualTip)->setPen(m_tipPen);
                m_visualTip->setPos(rect.x() + item->fadeIn(), rect.y());
                m_visualTip->setFlags(QGraphicsItem::ItemIgnoresTransformations);
                m_visualTip->setZValue(100);
                m_animation = new QGraphicsItemAnimation;
                m_animation->setItem(m_visualTip);
                m_animation->setTimeLine(m_animationTimer);
                double scale = 2.0;
                m_animation->setScaleAt(.5, scale, scale);
                scale = 1.0;
                m_animation->setScaleAt(1, scale, scale);
                scene()->addItem(m_visualTip);
                m_animationTimer->start();
            }
            setCursor(Qt::PointingHandCursor);
        } else if (opMode == FADEOUT) {
            if (m_visualTip == NULL) {
                ClipItem *item = (ClipItem *) clip;
                QRectF rect = clip->sceneBoundingRect();
                m_visualTip = new QGraphicsEllipseItem(-size, -size, size * 2, size * 2);
                ((QGraphicsEllipseItem*) m_visualTip)->setBrush(m_tipColor);
                ((QGraphicsEllipseItem*) m_visualTip)->setPen(m_tipPen);
                m_visualTip->setPos(rect.right() - item->fadeOut(), rect.y());
                m_visualTip->setFlags(QGraphicsItem::ItemIgnoresTransformations);
                m_visualTip->setZValue(100);
                m_animation = new QGraphicsItemAnimation;
                m_animation->setItem(m_visualTip);
                m_animation->setTimeLine(m_animationTimer);
                double scale = 2.0;
                m_animation->setScaleAt(.5, scale, scale);
                scale = 1.0;
                m_animation->setScaleAt(1, scale, scale);
                scene()->addItem(m_visualTip);
                m_animationTimer->start();
            }
            setCursor(Qt::PointingHandCursor);
        } else if (opMode == TRANSITIONSTART) {
            /*if (m_visualTip == NULL) {
                QRectF rect = clip->sceneBoundingRect();
                m_visualTip = new QGraphicsEllipseItem(-5, -5 , 10, 10);
                ((QGraphicsEllipseItem*) m_visualTip)->setBrush(m_tipColor);
                ((QGraphicsEllipseItem*) m_visualTip)->setPen(m_tipPen);
                m_visualTip->setZValue(100);
                m_animation = new QGraphicsItemAnimation;
                m_animation->setItem(m_visualTip);
                m_animation->setTimeLine(m_animationTimer);
                m_visualTip->setPos(rect.x() + 10, rect.y() + rect.height() / 2 + 12);
                double scale = 2.0;
                m_animation->setScaleAt(.5, scale, scale);
                scale = 1.0;
                m_animation->setScaleAt(1, scale, scale);
                scene()->addItem(m_visualTip);
                m_animationTimer->start();
            }*/
            setCursor(Qt::PointingHandCursor);
        } else if (opMode == TRANSITIONEND) {
            /*if (m_visualTip == NULL) {
                QRectF rect = clip->sceneBoundingRect();
                m_visualTip = new QGraphicsEllipseItem(-5, -5 , 10, 10);
                ((QGraphicsEllipseItem*) m_visualTip)->setBrush(m_tipColor);
                ((QGraphicsEllipseItem*) m_visualTip)->setPen(m_tipPen);
                m_visualTip->setZValue(100);
                m_animation = new QGraphicsItemAnimation;
                m_animation->setItem(m_visualTip);
                m_animation->setTimeLine(m_animationTimer);
                m_visualTip->setPos(rect.x() + rect.width() - 10 , rect.y() + rect.height() / 2 + 12);
                double scale = 2.0;
                m_animation->setScaleAt(.5, scale, scale);
                scale = 1.0;
                m_animation->setScaleAt(1, scale, scale);
                scene()->addItem(m_visualTip);
                m_animationTimer->start();
            }*/
            setCursor(Qt::PointingHandCursor);
        } else if (opMode == KEYFRAME) {
            setCursor(Qt::PointingHandCursor);
        }
    } // no clip under mouse
    else if (m_tool == RAZORTOOL) {
        event->accept();
        return;
    } else if (opMode == MOVEGUIDE) {
        m_moveOpMode = opMode;
        setCursor(Qt::SplitHCursor);
    } else {
        m_moveOpMode = NONE;
        if (event->buttons() != Qt::NoButton && event->modifiers() == Qt::NoModifier) {
            setCursorPos((int)(mapToScene(event->pos().x(), 0).x()));
        }
        if (m_visualTip) {
            if (m_animation) delete m_animation;
            m_animationTimer->stop();
            m_animation = NULL;
            delete m_visualTip;
            m_visualTip = NULL;

        }
        setCursor(Qt::ArrowCursor);
    }
    QGraphicsView::mouseMoveEvent(event);
}

// virtual
void CustomTrackView::mousePressEvent(QMouseEvent * event) {
    m_menuPosition = QPoint();
    m_blockRefresh = true;
    bool collision = false;

    if (m_tool != RAZORTOOL) activateMonitor();
    else if (m_document->renderer()->playSpeed() != 0.0) {
        m_document->renderer()->pause();
        return;
    }
    m_clickEvent = event->pos();

    // special cases (middle click button or ctrl / shift click
    if (event->button() == Qt::MidButton) {
        m_document->renderer()->switchPlay();
        m_blockRefresh = false;
        return;
    }

    // check item under mouse
    QList<QGraphicsItem *> collisionList = items(event->pos());

    if (event->modifiers() == Qt::ControlModifier && collisionList.count() == 0) {
        setDragMode(QGraphicsView::ScrollHandDrag);
        QGraphicsView::mousePressEvent(event);
        m_blockRefresh = false;
        return;
    }

    if (event->modifiers() == Qt::ShiftModifier && collisionList.count() == 0) {
        setDragMode(QGraphicsView::RubberBandDrag);
        QGraphicsView::mousePressEvent(event);
        m_blockRefresh = false;
        return;
    }

    if (collisionList.count() == 1 && collisionList.at(0)->type() == GUIDEITEM) {
        // a guide item was pressed
        collisionList.at(0)->setFlag(QGraphicsItem::ItemIsMovable, true);
        m_dragItem = NULL;
        m_dragGuide = (Guide *) collisionList.at(0);
        collision = true;
        m_operationMode = MOVEGUIDE;
        // deselect all clips so that only the guide will move
        m_scene->clearSelection();
        if (m_selectionGroup) {
            scene()->destroyItemGroup(m_selectionGroup);
            m_selectionGroup = NULL;
        }
        updateSnapPoints(NULL);
        QGraphicsView::mousePressEvent(event);
        return;
    }

    // Find first clip or transition under mouse
    int i = 0;
    m_dragItem = NULL;
    while (i < collisionList.count()) {
        if (collisionList.at(i)->type() == AVWIDGET || collisionList.at(i)->type() == TRANSITIONWIDGET) {
            m_dragItem = static_cast <AbstractClipItem *>(collisionList.at(i));
            m_dragItemInfo = m_dragItem->info();
            break;
        }
        i++;
    }

    // context menu requested
    if (event->button() == Qt::RightButton) {
        if (m_dragItem) {
            if (!m_dragItem->isSelected()) {
                m_scene->clearSelection();
                if (m_selectionGroup) {
                    scene()->destroyItemGroup(m_selectionGroup);
                    m_selectionGroup = NULL;
                }
                m_dragItem->setSelected(true);
            }
        }
        m_operationMode = NONE;
        displayContextMenu(event->globalPos(), m_dragItem);
        m_menuPosition = event->pos();
        m_dragItem = NULL;
        event->accept();
        return;
    }

    // No item under click
    if (m_dragItem == NULL) {
        if (m_selectionGroup) {
            scene()->destroyItemGroup(m_selectionGroup);
            m_selectionGroup = NULL;
        }
        setCursor(Qt::ArrowCursor);
        m_scene->clearSelection();
        setCursorPos((int)(mapToScene(event->x(), 0).x()));
        event->accept();
        emit clipItemSelected(NULL);
        return;
    }

    // Razor tool
    if (m_tool == RAZORTOOL) {
        if (m_dragItem->type() == TRANSITIONWIDGET) {
            emit displayMessage(i18n("Cannot cut a transition"), ErrorMessage);
            event->accept();
            m_dragItem = NULL;
            return;
        }
        AbstractClipItem *clip = static_cast <AbstractClipItem *>(m_dragItem);
        RazorClipCommand* command = new RazorClipCommand(this, clip->info(), GenTime((int)(mapToScene(event->pos()).x()), m_document->fps()), true);
        m_commandStack->push(command);
        m_document->setModified(true);
        m_dragItem = NULL;
        event->accept();
        return;
    }
    updateSnapPoints(m_dragItem);
    if (m_dragItem && m_dragItem->type() == AVWIDGET) emit clipItemSelected((ClipItem*) m_dragItem);
    else emit clipItemSelected(NULL);

    if (m_selectionGroup) {
        // delete selection group
        scene()->destroyItemGroup(m_selectionGroup);
        m_selectionGroup = NULL;
    }

    if (m_dragItem && m_operationMode == NONE) QGraphicsView::mousePressEvent(event);

    QList<QGraphicsItem *> selection = m_scene->selectedItems();
    if (selection.count() > 1) {
        m_selectionGroup = new AbstractGroupItem(m_document->fps());
        scene()->addItem(m_selectionGroup);
        for (int i = 0; i < selection.count(); i++) {
            if (selection.at(i)->type() == AVWIDGET || selection.at(i)->type() == TRANSITIONWIDGET)
                m_selectionGroup->addToGroup(selection.at(i));
        }
        QPointF top = m_selectionGroup->boundingRect().topLeft();
        const int width = m_selectionGroup->boundingRect().width();
        const int height = m_selectionGroup->boundingRect().height();
        m_selectionGroup->setPos(top);
        m_selectionGroup->translate(-top.x(), -top.y() + 1);
        m_selectionGroupInfo.startPos = GenTime(m_selectionGroup->scenePos().x(), m_document->fps());
        m_selectionGroupInfo.track = m_selectionGroup->track();
    }

    m_clickPoint = QPoint((int)(mapToScene(event->pos()).x() - m_dragItem->startPos().frames(m_document->fps())), (int)(event->pos().y() - m_dragItem->pos().y()));
    /*
                        if (!item->isSelected()) {

                            if (event->modifiers() != Qt::ControlModifier) {
                                QList<QGraphicsItem *> itemList = items();
                                for (int i = 0; i < itemList.count(); i++) {
                                    itemList.at(i)->setSelected(false);
                                    itemList.at(i)->update();
                                }
                            }
                            item->setSelected(true);
                            item->update();
                        }



                        m_clickPoint = QPoint((int)(mapToScene(event->pos()).x() - m_dragItem->startPos().frames(m_document->fps()) * m_scale), (int)(event->pos().y() - m_dragItem->pos().y()));
                        m_dragItemInfo.startPos = m_dragItem->startPos();
                        m_dragItemInfo.endPos = m_dragItem->endPos();
                        m_dragItemInfo.track = m_dragItem->track();

                        m_selectedClipList.clear();
                        QList<QGraphicsItem *> selected = scene()->selectedItems();
                        for (int i = 0; i < selected.count(); i++) {
                            if (selected.at(i)->type() == AVWIDGET || selected.at(i)->type() == TRANSITIONWIDGET)
                                m_selectedClipList.append(static_cast <AbstractClipItem *>(selected.at(i)));
                        }
          */
    m_operationMode = m_dragItem->operationMode(mapToScene(event->pos()));

    if (m_operationMode == KEYFRAME) {
        m_dragItem->updateSelectedKeyFrame();
        m_blockRefresh = false;
        return;
    } else if (m_operationMode == MOVE) {
        setCursor(Qt::ClosedHandCursor);
    } else if (m_operationMode == TRANSITIONSTART) {
        ItemInfo info;
        info.startPos = m_dragItem->startPos();
        info.track = m_dragItem->track();
        int transitiontrack = getPreviousVideoTrack(info.track);
        ClipItem *transitionClip = NULL;
        if (transitiontrack != 0) transitionClip = getClipItemAt((int) info.startPos.frames(m_document->fps()), m_scene->m_tracksList.count() - transitiontrack);
        if (transitionClip && transitionClip->endPos() < m_dragItem->endPos()) {
            info.endPos = transitionClip->endPos();
        } else info.endPos = info.startPos + GenTime(2.5);

        slotAddTransition((ClipItem *) m_dragItem, info, transitiontrack);
    } else if (m_operationMode == TRANSITIONEND) {
        ItemInfo info;
        info.endPos = m_dragItem->endPos();
        info.track = m_dragItem->track();
        int transitiontrack = getPreviousVideoTrack(info.track);
        ClipItem *transitionClip = NULL;
        if (transitiontrack != 0) transitionClip = getClipItemAt((int) info.endPos.frames(m_document->fps()), m_scene->m_tracksList.count() - transitiontrack);
        if (transitionClip && transitionClip->startPos() > m_dragItem->startPos()) {
            info.startPos = transitionClip->startPos();
        } else info.startPos = info.endPos - GenTime(2.5);
        slotAddTransition((ClipItem *) m_dragItem, info, transitiontrack);
    }

    m_blockRefresh = false;
    //kDebug()<<pos;
    //QGraphicsView::mousePressEvent(event);
}

void CustomTrackView::mouseDoubleClickEvent(QMouseEvent *event) {
    if (m_dragItem && m_dragItem->hasKeyFrames()) {
        if (m_moveOpMode == KEYFRAME) {
            // user double clicked on a keyframe, open edit dialog
            QDialog d(parentWidget());
            Ui::KeyFrameDialog_UI view;
            view.setupUi(&d);
            view.kfr_position->setText(m_document->timecode().getTimecode(GenTime(m_dragItem->selectedKeyFramePos(), m_document->fps()) - m_dragItem->cropStart(), m_document->fps()));
            view.kfr_value->setValue(m_dragItem->selectedKeyFrameValue());
            view.kfr_value->setFocus();
            if (d.exec() == QDialog::Accepted) {
                int pos = m_document->timecode().getFrameCount(view.kfr_position->text(), m_document->fps());
                m_dragItem->updateKeyFramePos(GenTime(pos, m_document->fps()) + m_dragItem->cropStart(), (double) view.kfr_value->value() * m_dragItem->keyFrameFactor());
                ClipItem *item = (ClipItem *)m_dragItem;
                QString previous = item->keyframes(item->selectedEffectIndex());
                item->updateKeyframeEffect();
                QString next = item->keyframes(item->selectedEffectIndex());
                EditKeyFrameCommand *command = new EditKeyFrameCommand(this, item->track(), item->startPos(), item->selectedEffectIndex(), previous, next, false);
                m_commandStack->push(command);
                updateEffect(m_scene->m_tracksList.count() - item->track(), item->startPos(), item->selectedEffect(), item->selectedEffectIndex());
            }

        } else  {
            // add keyframe
            GenTime keyFramePos = GenTime((int)(mapToScene(event->pos()).x()), m_document->fps()) - m_dragItem->startPos() + m_dragItem->cropStart();
            m_dragItem->addKeyFrame(keyFramePos, mapToScene(event->pos()).toPoint().y());
            ClipItem * item = (ClipItem *) m_dragItem;
            QString previous = item->keyframes(item->selectedEffectIndex());
            item->updateKeyframeEffect();
            QString next = item->keyframes(item->selectedEffectIndex());
            EditKeyFrameCommand *command = new EditKeyFrameCommand(this, m_dragItem->track(), m_dragItem->startPos(), item->selectedEffectIndex(), previous, next, false);
            m_commandStack->push(command);
            updateEffect(m_scene->m_tracksList.count() - item->track(), item->startPos(), item->selectedEffect(), item->selectedEffectIndex());
        }
    } else if (m_dragItem) {
        ClipDurationDialog d(m_dragItem, m_document->timecode(), this);
        if (d.exec() == QDialog::Accepted) {
            if (d.startPos() != m_dragItem->startPos()) {
                if (m_dragItem->type() == AVWIDGET) {
                    ItemInfo startInfo;
                    startInfo.startPos = m_dragItem->startPos();
                    startInfo.endPos = m_dragItem->endPos();
                    startInfo.track = m_dragItem->track();
                    ItemInfo endInfo;
                    endInfo.startPos = d.startPos();
                    endInfo.endPos = m_dragItem->endPos() + (endInfo.startPos - startInfo.startPos);
                    endInfo.track = m_dragItem->track();
                    MoveClipCommand *command = new MoveClipCommand(this, startInfo, endInfo, true);
                    m_commandStack->push(command);
                } else {
                    //TODO: move transition
                }
            }
            if (d.duration() != m_dragItem->duration()) {
                if (m_dragItem->type() == AVWIDGET) {
                    ItemInfo startInfo;
                    startInfo.startPos = m_dragItem->startPos();
                    startInfo.endPos = m_dragItem->endPos();
                    startInfo.track = m_dragItem->track();
                    ItemInfo endInfo;
                    endInfo.startPos = startInfo.startPos;
                    endInfo.endPos = endInfo.startPos + d.duration();
                    endInfo.track = m_dragItem->track();
                    ResizeClipCommand *command = new ResizeClipCommand(this, startInfo, endInfo, true);
                    m_commandStack->push(command);
                } else {
                    //TODO: resize transition
                }
            }
        }
    } else {
        QList<QGraphicsItem *> collisionList = items(event->pos());
        if (collisionList.count() == 1 && collisionList.at(0)->type() == GUIDEITEM) {
            Guide *editGuide = (Guide *) collisionList.at(0);
            if (editGuide) slotEditGuide(editGuide->info());
        }
    }
}


void CustomTrackView::editKeyFrame(const GenTime pos, const int track, const int index, const QString keyframes) {
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()), track);
    if (clip) {
        clip->setKeyframes(index, keyframes);
        updateEffect(m_scene->m_tracksList.count() - clip->track(), clip->startPos(), clip->effectAt(index), index);
    } else emit displayMessage(i18n("Cannot find clip with keyframe"), ErrorMessage);
}


void CustomTrackView::displayContextMenu(QPoint pos, AbstractClipItem *clip) {
    if (clip == NULL) m_timelineContextMenu->popup(pos);
    else if (clip->type() == AVWIDGET) m_timelineContextClipMenu->popup(pos);
    else if (clip->type() == TRANSITIONWIDGET) m_timelineContextTransitionMenu->popup(pos);
}

void CustomTrackView::activateMonitor() {
    emit activateDocumentMonitor();
}

void CustomTrackView::dragEnterEvent(QDragEnterEvent * event) {
    if (event->mimeData()->hasFormat("kdenlive/clip")) {
        if (m_selectionGroup) {
            scene()->destroyItemGroup(m_selectionGroup);
            m_selectionGroup = NULL;
        }
        QStringList list = QString(event->mimeData()->data("kdenlive/clip")).split(";");
        m_selectionGroup = new AbstractGroupItem(m_document->fps());
        QPoint pos = QPoint();
        DocClipBase *clip = m_document->getBaseClip(list.at(0));
        if (clip == NULL) kDebug() << " WARNING))))))))) CLIP NOT FOUND : " << list.at(0);
        ItemInfo info;
        info.startPos = GenTime(pos.x(), m_document->fps());
        info.cropStart = GenTime(list.at(1).toInt(), m_document->fps());
        info.endPos = info.startPos + GenTime(list.at(2).toInt() - list.at(1).toInt(), m_document->fps());
        info.track = (int)(pos.y() / m_tracksHeight);
        ClipItem *item = new ClipItem(clip, info, m_document->fps());
        m_selectionGroup->addToGroup(item);
        scene()->addItem(m_selectionGroup);
        event->acceptProposedAction();
    } else if (event->mimeData()->hasFormat("kdenlive/producerslist")) {
        QStringList ids = QString(event->mimeData()->data("kdenlive/producerslist")).split(";");
        m_scene->clearSelection();
        if (m_selectionGroup) {
            scene()->destroyItemGroup(m_selectionGroup);
            m_selectionGroup = NULL;
        }

        m_selectionGroup = new AbstractGroupItem(m_document->fps());
        QPoint pos = QPoint();
        for (int i = 0; i < ids.size(); ++i) {
            DocClipBase *clip = m_document->getBaseClip(ids.at(i));
            if (clip == NULL) kDebug() << " WARNING))))))))) CLIP NOT FOUND : " << ids.at(i);
            ItemInfo info;
            info.startPos = GenTime(pos.x(), m_document->fps());
            info.endPos = info.startPos + clip->duration();
            info.track = (int)(pos.y() / m_tracksHeight);
            ClipItem *item = new ClipItem(clip, info, m_document->fps());
            pos.setX(pos.x() + clip->duration().frames(m_document->fps()));
            m_selectionGroup->addToGroup(item);
        }
        scene()->addItem(m_selectionGroup);
        event->acceptProposedAction();
    } else QGraphicsView::dragEnterEvent(event);
}

void CustomTrackView::slotRefreshEffects(ClipItem *clip) {
    int track = m_scene->m_tracksList.count() - clip->track();
    GenTime pos = clip->startPos();
    if (!m_document->renderer()->mltRemoveEffect(track, pos, "-1", false)) {
        emit displayMessage(i18n("Problem deleting effect"), ErrorMessage);
        return;
    }
    bool success = true;
    for (int i = 0; i < clip->effectsCount(); i++) {
        if (!m_document->renderer()->mltAddEffect(track, pos, clip->getEffectArgs(clip->effectAt(i)), false)) success = false;
    }
    if (!success) emit displayMessage(i18n("Problem adding effect to clip"), ErrorMessage);
    m_document->renderer()->doRefresh();
}

void CustomTrackView::addEffect(int track, GenTime pos, QDomElement effect) {
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()) + 1, m_scene->m_tracksList.count() - track);
    if (clip) {
        QHash <QString, QString> effectParams = clip->addEffect(effect);
        if (!m_document->renderer()->mltAddEffect(track, pos, effectParams))
            emit displayMessage(i18n("Problem adding effect to clip"), ErrorMessage);
        emit clipItemSelected(clip);
    } else emit displayMessage(i18n("Cannot find clip to add effect"), ErrorMessage);
}

void CustomTrackView::deleteEffect(int track, GenTime pos, QDomElement effect) {
    QString index = effect.attribute("kdenlive_ix");
    if (effect.attribute("disabled") != "1" && !m_document->renderer()->mltRemoveEffect(track, pos, index)) {
        emit displayMessage(i18n("Problem deleting effect"), ErrorMessage);
        return;
    }
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()) + 1, m_scene->m_tracksList.count() - track);
    if (clip) {
        clip->deleteEffect(index);
        emit clipItemSelected(clip);
    }
}

void CustomTrackView::slotAddEffect(QDomElement effect, GenTime pos, int track) {
    QList<QGraphicsItem *> itemList;
    if (track == -1) itemList = scene()->selectedItems();
    if (itemList.isEmpty()) {
        ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()) + 1, track);
        if (clip) itemList.append(clip);
        else emit displayMessage(i18n("Select a clip if you want to apply an effect"), ErrorMessage);
    }
    kDebug() << "// REQUESTING EFFECT ON CLIP: " << pos.frames(25) << ", TRK: " << track << "SELECTED ITEMS: " << itemList.count();
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = (ClipItem *)itemList.at(i);
            item->initEffect(effect);
            AddEffectCommand *command = new AddEffectCommand(this, m_scene->m_tracksList.count() - item->track(), item->startPos(), effect, true);
            m_commandStack->push(command);
        }
    }
    m_document->setModified(true);
}

void CustomTrackView::slotDeleteEffect(ClipItem *clip, QDomElement effect) {
    AddEffectCommand *command = new AddEffectCommand(this, m_scene->m_tracksList.count() - clip->track(), clip->startPos(), effect, false);
    m_commandStack->push(command);
    m_document->setModified(true);
}

void CustomTrackView::updateEffect(int track, GenTime pos, QDomElement effect, int ix) {
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()) + 1, m_scene->m_tracksList.count() - track);
    if (clip) {
        QHash <QString, QString> effectParams = clip->getEffectArgs(effect);
        // check if we are trying to reset a keyframe effect
        if (effectParams.contains("keyframes") && effectParams.value("keyframes").isEmpty()) {
            clip->initEffect(effect);
            clip->setEffectAt(ix, effect);
            effectParams = clip->getEffectArgs(effect);
        }
        if (effectParams.value("disabled") == "1") {
            if (m_document->renderer()->mltRemoveEffect(track, pos, effectParams.value("kdenlive_ix"))) {
                kDebug() << "//////  DISABLING EFFECT: " << index << ", CURRENTLA: " << clip->selectedEffectIndex();
            } else emit displayMessage(i18n("Problem deleting effect"), ErrorMessage);
        } else if (!m_document->renderer()->mltEditEffect(m_scene->m_tracksList.count() - clip->track(), clip->startPos(), effectParams))
            emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
        if (ix == clip->selectedEffectIndex()) {
            clip->setSelectedEffect(ix);
        }
        if (effect.attribute("tag") == "volume") {
            // A fade effect was modified, update the clip
            if (effect.attribute("id") == "fadein") {
                int pos = effectParams.value("out").toInt() - effectParams.value("in").toInt();
                clip->setFadeIn(pos);
            }
            if (effect.attribute("id") == "fadeout") {
                int pos = effectParams.value("out").toInt() - effectParams.value("in").toInt();
                clip->setFadeOut(pos);
            }

        }
    }
    m_document->setModified(true);
}

void CustomTrackView::moveEffect(int track, GenTime pos, int oldPos, int newPos) {
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()) + 1, m_scene->m_tracksList.count() - track);
    if (clip) {
        m_document->renderer()->mltMoveEffect(track, pos, oldPos, newPos);
        QDomElement act = clip->effectAt(newPos - 1).cloneNode().toElement();
        QDomElement before = clip->effectAt(oldPos - 1).cloneNode().toElement();
        clip->setEffectAt(oldPos - 1, act);
        clip->setEffectAt(newPos - 1, before);
        emit clipItemSelected(clip, newPos - 1);
    }
    m_document->setModified(true);
}

void CustomTrackView::slotChangeEffectState(ClipItem *clip, int effectPos, bool disable) {
    QDomElement effect = clip->effectAt(effectPos);
    QDomElement oldEffect = effect.cloneNode().toElement();
    effect.setAttribute("disabled", disable);
    EditEffectCommand *command = new EditEffectCommand(this, m_scene->m_tracksList.count() - clip->track(), clip->startPos(), oldEffect, effect, effectPos, true);
    m_commandStack->push(command);
    m_document->setModified(true);
}

void CustomTrackView::slotChangeEffectPosition(ClipItem *clip, int currentPos, int newPos) {
    MoveEffectCommand *command = new MoveEffectCommand(this, m_scene->m_tracksList.count() - clip->track(), clip->startPos(), currentPos, newPos, true);
    m_commandStack->push(command);
    m_document->setModified(true);
}

void CustomTrackView::slotUpdateClipEffect(ClipItem *clip, QDomElement oldeffect, QDomElement effect, int ix) {
    EditEffectCommand *command = new EditEffectCommand(this, m_scene->m_tracksList.count() - clip->track(), clip->startPos(), oldeffect, effect, ix, true);
    m_commandStack->push(command);
}

void CustomTrackView::cutClip(ItemInfo info, GenTime cutTime, bool cut) {
    if (cut) {
        // cut clip
        ClipItem *item = getClipItemAt((int) info.startPos.frames(m_document->fps()) + 1, info.track);
        if (!item || cutTime >= item->endPos() || cutTime <= item->startPos()) {
            emit displayMessage(i18n("Cannot find clip to cut"), ErrorMessage);
            kDebug() << "/////////  ERROR CUTTING CLIP : (" << item->startPos().frames(25) << "-" << item->endPos().frames(25) << "), INFO: (" << info.startPos.frames(25) << "-" << info.endPos.frames(25) << ")" << ", CUT: " << cutTime.frames(25);
            m_blockRefresh = false;
            return;
        }
        kDebug() << "/////////  CUTTING CLIP : (" << item->startPos().frames(25) << "-" << item->endPos().frames(25) << "), INFO: (" << info.startPos.frames(25) << "-" << info.endPos.frames(25) << ")" << ", CUT: " << cutTime.frames(25);

        m_document->renderer()->mltCutClip(m_scene->m_tracksList.count() - info.track, cutTime);
        int cutPos = (int) cutTime.frames(m_document->fps());
        ItemInfo newPos;
        newPos.startPos = cutTime;
        newPos.endPos = info.endPos;
        newPos.cropStart = item->cropStart() + (cutTime - info.startPos);
        newPos.track = info.track;
        ClipItem *dup = item->clone(newPos);
        kDebug() << "// REsizing item to: " << cutPos;
        item->resizeEnd(cutPos, false);
        scene()->addItem(dup);
        if (item->checkKeyFrames()) slotRefreshEffects(item);
        if (dup->checkKeyFrames()) slotRefreshEffects(dup);
        item->baseClip()->addReference();
        m_document->updateClip(item->baseClip()->getId());
        kDebug() << "/////////  CUTTING CLIP RESULT: (" << item->startPos().frames(25) << "-" << item->endPos().frames(25) << "), DUP: (" << dup->startPos().frames(25) << "-" << dup->endPos().frames(25) << ")" << ", CUT: " << cutTime.frames(25);
        kDebug() << "//  CUTTING CLIP dONE";
    } else {
        // uncut clip

        ClipItem *item = getClipItemAt((int) info.startPos.frames(m_document->fps()), info.track);
        ClipItem *dup = getClipItemAt((int) cutTime.frames(m_document->fps()) + 1, info.track);
        if (!item || !dup || item == dup) {
            emit displayMessage(i18n("Cannot find clip to uncut"), ErrorMessage);
            m_blockRefresh = false;
            return;
        }

        kDebug() << "// UNCUTTING CLIPS: ITEM 1 (" << item->startPos().frames(25) << "x" << item->endPos().frames(25) << ")";
        kDebug() << "// UNCUTTING CLIPS: ITEM 2 (" << dup->startPos().frames(25) << "x" << dup->endPos().frames(25) << ")";
        kDebug() << "// UNCUTTING CLIPS, INFO (" << info.startPos.frames(25) << "x" << info.endPos.frames(25) << ") , CUT: " << cutTime.frames(25);;
        //deleteClip(dup->info());


        if (dup->isSelected()) emit clipItemSelected(NULL);
        dup->baseClip()->removeReference();
        m_document->updateClip(dup->baseClip()->getId());
        scene()->removeItem(dup);
        delete dup;
        m_document->renderer()->mltRemoveClip(m_scene->m_tracksList.count() - info.track, cutTime);

        ItemInfo clipinfo = item->info();
        clipinfo.track = m_scene->m_tracksList.count() - clipinfo.track;
        bool success = m_document->renderer()->mltResizeClipEnd(clipinfo, info.endPos - info.startPos);
        if (success) {
            item->resizeEnd((int) info.endPos.frames(m_document->fps()));
        } else
            emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);

    }
    QTimer::singleShot(3000, this, SLOT(slotEnableRefresh()));
}

void CustomTrackView::slotEnableRefresh() {
    m_blockRefresh = false;
}

void CustomTrackView::slotAddTransitionToSelectedClips(QDomElement transition) {
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    if (itemList.count() == 1) {
        if (itemList.at(0)->type() == AVWIDGET) {
            ClipItem *item = (ClipItem *) itemList.at(0);
            ItemInfo info;
            info.track = item->track();
            ClipItem *transitionClip = NULL;
            const int transitiontrack = getPreviousVideoTrack(info.track);
            GenTime pos = GenTime((int)(mapToScene(m_menuPosition).x()), m_document->fps());
            if (pos < item->startPos() + item->duration() / 2) {
                info.startPos = item->startPos();
                if (transitiontrack != 0) transitionClip = getClipItemAt((int) info.startPos.frames(m_document->fps()), m_scene->m_tracksList.count() - transitiontrack);
                if (transitionClip && transitionClip->endPos() < item->endPos()) {
                    info.endPos = transitionClip->endPos();
                } else info.endPos = info.startPos + GenTime(2.5);
            } else {
                info.endPos = item->endPos();
                if (transitiontrack != 0) transitionClip = getClipItemAt((int) info.endPos.frames(m_document->fps()), m_scene->m_tracksList.count() - transitiontrack);
                if (transitionClip && transitionClip->startPos() > item->startPos()) {
                    info.startPos = transitionClip->startPos();
                } else info.startPos = info.endPos - GenTime(2.5);
            }
            slotAddTransition(item, info, transitiontrack, transition);
        }
    } else for (int i = 0; i < itemList.count(); i++) {
            if (itemList.at(i)->type() == AVWIDGET) {
                ClipItem *item = (ClipItem *) itemList.at(i);
                ItemInfo info;
                info.startPos = item->startPos();
                info.endPos = info.startPos + GenTime(2.5);
                info.track = item->track();
                int transitiontrack = getPreviousVideoTrack(info.track);
                slotAddTransition(item, info, transitiontrack, transition);
            }
        }
}

void CustomTrackView::slotAddTransition(ClipItem* clip, ItemInfo transitionInfo, int endTrack, QDomElement transition) {
    AddTransitionCommand* command = new AddTransitionCommand(this, transitionInfo, endTrack, transition, false, true);
    m_commandStack->push(command);
    m_document->setModified(true);
}

void CustomTrackView::addTransition(ItemInfo transitionInfo, int endTrack, QDomElement params) {
    Transition *tr = new Transition(transitionInfo, endTrack, m_document->fps(), params);
    scene()->addItem(tr);

    //kDebug() << "---- ADDING transition " << params.attribute("value");
    m_document->renderer()->mltAddTransition(tr->transitionTag(), endTrack, m_scene->m_tracksList.count() - transitionInfo.track, transitionInfo.startPos, transitionInfo.endPos, tr->toXML());
    m_document->setModified(true);
}

void CustomTrackView::deleteTransition(ItemInfo transitionInfo, int endTrack, QDomElement params) {
    Transition *item = getTransitionItemAt((int)transitionInfo.startPos.frames(m_document->fps()) + 1, transitionInfo.track);
    m_document->renderer()->mltDeleteTransition(item->transitionTag(), endTrack, m_scene->m_tracksList.count() - transitionInfo.track, transitionInfo.startPos, transitionInfo.endPos, item->toXML());
    delete item;
    emit transitionItemSelected(NULL);
    m_document->setModified(true);
}

void CustomTrackView::slotTransitionUpdated(Transition *tr, QDomElement old) {
    EditTransitionCommand *command = new EditTransitionCommand(this, tr->track(), tr->startPos(), old, tr->toXML() , true);
    m_commandStack->push(command);
    m_document->setModified(true);
}

void CustomTrackView::updateTransition(int track, GenTime pos, QDomElement oldTransition, QDomElement transition) {
    Transition *item = getTransitionItemAt((int)pos.frames(m_document->fps()) + 1, track);
    if (!item) {
        kWarning() << "Unable to find transition at pos :" << pos.frames(m_document->fps()) << ", ON track: " << track;
        return;
    }
    m_document->renderer()->mltUpdateTransition(oldTransition.attribute("tag"), transition.attribute("tag"), transition.attribute("transition_btrack").toInt(), m_scene->m_tracksList.count() - transition.attribute("transition_atrack").toInt(), item->startPos(), item->endPos(), transition);
    item->setTransitionParameters(transition);
    m_document->setModified(true);
}

void CustomTrackView::dragMoveEvent(QDragMoveEvent * event) {
    event->setDropAction(Qt::IgnoreAction);
    const int track = (int)(mapToScene(event->pos()).y() / m_tracksHeight);
    const int pos = mapToScene(event->pos()).x();
    kDebug() << "// DRAG MOVE TO TRACK: " << track;
    if (m_selectionGroup) {
        m_selectionGroup->setPos(pos, event->pos().y()/*track * m_tracksHeight + 1 - (int) m_selectionGroup->pos().y()*/);
        event->setDropAction(Qt::MoveAction);
        if (event->mimeData()->hasFormat("kdenlive/producerslist") || event->mimeData()->hasFormat("kdenlive/clip")) {
            event->acceptProposedAction();
        }
    } else {
        QGraphicsView::dragMoveEvent(event);
    }
}

void CustomTrackView::dragLeaveEvent(QDragLeaveEvent * event) {
    if (m_selectionGroup) {
        QList<QGraphicsItem *> items = m_selectionGroup->childItems();
        qDeleteAll(items);
        scene()->destroyItemGroup(m_selectionGroup);
        m_selectionGroup = NULL;
    } else QGraphicsView::dragLeaveEvent(event);
}

void CustomTrackView::dropEvent(QDropEvent * event) {
    if (m_selectionGroup) {
        QList<QGraphicsItem *> items = m_selectionGroup->childItems();
        m_scene->clearSelection();
        if (m_selectionGroup) {
            scene()->destroyItemGroup(m_selectionGroup);
            m_selectionGroup = NULL;
        }
        for (int i = 0; i < items.count(); i++) {
            ClipItem *item = static_cast <ClipItem *>(items.at(i));
            AddTimelineClipCommand *command = new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), item->info(), item->effectList(), false, false);
            m_commandStack->push(command);
            item->baseClip()->addReference();
            m_document->updateClip(item->baseClip()->getId());
            ItemInfo info;
            info = item->info();
            if (item->baseClip()->isTransparent()) {
                // add transparency transition
                int endTrack = getPreviousVideoTrack(info.track);
                Transition *tr = new Transition(info, endTrack, m_document->fps(), MainWindow::transitions.getEffectByTag("composite", "alphatransparency"), true);
                scene()->addItem(tr);
                m_document->renderer()->mltAddTransition(tr->transitionTag(), endTrack, m_scene->m_tracksList.count() - info.track, info.startPos, info.endPos, tr->toXML());
            }
            info.track = m_scene->m_tracksList.count() - item->track();
            m_document->renderer()->mltInsertClip(info, item->xml(), item->baseClip()->producer(item->track()));
            item->setSelected(true);
        }
        m_document->setModified(true);
    } else QGraphicsView::dropEvent(event);
}


QStringList CustomTrackView::mimeTypes() const {
    QStringList qstrList;
    // list of accepted mime types for drop
    qstrList.append("text/plain");
    qstrList.append("kdenlive/producerslist");
    qstrList.append("kdenlive/clip");
    return qstrList;
}

Qt::DropActions CustomTrackView::supportedDropActions() const {
    // returns what actions are supported when dropping
    return Qt::MoveAction;
}

void CustomTrackView::setDuration(int duration) {
    if (duration > sceneRect().width())
        setSceneRect(0, 0, (duration + 100), sceneRect().height());
    m_projectDuration = duration;
}

int CustomTrackView::duration() const {
    return m_projectDuration;
}

void CustomTrackView::addTrack(TrackInfo type) {
    m_scene->m_tracksList << type;
    m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), m_tracksHeight * m_scene->m_tracksList.count());
    setSceneRect(0, 0, sceneRect().width(), m_tracksHeight * m_scene->m_tracksList.count());
    verticalScrollBar()->setMaximum(m_tracksHeight * m_scene->m_tracksList.count());
    //setFixedHeight(50 * m_tracksCount);
}

void CustomTrackView::removeTrack() {
    // TODO: implement track deletion
    //m_tracksCount--;
    m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), m_tracksHeight * m_scene->m_tracksList.count());
}


void CustomTrackView::slotSwitchTrackAudio(int ix) {
    int tracknumber = m_scene->m_tracksList.count() - ix;
    kDebug() << "/////  MUTING TRK: " << ix << "; PL NUM: " << tracknumber;
    m_scene->m_tracksList[tracknumber - 1].isMute = !m_scene->m_tracksList.at(tracknumber - 1).isMute;
    m_document->renderer()->mltChangeTrackState(tracknumber, m_scene->m_tracksList.at(tracknumber - 1).isMute, m_scene->m_tracksList.at(tracknumber - 1).isBlind);
}

void CustomTrackView::slotSwitchTrackVideo(int ix) {
    int tracknumber = m_scene->m_tracksList.count() - ix;
    m_scene->m_tracksList[tracknumber - 1].isBlind = !m_scene->m_tracksList.at(tracknumber - 1).isBlind;
    m_document->renderer()->mltChangeTrackState(tracknumber, m_scene->m_tracksList.at(tracknumber - 1).isMute, m_scene->m_tracksList.at(tracknumber - 1).isBlind);
}

void CustomTrackView::deleteClip(const QString &clipId) {
    QList<QGraphicsItem *> itemList = items();
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = (ClipItem *)itemList.at(i);
            if (item->clipProducer() == clipId) {
                AddTimelineClipCommand *command = new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), item->info(), item->effectList(), true, true);
                m_commandStack->push(command);
                //delete item;
            }
        }
    }
}

void CustomTrackView::setCursorPos(int pos, bool seek) {
    emit cursorMoved((int)(m_cursorPos), (int)(pos));
    m_cursorPos = pos;
    m_cursorLine->setPos(pos, 0);
    if (seek) m_document->renderer()->seek(GenTime(pos, m_document->fps()));
    else if (m_autoScroll) checkScrolling();
}

void CustomTrackView::updateCursorPos() {
    m_cursorLine->setPos(m_cursorPos, 0);
}

int CustomTrackView::cursorPos() {
    return (int)(m_cursorPos);
}

void CustomTrackView::moveCursorPos(int delta) {
    if (m_cursorPos + delta < 0) delta = 0 - m_cursorPos;
    emit cursorMoved((int)(m_cursorPos), (int)((m_cursorPos + delta)));
    m_cursorPos += delta;
    m_cursorLine->setPos(m_cursorPos, 0);
    m_document->renderer()->seek(GenTime(m_cursorPos, m_document->fps()));
    //if (m_autoScroll && m_scale < 50) checkScrolling();
}

void CustomTrackView::checkScrolling() {
    int vert = verticalScrollBar()->value();
    int hor = cursorPos();
    ensureVisible(hor, vert + 10, 2, 2, 50, 0);
    //centerOn(QPointF(cursorPos(), m_tracksHeight));
    /*QRect rectInView = viewport()->rect();
    int delta = rectInView.width() / 3;
    int max = rectInView.right() + horizontalScrollBar()->value() - delta;
    //kDebug() << "CURSOR POS: "<<m_cursorPos<< "Scale: "<<m_scale;
    if (m_cursorPos * m_scale >= max) horizontalScrollBar()->setValue((int)(horizontalScrollBar()->value() + 1 + m_scale));*/
}

void CustomTrackView::mouseReleaseEvent(QMouseEvent * event) {
    QGraphicsView::mouseReleaseEvent(event);
    if (event->button() == Qt::MidButton) {
        return;
    }
    setDragMode(QGraphicsView::NoDrag);
    if (m_operationMode == MOVEGUIDE) {
        setCursor(Qt::ArrowCursor);
        m_operationMode = NONE;
        m_dragGuide->setFlag(QGraphicsItem::ItemIsMovable, false);
        EditGuideCommand *command = new EditGuideCommand(this, m_dragGuide->position(), m_dragGuide->label(), GenTime(m_dragGuide->pos().x(), m_document->fps()), m_dragGuide->label(), false);
        m_commandStack->push(command);
        m_dragGuide->updateGuide(GenTime(m_dragGuide->pos().x(), m_document->fps()));
        m_dragGuide = NULL;
        m_dragItem = NULL;
        return;
    }
    if (m_dragItem == NULL && m_selectionGroup == NULL) {
        emit transitionItemSelected(NULL);
        return;
    }
    ItemInfo info;
    if (m_dragItem) info = m_dragItem->info();

    if (m_operationMode == MOVE) {
        setCursor(Qt::OpenHandCursor);

        if (m_selectionGroup == NULL) {
            // we are moving one clip, easy
            if (m_dragItem->type() == AVWIDGET && (m_dragItemInfo.startPos != info.startPos || m_dragItemInfo.track != info.track)) {
                ClipItem *item = static_cast <ClipItem *>(m_dragItem);
                bool success = m_document->renderer()->mltMoveClip((int)(m_scene->m_tracksList.count() - m_dragItemInfo.track), (int)(m_scene->m_tracksList.count() - m_dragItem->track()), (int) m_dragItemInfo.startPos.frames(m_document->fps()), (int)(m_dragItem->startPos().frames(m_document->fps())), item->baseClip()->producer(info.track));
                if (success) {
                    MoveClipCommand *command = new MoveClipCommand(this, m_dragItemInfo, info, false);
                    m_commandStack->push(command);
                    if (item->baseClip()->isTransparent()) {
                        // Also move automatic transition
                        Transition *tr = getTransitionItemAt((int) m_dragItemInfo.startPos.frames(m_document->fps()) + 1, m_dragItemInfo.track);
                        if (tr && tr->isAutomatic()) {
                            tr->updateTransitionEndTrack(getPreviousVideoTrack(info.track));
                            m_document->renderer()->mltMoveTransition(tr->transitionTag(), m_scene->m_tracksList.count() - m_dragItemInfo.track, m_scene->m_tracksList.count() - info.track, tr->transitionEndTrack(), m_dragItemInfo.startPos, m_dragItemInfo.endPos, info.startPos, info.endPos);
                            tr->setPos((int) info.startPos.frames(m_document->fps()), (int)(info.track * m_tracksHeight + 1));
                        }
                    }
                } else {
                    // undo last move and emit error message
                    MoveClipCommand *command = new MoveClipCommand(this, info, m_dragItemInfo, true);
                    m_commandStack->push(command);
                    emit displayMessage(i18n("Cannot move clip to position %1seconds", QString::number(m_dragItemInfo.startPos.seconds(), 'g', 2)), ErrorMessage);
                }
            }
            if (m_dragItem->type() == TRANSITIONWIDGET && (m_dragItemInfo.startPos != info.startPos || m_dragItemInfo.track != info.track)) {
                MoveTransitionCommand *command = new MoveTransitionCommand(this, m_dragItemInfo, info, false);
                m_commandStack->push(command);
                Transition *transition = (Transition *) m_dragItem;
                transition->updateTransitionEndTrack(getPreviousVideoTrack(m_dragItem->track()));
                m_document->renderer()->mltMoveTransition(transition->transitionTag(), (int)(m_scene->m_tracksList.count() - m_dragItemInfo.track), (int)(m_scene->m_tracksList.count() - m_dragItem->track()), transition->transitionEndTrack(), m_dragItemInfo.startPos, m_dragItemInfo.endPos, info.startPos, info.endPos);
            }
        } else {
            // Moving several clips. We need to delete them and readd them to new position,
            // or they might overlap each other during the move

            QList<QGraphicsItem *> items = m_selectionGroup->childItems();

            GenTime timeOffset = GenTime(m_selectionGroup->scenePos().x(), m_document->fps()) - m_selectionGroupInfo.startPos;
            const int trackOffset = m_selectionGroup->track() - m_selectionGroupInfo.track;
            //kDebug() << "&DROPPED GRPOUP:" << timeOffset.frames(25) << "x" << trackOffset;
            if (timeOffset != GenTime() || trackOffset != 0) {
                QUndoCommand *moveClips = new QUndoCommand();
                moveClips->setText("Move clips");
                // remove items in MLT playlist
                for (int i = 0; i < items.count(); i++) {
                    AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
                    ItemInfo info = item->info();
                    /*info.startPos = info.startPos - timeOffset;
                    info.endPos = info.endPos - timeOffset;
                    info.track = info.track - trackOffset;*/
                    //kDebug() << "REM CLP:" << i << ", START:" << info.startPos.frames(25);
                    if (item->type() == AVWIDGET) {
                        ClipItem *clip = static_cast <ClipItem*>(item);
                        new AddTimelineClipCommand(this, clip->xml(), clip->clipProducer(), info, clip->effectList(), false, true, moveClips);
                        m_document->renderer()->mltRemoveClip(m_scene->m_tracksList.count() - info.track, info.startPos);
                    } else {
                        Transition *tr = static_cast <Transition*>(item);
                        new AddTransitionCommand(this, info, tr->transitionEndTrack(), tr->toXML(), false, true, moveClips);
                        m_document->renderer()->mltDeleteTransition(tr->transitionTag(), tr->transitionEndTrack(), m_scene->m_tracksList.count() - info.track, info.startPos, info.endPos, tr->toXML());
                    }
                }

                for (int i = 0; i < items.count(); i++) {
                    // re-add items in correct place
                    AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
                    ItemInfo info = item->info();
                    info.startPos = info.startPos + timeOffset;
                    info.endPos = info.endPos + timeOffset;
                    info.track = info.track + trackOffset;
                    if (item->type() == AVWIDGET) {
                        ClipItem *clip = static_cast <ClipItem*>(item);
                        new AddTimelineClipCommand(this, clip->xml(), clip->clipProducer(), info, clip->effectList(), false, false, moveClips);
                        info.track = m_scene->m_tracksList.count() - info.track;
                        m_document->renderer()->mltInsertClip(info, clip->xml(), clip->baseClip()->producer(info.track));
                    } else {
                        Transition *tr = static_cast <Transition*>(item);
                        ItemInfo transitionInfo = tr->info();
                        new AddTransitionCommand(this, info, tr->transitionEndTrack(), tr->toXML(), false, false, moveClips);
                        m_document->renderer()->mltAddTransition(tr->transitionTag(), tr->transitionEndTrack() + trackOffset, m_scene->m_tracksList.count() - transitionInfo.track, transitionInfo.startPos, transitionInfo.endPos, tr->toXML());
                    }
                }
                m_commandStack->push(moveClips);
            }
        }

    } else if (m_operationMode == RESIZESTART && m_dragItem->startPos() != m_dragItemInfo.startPos) {
        // resize start
        if (m_dragItem->type() == AVWIDGET) {
            ItemInfo resizeinfo = m_dragItemInfo;
            resizeinfo.track = m_scene->m_tracksList.count() - resizeinfo.track;
            bool success = m_document->renderer()->mltResizeClipStart(resizeinfo, m_dragItem->startPos() - m_dragItemInfo.startPos);
            if (success) {
                updateClipFade((ClipItem *) m_dragItem);
                ResizeClipCommand *command = new ResizeClipCommand(this, m_dragItemInfo, info, false);
                m_commandStack->push(command);
            } else {
                m_dragItem->resizeStart((int) m_dragItemInfo.startPos.frames(m_document->fps()));
                emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
            }
        } else if (m_dragItem->type() == TRANSITIONWIDGET) {
            MoveTransitionCommand *command = new MoveTransitionCommand(this, m_dragItemInfo, info, false);
            m_commandStack->push(command);
            Transition *transition = static_cast <Transition *>(m_dragItem);
            m_document->renderer()->mltMoveTransition(transition->transitionTag(), (int)(m_scene->m_tracksList.count() - m_dragItemInfo.track), (int)(m_scene->m_tracksList.count() - m_dragItemInfo.track), 0, m_dragItemInfo.startPos, m_dragItemInfo.endPos, info.startPos, info.endPos);
        }

        //m_document->renderer()->doRefresh();
    } else if (m_operationMode == RESIZEEND && m_dragItem->endPos() != m_dragItemInfo.endPos) {
        // resize end
        if (m_dragItem->type() == AVWIDGET) {
            ItemInfo resizeinfo = info;
            resizeinfo.track = m_scene->m_tracksList.count() - resizeinfo.track;
            bool success = m_document->renderer()->mltResizeClipEnd(resizeinfo, resizeinfo.endPos - resizeinfo.startPos);
            if (success) {
                ResizeClipCommand *command = new ResizeClipCommand(this, m_dragItemInfo, info, false);
                m_commandStack->push(command);
            } else {
                m_dragItem->resizeEnd((int) m_dragItemInfo.endPos.frames(m_document->fps()));
                emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
            }
        } else if (m_dragItem->type() == TRANSITIONWIDGET) {
            MoveTransitionCommand *command = new MoveTransitionCommand(this, m_dragItemInfo, info, false);
            m_commandStack->push(command);
            Transition *transition = static_cast <Transition *>(m_dragItem);
            m_document->renderer()->mltMoveTransition(transition->transitionTag(), (int)(m_scene->m_tracksList.count() - m_dragItemInfo.track), (int)(m_scene->m_tracksList.count() - m_dragItemInfo.track), 0, m_dragItemInfo.startPos, m_dragItemInfo.endPos, info.startPos, info.endPos);
        }
        //m_document->renderer()->doRefresh();
    } else if (m_operationMode == FADEIN) {
        // resize fade in effect
        ClipItem * item = (ClipItem *) m_dragItem;
        int ix = item->hasEffect("volume", "fadein");
        if (ix != -1) {
            QDomElement oldeffect = item->effectAt(ix);
            int start = item->cropStart().frames(m_document->fps());
            int end = item->fadeIn();
            if (end == 0) {
                slotDeleteEffect(item, oldeffect);
            } else {
                end += start;
                QDomElement effect = oldeffect.cloneNode().toElement();
                EffectsList::setParameter(effect, "in", QString::number(start));
                EffectsList::setParameter(effect, "out", QString::number(end));
                slotUpdateClipEffect(item, oldeffect, effect, ix);
            }
        } else if (item->fadeIn() != 0) {
            QDomElement effect = MainWindow::audioEffects.getEffectByTag("volume", "fadein").cloneNode().toElement();
            EffectsList::setParameter(effect, "out", QString::number(item->fadeIn()));
            slotAddEffect(effect, m_dragItem->startPos(), m_dragItem->track());
        }
    } else if (m_operationMode == FADEOUT) {
        // resize fade in effect
        ClipItem * item = (ClipItem *) m_dragItem;
        int ix = item->hasEffect("volume", "fadeout");
        if (ix != -1) {
            QDomElement oldeffect = item->effectAt(ix);
            int end = (item->duration() + item->cropStart()).frames(m_document->fps());
            int start = item->fadeOut();
            if (start == 0) {
                slotDeleteEffect(item, oldeffect);
            } else {
                start = end - start;
                QDomElement effect = oldeffect.cloneNode().toElement();
                EffectsList::setParameter(effect, "in", QString::number(start));
                EffectsList::setParameter(effect, "out", QString::number(end));
                slotUpdateClipEffect(item, oldeffect, effect, ix);
            }
        } else if (item->fadeOut() != 0) {
            QDomElement effect = MainWindow::audioEffects.getEffectByTag("volume", "fadeout").cloneNode().toElement();
            EffectsList::setParameter(effect, "out", QString::number(item->fadeOut()));
            slotAddEffect(effect, m_dragItem->startPos(), m_dragItem->track());
        }
    } else if (m_operationMode == KEYFRAME) {
        // update the MLT effect
        ClipItem * item = (ClipItem *) m_dragItem;
        QString previous = item->keyframes(item->selectedEffectIndex());
        item->updateKeyframeEffect();
        QString next = item->keyframes(item->selectedEffectIndex());
        EditKeyFrameCommand *command = new EditKeyFrameCommand(this, item->track(), item->startPos(), item->selectedEffectIndex(), previous, next, false);
        m_commandStack->push(command);
        updateEffect(m_scene->m_tracksList.count() - item->track(), item->startPos(), item->selectedEffect(), item->selectedEffectIndex());
    }

    emit transitionItemSelected((m_dragItem && m_dragItem->type() == TRANSITIONWIDGET) ? static_cast <Transition *>(m_dragItem) : NULL);
    m_document->setModified(true);
    m_operationMode = NONE;
}

void CustomTrackView::deleteClip(ItemInfo info) {
    ClipItem *item = getClipItemAt((int) info.startPos.frames(m_document->fps()) + 1, info.track);
    if (!item) {
        kDebug() << "----------------  ERROR, CANNOT find clip to delete at...";// << rect.x();
        return;
    }
    if (item->isSelected()) emit clipItemSelected(NULL);
    item->baseClip()->removeReference();
    m_document->updateClip(item->baseClip()->getId());

    if (item->baseClip()->isTransparent()) {
        // also remove automatic transition
        Transition *tr = getTransitionItemAt((int) info.startPos.frames(m_document->fps()) + 1, info.track);
        if (tr && tr->isAutomatic()) {
            m_document->renderer()->mltDeleteTransition(tr->transitionTag(), tr->transitionEndTrack(), m_scene->m_tracksList.count() - info.track, info.startPos, info.endPos, tr->toXML());
            scene()->removeItem(tr);
            delete tr;
        }
    }
    scene()->removeItem(item);
    delete item;
    m_document->renderer()->mltRemoveClip(m_scene->m_tracksList.count() - info.track, info.startPos);
    m_document->renderer()->doRefresh();
}

void CustomTrackView::deleteSelectedClips() {
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    if (itemList.count() == 0) {
        emit displayMessage(i18n("Select clip to delete"), ErrorMessage);
        return;
    }
    QUndoCommand *deleteSelected = new QUndoCommand();
    deleteSelected->setText(i18n("Delete selected items"));
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), item->info(), item->effectList(), true, true, deleteSelected);
        } else if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            Transition *item = static_cast <Transition *>(itemList.at(i));
            ItemInfo info;
            info.startPos = item->startPos();
            info.endPos = item->endPos();
            info.track = item->track();
            new AddTransitionCommand(this, info, item->transitionEndTrack(), item->toXML(), true, true, deleteSelected);
        }
    }
    m_commandStack->push(deleteSelected);
}

void CustomTrackView::changeClipSpeed() {
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    if (itemList.count() == 0) {
        emit displayMessage(i18n("Select clip to change speed"), ErrorMessage);
        return;
    }
    QUndoCommand *changeSelected = new QUndoCommand();
    changeSelected->setText("Edit clip speed");
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            ItemInfo info = item->info();
            int percent = QInputDialog::getInteger(this, i18n("Edit Clip Speed"), i18n("New speed (percents)"), item->speed() * 100, 1, 300);
            double speed = (double) percent / 100.0;
            if (item->speed() != speed)
                new ChangeSpeedCommand(this, info, item->speed(), speed, item->clipProducer(), true, changeSelected);
        }
    }
    m_commandStack->push(changeSelected);
}

void CustomTrackView::doChangeClipSpeed(ItemInfo info, const double speed, const double oldspeed, const QString &id) {

    DocClipBase *baseclip = m_document->clipManager()->getClipById(id);
    ClipItem *item = getClipItemAt((int) info.startPos.frames(m_document->fps()) + 1, info.track);
    info.track = m_scene->m_tracksList.count() - item->track();
    int endPos = m_document->renderer()->mltChangeClipSpeed(info, speed, oldspeed, baseclip->producer());
    //kDebug() << "//CH CLIP SPEED: " << speed << "x" << oldspeed << ", END POS: " << endPos;
    item->setSpeed(speed);
    item->updateRectGeometry();
    if (item->cropDuration().frames(m_document->fps()) > endPos)
        item->AbstractClipItem::resizeEnd(info.startPos.frames(m_document->fps()) + endPos, speed);
}

void CustomTrackView::cutSelectedClips() {
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    GenTime currentPos = GenTime(m_cursorPos, m_document->fps());
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            if (currentPos > item->startPos() && currentPos <  item->endPos()) {
                RazorClipCommand *command = new RazorClipCommand(this, item->info(), currentPos, true);
                m_commandStack->push(command);
            }
        }
    }
}

void CustomTrackView::addClip(QDomElement xml, const QString &clipId, ItemInfo info, EffectsList effects) {
    DocClipBase *baseclip = m_document->clipManager()->getClipById(clipId);
    if (baseclip == NULL) {
        emit displayMessage(i18n("No clip copied"), ErrorMessage);
        return;
    }
    ClipItem *item = new ClipItem(baseclip, info, m_document->fps());
    item->setEffectList(effects);
    scene()->addItem(item);
    if (item->baseClip()->isTransparent()) {
        // add transparency transition
        int endTrack = getPreviousVideoTrack(info.track);
        Transition *tr = new Transition(info, endTrack, m_document->fps(), MainWindow::transitions.getEffectByTag("composite", "alphatransparency"), true);
        scene()->addItem(tr);
        m_document->renderer()->mltAddTransition(tr->transitionTag(), endTrack, m_scene->m_tracksList.count() - info.track, info.startPos, info.endPos, tr->toXML());
    }

    baseclip->addReference();
    m_document->updateClip(baseclip->getId());
    info.track = m_scene->m_tracksList.count() - info.track;
    m_document->renderer()->mltInsertClip(info, xml, baseclip->producer(info.track));
    for (int i = 0; i < item->effectsCount(); i++) {
        m_document->renderer()->mltAddEffect(info.track, info.startPos, item->getEffectArgs(item->effectAt(i)), false);
    }
    m_document->renderer()->doRefresh();
}

void CustomTrackView::slotUpdateClip(const QString &clipId) {
    QList<QGraphicsItem *> list = scene()->items();
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->type() == AVWIDGET) {
            clip = static_cast <ClipItem *>(list.at(i));
            if (clip->clipProducer() == clipId) {
                clip->refreshClip();
                ItemInfo info = clip->info();
                info.track = m_scene->m_tracksList.count() - clip->track();
                m_document->renderer()->mltUpdateClip(info, clip->xml(), clip->baseClip()->producer());
            }
        }
    }
}

ClipItem *CustomTrackView::getClipItemAt(int pos, int track) {
    QList<QGraphicsItem *> list = scene()->items(QPointF(pos , track * m_tracksHeight + m_tracksHeight / 2));
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->type() == AVWIDGET) {
            clip = static_cast <ClipItem *>(list.at(i));
            break;
        }
    }
    return clip;
}

ClipItem *CustomTrackView::getClipItemAt(GenTime pos, int track) {
    int framepos = (int)(pos.frames(m_document->fps()));
    return getClipItemAt(framepos, track);
}

Transition *CustomTrackView::getTransitionItemAt(int pos, int track) {
    QList<QGraphicsItem *> list = scene()->items(QPointF(pos, (track + 1) * m_tracksHeight));
    Transition *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->type() == TRANSITIONWIDGET) {
            clip = static_cast <Transition *>(list.at(i));
            break;
        }
    }
    return clip;
}

Transition *CustomTrackView::getTransitionItemAt(GenTime pos, int track) {
    int framepos = (int)(pos.frames(m_document->fps()));
    return getTransitionItemAt(framepos, track);
}

void CustomTrackView::moveClip(const ItemInfo start, const ItemInfo end) {
    ClipItem *item = getClipItemAt((int) start.startPos.frames(m_document->fps()) + 1, start.track);
    if (!item) {
        emit displayMessage(i18n("Cannot move clip at time: %1s on track %2", QString::number(start.startPos.seconds(), 'g', 2), start.track), ErrorMessage);
        kDebug() << "----------------  ERROR, CANNOT find clip to move at.. ";// << startPos.x() * m_scale * FRAME_SIZE + 1 << ", " << startPos.y() * m_tracksHeight + m_tracksHeight / 2;
        return;
    }
    //kDebug() << "----------------  Move CLIP FROM: " << startPos.x() << ", END:" << endPos.x() << ",TRACKS: " << startPos.y() << " TO " << endPos.y();

    bool success = m_document->renderer()->mltMoveClip((int)(m_scene->m_tracksList.count() - start.track), (int)(m_scene->m_tracksList.count() - end.track), (int) start.startPos.frames(m_document->fps()), (int)end.startPos.frames(m_document->fps()), item->baseClip()->producer(end.track));
    if (success) {
        item->setPos((int) end.startPos.frames(m_document->fps()), (int)(end.track * m_tracksHeight + 1));
        if (item->baseClip()->isTransparent()) {
            // Also move automatic transition
            Transition *tr = getTransitionItemAt((int) start.startPos.frames(m_document->fps()) + 1, start.track);
            if (tr && tr->isAutomatic()) {
                tr->updateTransitionEndTrack(getPreviousVideoTrack(end.track));
                m_document->renderer()->mltMoveTransition(tr->transitionTag(), m_scene->m_tracksList.count() - start.track, m_scene->m_tracksList.count() - end.track, tr->transitionEndTrack(), start.startPos, start.endPos, end.startPos, end.endPos);
                tr->setPos((int) end.startPos.frames(m_document->fps()), (int)(end.track * m_tracksHeight + 1));
            }
        }
    } else {
        // undo last move and emit error message
        emit displayMessage(i18n("Cannot move clip to position %1seconds", QString::number(end.startPos.seconds(), 'g', 2)), ErrorMessage);
    }
}

void CustomTrackView::moveTransition(const ItemInfo start, const ItemInfo end) {
    Transition *item = getTransitionItemAt((int)start.startPos.frames(m_document->fps()) + 1, start.track);
    if (!item) {
        emit displayMessage(i18n("Cannot move transition at time: %1s on track %2", start.startPos.seconds(), start.track), ErrorMessage);
        kDebug() << "----------------  ERROR, CANNOT find transition to move... ";// << startPos.x() * m_scale * FRAME_SIZE + 1 << ", " << startPos.y() * m_tracksHeight + m_tracksHeight / 2;
        return;
    }
    //kDebug() << "----------------  Move TRANSITION FROM: " << startPos.x() << ", END:" << endPos.x() << ",TRACKS: " << oldtrack << " TO " << newtrack;

    //kDebug()<<"///  RESIZE TRANS START: ("<< startPos.x()<<"x"<< startPos.y()<<") / ("<<endPos.x()<<"x"<< endPos.y()<<")";
    if (end.endPos - end.startPos == start.endPos - start.startPos) {
        // Transition was moved
        item->setPos((int) end.startPos.frames(m_document->fps()), (end.track) * m_tracksHeight + 1);
    } else if (end.endPos == start.endPos) {
        // Transition start resize
        item->resizeStart((int) end.startPos.frames(m_document->fps()));
    } else {
        // Transition end resize;
        item->resizeEnd((int) end.endPos.frames(m_document->fps()));
    }
    //item->moveTransition(GenTime((int) (endPos.x() - startPos.x()), m_document->fps()));
    item->updateTransitionEndTrack(getPreviousVideoTrack(end.track));
    m_document->renderer()->mltMoveTransition(item->transitionTag(), m_scene->m_tracksList.count() - start.track, m_scene->m_tracksList.count() - end.track, item->transitionEndTrack(), start.startPos, start.endPos, end.startPos, end.endPos);
}

void CustomTrackView::resizeClip(const ItemInfo start, const ItemInfo end) {
    int offset = 0;
    bool resizeClipStart = true;
    if (start.startPos == end.startPos) resizeClipStart = false;
    /*if (resizeClipStart) offset = 1;
    else offset = -1;*/
    ClipItem *item = getClipItemAt((int)(start.startPos.frames(m_document->fps()) + offset), start.track);
    if (!item) {
        emit displayMessage(i18n("Cannot move clip at time: %1s on track %2", start.startPos.seconds(), start.track), ErrorMessage);
        kDebug() << "----------------  ERROR, CANNOT find clip to resize at... "; // << startPos;
        return;
    }
    if (resizeClipStart) {
        ItemInfo clipinfo = item->info();
        clipinfo.track = m_scene->m_tracksList.count() - clipinfo.track;
        bool success = m_document->renderer()->mltResizeClipStart(clipinfo, end.startPos - item->startPos());
        if (success) {
            item->resizeStart((int) end.startPos.frames(m_document->fps()));
            updateClipFade(item);
        } else emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
    } else {
        ItemInfo clipinfo = item->info();
        clipinfo.track = m_scene->m_tracksList.count() - clipinfo.track;
        bool success = m_document->renderer()->mltResizeClipEnd(clipinfo, end.endPos - clipinfo.startPos);
        if (success) {
            item->resizeEnd((int) end.endPos.frames(m_document->fps()));
            updateClipFade(item, true);
        } else emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
    }
    m_document->renderer()->doRefresh();
}

void CustomTrackView::updateClipFade(ClipItem * item, bool updateFadeOut) {
    if (!updateFadeOut) {
        int end = item->fadeIn();
        if (end != 0) {
            // there is a fade in effect
            QStringList clipeffects = item->effectNames();
            int effectPos = item->hasEffect("volume", "fadein");
            QDomElement oldeffect = item->effectAt(effectPos);
            int start = item->cropStart().frames(m_document->fps());
            end += start;
            EffectsList::setParameter(oldeffect, "in", QString::number(start));
            EffectsList::setParameter(oldeffect, "out", QString::number(end));
            QHash <QString, QString> effectParams = item->getEffectArgs(oldeffect);
            if (!m_document->renderer()->mltEditEffect(m_scene->m_tracksList.count() - item->track(), item->startPos(), effectParams))
                emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
        }
    } else {
        int start = item->fadeOut();
        if (start != 0) {
            // there is a fade in effect
            QStringList clipeffects = item->effectNames();
            int effectPos = item->hasEffect("volume", "fadeout");
            QDomElement oldeffect = item->effectAt(effectPos);
            int end = (item->duration() - item->cropStart()).frames(m_document->fps());
            start = end - start;
            EffectsList::setParameter(oldeffect, "in", QString::number(start));
            EffectsList::setParameter(oldeffect, "out", QString::number(end));
            QHash <QString, QString> effectParams = item->getEffectArgs(oldeffect);
            if (m_document->renderer()->mltEditEffect(m_scene->m_tracksList.count() - item->track(), item->startPos(), effectParams))
                emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
        }
    }
}

double CustomTrackView::getSnapPointForPos(double pos) {
    return m_scene->getSnapPointForPos(pos);
}

void CustomTrackView::updateSnapPoints(AbstractClipItem *selected) {
    QList <GenTime> snaps;
    GenTime offset;
    if (selected) offset = selected->duration();
    QList<QGraphicsItem *> itemList = items();
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET && itemList.at(i) != selected) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            GenTime start = item->startPos();
            GenTime end = item->endPos();
            snaps.append(start);
            snaps.append(end);
            QList < GenTime > markers = item->snapMarkers();
            for (int i = 0; i < markers.size(); ++i) {
                GenTime t = markers.at(i);
                snaps.append(t);
                if (t > offset) snaps.append(t - offset);
            }
            if (offset != GenTime()) {
                if (start > offset) snaps.append(start - offset);
                if (end > offset) snaps.append(end - offset);
            }
        } else if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            Transition *transition = static_cast <Transition*>(itemList.at(i));
            GenTime start = transition->startPos();
            GenTime end = transition->endPos();
            snaps.append(start);
            snaps.append(end);
            if (offset != GenTime()) {
                if (start > offset) snaps.append(start - offset);
                if (end > offset) snaps.append(end - offset);
            }
        }
    }

    // add cursor position
    GenTime pos = GenTime(m_cursorPos, m_document->fps());
    snaps.append(pos);
    if (offset != GenTime()) snaps.append(pos - offset);

    // add guides
    for (int i = 0; i < m_guides.count(); i++) {
        snaps.append(m_guides.at(i)->position());
        if (offset != GenTime()) snaps.append(m_guides.at(i)->position() - offset);
    }

    qSort(snaps);
    m_scene->setSnapList(snaps);
    //for (int i = 0; i < m_snapPoints.size(); ++i)
    //    kDebug() << "SNAP POINT: " << m_snapPoints.at(i).frames(25);
}

void CustomTrackView::slotSeekToPreviousSnap() {
    updateSnapPoints(NULL);
    GenTime res = m_scene->previousSnapPoint(GenTime(m_cursorPos, m_document->fps()));
    setCursorPos((int) res.frames(m_document->fps()));
    checkScrolling();
}

void CustomTrackView::slotSeekToNextSnap() {
    updateSnapPoints(NULL);
    GenTime res = m_scene->nextSnapPoint(GenTime(m_cursorPos, m_document->fps()));
    setCursorPos((int) res.frames(m_document->fps()));
    checkScrolling();
}

void CustomTrackView::clipStart() {
    ClipItem *item = getMainActiveClip();
    if (item != NULL) {
        setCursorPos((int) item->startPos().frames(m_document->fps()));
        checkScrolling();
    }
}

void CustomTrackView::clipEnd() {
    ClipItem *item = getMainActiveClip();
    if (item != NULL) {
        setCursorPos((int) item->endPos().frames(m_document->fps()));
        checkScrolling();
    }
}

void CustomTrackView::slotAddClipMarker() {
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    if (itemList.count() != 1) {
        emit displayMessage(i18n("Cannot add marker if more than one clip is selected"), ErrorMessage);
        kDebug() << "// CANNOT ADD MARKER IF MORE TAN ONE CLIP IS SELECTED....";
        return;
    }
    AbstractClipItem *item = (AbstractClipItem *)itemList.at(0);
    if (item->type() != AVWIDGET) return;
    GenTime pos = GenTime(m_cursorPos, m_document->fps());
    if (item->startPos() > pos || item->endPos() < pos) return;
    ClipItem *clip = (ClipItem *) item;
    QString id = clip->baseClip()->getId();
    GenTime position = pos - item->startPos() + item->cropStart();
    CommentedTime marker(position, i18n("Marker"));
    MarkerDialog d(clip->baseClip(), marker, m_document->timecode(), this);
    if (d.exec() == QDialog::Accepted) {
        slotAddClipMarker(id, d.newMarker().time(), d.newMarker().comment());
    }
}

void CustomTrackView::slotAddClipMarker(const QString &id, GenTime t, QString c) {
    QString oldcomment = m_document->clipManager()->getClipById(id)->markerComment(t);
    AddMarkerCommand *command = new AddMarkerCommand(this, oldcomment, c, id, t, true);
    m_commandStack->push(command);
}

void CustomTrackView::slotDeleteClipMarker() {
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    if (itemList.count() != 1) {
        emit displayMessage(i18n("Cannot delete marker if more than one clip is selected"), ErrorMessage);
        kDebug() << "// CANNOT DELETE MARKER IF MORE TAN ONE CLIP IS SELECTED....";
        return;
    }
    AbstractClipItem *item = (AbstractClipItem *)itemList.at(0);
    if (item->type() != AVWIDGET) {
        emit displayMessage(i18n("No clip selected"), ErrorMessage);
        return;
    }
    GenTime pos = GenTime(m_cursorPos, m_document->fps());
    if (item->startPos() > pos || item->endPos() < pos) {
        emit displayMessage(i18n("No selected clip at cursor time"), ErrorMessage);
        return;
    }
    ClipItem *clip = (ClipItem *) item;
    QString id = clip->baseClip()->getId();
    GenTime position = pos - item->startPos() + item->cropStart();
    QString comment = clip->baseClip()->markerComment(position);
    if (comment.isEmpty()) {
        emit displayMessage(i18n("No marker found at cursor time"), ErrorMessage);
        return;
    }
    AddMarkerCommand *command = new AddMarkerCommand(this, comment, QString(), id, position, true);
    m_commandStack->push(command);
}

void CustomTrackView::slotDeleteAllClipMarkers() {
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    if (itemList.count() != 1) {
        emit displayMessage(i18n("Cannot delete marker if more than one clip is selected"), ErrorMessage);
        kDebug() << "// CANNOT DELETE MARKER IF MORE TAN ONE CLIP IS SELECTED....";
        return;
    }
    AbstractClipItem *item = (AbstractClipItem *)itemList.at(0);
    if (item->type() != AVWIDGET) {
        emit displayMessage(i18n("No clip selected"), ErrorMessage);
        return;
    }

    ClipItem *clip = static_cast <ClipItem *>(item);
    QList <CommentedTime> markers = clip->baseClip()->commentedSnapMarkers();

    if (markers.isEmpty()) {
        emit displayMessage(i18n("Clip has no markers"), ErrorMessage);
        return;
    }
    QString id = clip->baseClip()->getId();
    QUndoCommand *deleteMarkers = new QUndoCommand();
    deleteMarkers->setText("Delete clip markers");

    for (int i = 0; i < markers.size(); i++) {
        new AddMarkerCommand(this, markers.at(i).comment(), QString(), id, markers.at(i).time(), true, deleteMarkers);
    }
    m_commandStack->push(deleteMarkers);
}

void CustomTrackView::slotEditClipMarker() {
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    if (itemList.count() != 1) {
        emit displayMessage(i18n("Cannot edit marker if more than one clip is selected"), ErrorMessage);
        kDebug() << "// CANNOT DELETE MARKER IF MORE TAN ONE CLIP IS SELECTED....";
        return;
    }
    AbstractClipItem *item = (AbstractClipItem *)itemList.at(0);
    if (item->type() != AVWIDGET) {
        emit displayMessage(i18n("No clip at cursor time"), ErrorMessage);
        return;
    }
    GenTime pos = GenTime(m_cursorPos, m_document->fps());
    if (item->startPos() > pos || item->endPos() < pos) {
        emit displayMessage(i18n("No selected clip at cursor time"), ErrorMessage);
        return;
    }
    ClipItem *clip = (ClipItem *) item;
    QString id = clip->baseClip()->getId();
    GenTime position = pos - item->startPos() + item->cropStart();
    QString oldcomment = clip->baseClip()->markerComment(position);
    if (oldcomment.isEmpty()) {
        emit displayMessage(i18n("No marker found at cursor time"), ErrorMessage);
        return;
    }

    CommentedTime marker(position, oldcomment);
    MarkerDialog d(clip->baseClip(), marker, m_document->timecode(), this);
    if (d.exec() == QDialog::Accepted) {
        if (d.newMarker().time() == position) {
            // marker position was not changed, only text
            AddMarkerCommand *command = new AddMarkerCommand(this, oldcomment, d.newMarker().comment(), id, position, true);
            m_commandStack->push(command);
        } else {
            // marker text and position were changed, remove previous marker and add new one
            AddMarkerCommand *command1 = new AddMarkerCommand(this, oldcomment, QString(), id, position, true);
            AddMarkerCommand *command2 = new AddMarkerCommand(this, QString(), d.newMarker().comment(), id, d.newMarker().time(), true);
            m_commandStack->push(command1);
            m_commandStack->push(command2);
        }
    }
}

void CustomTrackView::addMarker(const QString &id, const GenTime &pos, const QString comment) {
    DocClipBase *base = m_document->clipManager()->getClipById(id);
    if (!comment.isEmpty()) base->addSnapMarker(pos, comment);
    else base->deleteSnapMarker(pos);
    m_document->setModified(true);
    viewport()->update();
}

bool sortGuidesList(const Guide *g1 , const Guide *g2) {
    return (*g1).position() < (*g2).position();
}

void CustomTrackView::editGuide(const GenTime oldPos, const GenTime pos, const QString &comment) {
    if (oldPos > GenTime() && pos > GenTime()) {
        // move guide
        for (int i = 0; i < m_guides.count(); i++) {
            if (m_guides.at(i)->position() == oldPos) {
                Guide *item = m_guides.at(i);
                item->updateGuide(pos, comment);
                break;
            }
        }
    } else if (pos > GenTime()) addGuide(pos, comment);
    else {
        // remove guide
        bool found = false;
        for (int i = 0; i < m_guides.count(); i++) {
            if (m_guides.at(i)->position() == oldPos) {
                Guide *item = m_guides.takeAt(i);
                delete item;
                found = true;
                break;
            }
        }
        if (!found) emit displayMessage(i18n("No guide at cursor time"), ErrorMessage);
    }
    qSort(m_guides.begin(), m_guides.end(), sortGuidesList);
    m_document->syncGuides(m_guides);
}

bool CustomTrackView::addGuide(const GenTime pos, const QString &comment) {
    for (int i = 0; i < m_guides.count(); i++) {
        if (m_guides.at(i)->position() == pos) {
            emit displayMessage(i18n("A guide already exists at that position"), ErrorMessage);
            return false;
        }
    }
    Guide *g = new Guide(this, pos, comment, m_document->fps(), m_tracksHeight * m_scene->m_tracksList.count());
    scene()->addItem(g);
    m_guides.append(g);
    qSort(m_guides.begin(), m_guides.end(), sortGuidesList);
    m_document->syncGuides(m_guides);
    return true;
}

void CustomTrackView::slotAddGuide() {
    CommentedTime marker(GenTime(m_cursorPos, m_document->fps()), i18n("Guide"));
    MarkerDialog d(NULL, marker, m_document->timecode(), this);
    if (d.exec() != QDialog::Accepted) return;
    if (addGuide(d.newMarker().time(), d.newMarker().comment())) {
        EditGuideCommand *command = new EditGuideCommand(this, GenTime(), QString(), d.newMarker().time(), d.newMarker().comment(), false);
        m_commandStack->push(command);
    }
}

void CustomTrackView::slotEditGuide() {
    GenTime pos = GenTime(m_cursorPos, m_document->fps());
    bool found = false;
    for (int i = 0; i < m_guides.count(); i++) {
        if (m_guides.at(i)->position() == pos) {
            slotEditGuide(m_guides.at(i)->info());
            found = true;
            break;
        }
    }
    if (!found) emit displayMessage(i18n("No guide at cursor time"), ErrorMessage);
}

void CustomTrackView::slotEditGuide(CommentedTime guide) {
    MarkerDialog d(NULL, guide, m_document->timecode(), this);
    if (d.exec() == QDialog::Accepted) {
        EditGuideCommand *command = new EditGuideCommand(this, guide.time(), guide.comment(), d.newMarker().time(), d.newMarker().comment(), true);
        m_commandStack->push(command);
    }
}


void CustomTrackView::slotDeleteGuide() {
    GenTime pos = GenTime(m_cursorPos, m_document->fps());
    bool found = false;
    for (int i = 0; i < m_guides.count(); i++) {
        if (m_guides.at(i)->position() == pos) {
            EditGuideCommand *command = new EditGuideCommand(this, m_guides.at(i)->position(), m_guides.at(i)->label(), GenTime(), QString(), true);
            m_commandStack->push(command);
            found = true;
            break;
        }
    }
    if (!found) emit displayMessage(i18n("No guide at cursor time"), ErrorMessage);
}

void CustomTrackView::slotDeleteAllGuides() {
    QUndoCommand *deleteAll = new QUndoCommand();
    deleteAll->setText("Delete all guides");
    for (int i = 0; i < m_guides.count(); i++) {
        EditGuideCommand *command = new EditGuideCommand(this, m_guides.at(i)->position(), m_guides.at(i)->label(), GenTime(), QString(), true, deleteAll);
    }
    m_commandStack->push(deleteAll);
}

void CustomTrackView::setTool(PROJECTTOOL tool) {
    m_tool = tool;
}

void CustomTrackView::setScale(double scaleFactor) {
    QMatrix matrix;
    matrix = matrix.scale(scaleFactor, 1);
    m_scene->setScale(scaleFactor);
    //scale(scaleFactor, 1);
    m_animationTimer->stop();
    if (m_visualTip) {
        delete m_visualTip;
        m_visualTip = NULL;
    }
    if (m_animation) {
        delete m_animation;
        m_animation = NULL;
    }
    /*double pos = cursorPos() / m_scale;
    m_scale = scaleFactor;
    m_scene->setScale(m_scale);
    int vert = verticalScrollBar()->value();
    kDebug() << " HHHHHHHH  SCALING: " << m_scale;
    QList<QGraphicsItem *> itemList = items();
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET || itemList.at(i)->type() == TRANSITIONWIDGET) {
            AbstractClipItem *clip = (AbstractClipItem *)itemList.at(i);
            clip->setRect(0, 0, (qreal) clip->duration().frames(m_document->fps()) * m_scale - .5, clip->rect().height());
            clip->setPos((qreal) clip->startPos().frames(m_document->fps()) * m_scale, clip->pos().y());
        }
    }

    for (int i = 0; i < m_guides.count(); i++) {
        m_guides.at(i)->updatePosition(m_scale);
    }

    setSceneRect(0, 0, (m_projectDuration + 100) * m_scale, sceneRect().height());
    updateCursorPos();*/
    setMatrix(matrix);
    centerOn(QPointF(cursorPos(), m_tracksHeight));
    //verticalScrollBar()->setValue(vert);*/
}

void CustomTrackView::slotRefreshGuides() {
    if (KdenliveSettings::showmarkers()) {
        kDebug() << "// refresh GUIDES";
        for (int i = 0; i < m_guides.count(); i++) {
            m_guides.at(i)->update();
        }
    }
}

void CustomTrackView::drawBackground(QPainter * painter, const QRectF & rect) {
    QColor base = palette().button().color();
    QRectF r = rect;
    r.setWidth(r.width() + 1);
    painter->setClipRect(r);
    painter->drawLine(r.left(), 0, r.right(), 0);
    uint max = m_scene->m_tracksList.count();
    for (uint i = 0; i < max;i++) {
        /*if (max - i - 1 == m_selectedTrack) painter->fillRect(r.left(), m_tracksHeight * i + 1, r.right() - r.left() + 1, m_tracksHeight - 1, QBrush(QColor(211, 205, 147)));
               else*/
        if (m_scene->m_tracksList.at(max - i - 1).type == AUDIOTRACK) painter->fillRect(r.left(), m_tracksHeight * i + 1, r.right() - r.left() + 1, m_tracksHeight - 1, QBrush(QColor(240, 240, 255)));
        painter->drawLine(r.left(), m_tracksHeight * (i + 1), r.right(), m_tracksHeight * (i + 1));
    }
    int lowerLimit = m_tracksHeight * m_scene->m_tracksList.count() + 1;
    if (height() > lowerLimit)
        painter->fillRect(QRectF(r.left(), lowerLimit, r.width(), height() - lowerLimit), QBrush(base));
}

bool CustomTrackView::findString(const QString &text) {
    QString marker;
    for (int i = 0; i < m_searchPoints.size(); ++i) {
        marker = m_searchPoints.at(i).comment();
        if (marker.contains(text, Qt::CaseInsensitive)) {
            setCursorPos(m_searchPoints.at(i).time().frames(m_document->fps()), true);
            int vert = verticalScrollBar()->value();
            int hor = cursorPos();
            ensureVisible(hor, vert + 10, 2, 2, 50, 0);
            m_findIndex = i;
            return true;
        }
    }
    return false;
}

bool CustomTrackView::findNextString(const QString &text) {
    QString marker;
    for (int i = m_findIndex + 1; i < m_searchPoints.size(); ++i) {
        marker = m_searchPoints.at(i).comment();
        if (marker.contains(text, Qt::CaseInsensitive)) {
            setCursorPos(m_searchPoints.at(i).time().frames(m_document->fps()), true);
            int vert = verticalScrollBar()->value();
            int hor = cursorPos();
            ensureVisible(hor, vert + 10, 2, 2, 50, 0);
            m_findIndex = i;
            return true;
        }
    }
    m_findIndex = -1;
    return false;
}

void CustomTrackView::initSearchStrings() {
    m_searchPoints.clear();
    QList<QGraphicsItem *> itemList = items();
    for (int i = 0; i < itemList.count(); i++) {
        // parse all clip names
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            GenTime start = item->startPos();
            CommentedTime t(start, item->clipName());
            m_searchPoints.append(t);
            // add all clip markers
            QList < CommentedTime > markers = item->commentedSnapMarkers();
            m_searchPoints += markers;
        }
    }

    // add guides
    for (int i = 0; i < m_guides.count(); i++) {
        m_searchPoints.append(m_guides.at(i)->info());
    }

    qSort(m_searchPoints);
}

void CustomTrackView::clearSearchStrings() {
    m_searchPoints.clear();
    m_findIndex = 0;
}

void CustomTrackView::copyClip() {
    while (m_copiedItems.count() > 0) {
        delete m_copiedItems.takeFirst();
    }
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    if (itemList.count() == 0) {
        emit displayMessage(i18n("Select a clip before copying"), ErrorMessage);
        return;
    }
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *dup = static_cast <ClipItem *>(itemList.at(i));
            m_copiedItems.append(dup->clone(dup->info()));
        } else if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            Transition *dup = static_cast <Transition *>(itemList.at(i));
            m_copiedItems.append(dup->clone());
        }
    }
}

bool CustomTrackView::canBePastedTo(ItemInfo info, int type) const {
    QRectF rect((double) info.startPos.frames(m_document->fps()), (double)(info.track * m_tracksHeight + 1), (double)(info.endPos - info.startPos).frames(m_document->fps()), (double)(m_tracksHeight - 1));
    QList<QGraphicsItem *> collisions = scene()->items(rect, Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisions.count(); i++) {
        if (collisions.at(i)->type() == type) return false;
    }
    return true;
}

bool CustomTrackView::canBePasted(QList<AbstractClipItem *> items, GenTime offset, int trackOffset) const {
    for (int i = 0; i < items.count(); i++) {
        ItemInfo info = items.at(i)->info();
        info.startPos += offset;
        info.endPos += offset;
        info.track += trackOffset;
        if (!canBePastedTo(info, items.at(i)->type())) return false;
    }
    return true;
}

bool CustomTrackView::canBeMoved(QList<AbstractClipItem *> items, GenTime offset, int trackOffset) const {
    QPainterPath movePath;
    movePath.moveTo(0, 0);

    for (int i = 0; i < items.count(); i++) {
        ItemInfo info = items.at(i)->info();
        info.startPos = info.startPos + offset;
        info.endPos = info.endPos + offset;
        info.track = info.track + trackOffset;
        if (info.startPos < GenTime()) {
            // No clip should go below 0
            return false;
        }
        QRectF rect((double) info.startPos.frames(m_document->fps()), (double)(info.track * m_tracksHeight + 1), (double)(info.endPos - info.startPos).frames(m_document->fps()), (double)(m_tracksHeight - 1));
        movePath.addRect(rect);
    }
    QList<QGraphicsItem *> collisions = scene()->items(movePath, Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisions.count(); i++) {
        if ((collisions.at(i)->type() == AVWIDGET || collisions.at(i)->type() == TRANSITIONWIDGET) && !items.contains(static_cast <AbstractClipItem *>(collisions.at(i)))) {
            kDebug() << "  ////////////   CLIP COLLISION, MOVE NOT ALLOWED";
            return false;
        }
    }
    return true;
}

void CustomTrackView::pasteClip() {
    if (m_copiedItems.count() == 0) {
        emit displayMessage(i18n("No clip copied"), ErrorMessage);
        return;
    }
    QPoint position;
    if (m_menuPosition.isNull()) position = mapFromGlobal(QCursor::pos());
    else position = m_menuPosition;
    GenTime pos = GenTime((int)(mapToScene(position).x()), m_document->fps());
    int track = (int)(position.y() / m_tracksHeight);
    ItemInfo first = m_copiedItems.at(0)->info();

    GenTime offset = pos - first.startPos;
    int trackOffset = track - first.track;

    if (!canBePasted(m_copiedItems, offset, trackOffset)) {
        emit displayMessage(i18n("Cannot paste selected clips"), ErrorMessage);
        return;
    }
    QUndoCommand *pasteClips = new QUndoCommand();
    pasteClips->setText("Paste clips");

    for (int i = 0; i < m_copiedItems.count(); i++) {
        // parse all clip names
        if (m_copiedItems.at(i) && m_copiedItems.at(i)->type() == AVWIDGET) {
            ClipItem *clip = static_cast <ClipItem *>(m_copiedItems.at(i));
            ItemInfo info;
            info.startPos = clip->startPos() + offset;
            info.endPos = clip->endPos() + offset;
            info.cropStart = clip->cropStart();
            info.track = clip->track() + trackOffset;
            if (canBePastedTo(info, AVWIDGET)) {
                new AddTimelineClipCommand(this, clip->xml(), clip->clipProducer(), info, clip->effectList(), true, false, pasteClips);
            } else emit displayMessage(i18n("Cannot paste clip to selected place"), ErrorMessage);
        } else if (m_copiedItems.at(i) && m_copiedItems.at(i)->type() == TRANSITIONWIDGET) {
            Transition *tr = static_cast <Transition *>(m_copiedItems.at(i));
            ItemInfo info;
            info.startPos = tr->startPos() + offset;
            info.endPos = tr->endPos() + offset;
            info.track = tr->track() + trackOffset;
            if (canBePastedTo(info, TRANSITIONWIDGET)) {
                new AddTransitionCommand(this, info, tr->transitionEndTrack() + trackOffset, tr->toXML(), false, true, pasteClips);
            } else emit displayMessage(i18n("Cannot paste transition to selected place"), ErrorMessage);
        }
    }
    m_commandStack->push(pasteClips);
}

void CustomTrackView::pasteClipEffects() {
    if (m_copiedItems.count() != 1 || m_copiedItems.at(0)->type() != AVWIDGET) {
        emit displayMessage(i18n("You must copy exactly one clip before pasting effects"), ErrorMessage);
        return;
    }
    ClipItem *clip = static_cast < ClipItem *>(m_copiedItems.at(0));
    EffectsList effects = clip->effectList();

    QUndoCommand *paste = new QUndoCommand();
    paste->setText("Paste effects");

    QList<QGraphicsItem *> clips = scene()->selectedItems();
    for (int i = 0; i < clips.count(); ++i) {
        if (clips.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast < ClipItem *>(clips.at(i));
            for (int i = 0; i < clip->effectsCount(); i++) {
                new AddEffectCommand(this, m_scene->m_tracksList.count() - item->track(), item->startPos(), clip->effectAt(i), true, paste);
            }
        }
    }
    m_commandStack->push(paste);
}


ClipItem *CustomTrackView::getClipUnderCursor() const {
    QRectF rect((double) m_cursorPos, 0.0, 1.0, (double)(m_tracksHeight * m_scene->m_tracksList.count()));
    QList<QGraphicsItem *> collisions = scene()->items(rect, Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisions.count(); i++) {
        if (collisions.at(i)->type() == AVWIDGET) {
            return static_cast < ClipItem *>(collisions.at(i));
        }
    }
    return NULL;
}

ClipItem *CustomTrackView::getMainActiveClip() const {
    QList<QGraphicsItem *> clips = scene()->selectedItems();
    if (clips.isEmpty()) {
        return getClipUnderCursor();
    } else {
        ClipItem *item = NULL;
        for (int i = 0; i < clips.count(); ++i) {
            if (clips.at(i)->type() == AVWIDGET)
                item = static_cast < ClipItem *>(clips.at(i));
            if (item->startPos().frames(m_document->fps()) <= m_cursorPos && item->endPos().frames(m_document->fps()) >= m_cursorPos) break;
        }
        if (item) return item;
    }
    return NULL;
}

ClipItem *CustomTrackView::getActiveClipUnderCursor() const {
    QList<QGraphicsItem *> clips = scene()->selectedItems();
    if (clips.isEmpty()) {
        return getClipUnderCursor();
    } else {
        ClipItem *item;
        for (int i = 0; i < clips.count(); ++i) {
            if (clips.at(i)->type() == AVWIDGET)
                item = static_cast < ClipItem *>(clips.at(i));
            if (item->startPos().frames(m_document->fps()) <= m_cursorPos && item->endPos().frames(m_document->fps()) >= m_cursorPos) return item;
        }
    }
    return NULL;
}

void CustomTrackView::setInPoint() {
    ClipItem *clip = getActiveClipUnderCursor();
    if (clip == NULL) {
        emit displayMessage(i18n("You must select one clip for this action"), ErrorMessage);
        return;
    }
    ItemInfo startInfo = clip->info();
    ItemInfo endInfo = clip->info();
    endInfo.startPos = GenTime(m_cursorPos, m_document->fps());
    ResizeClipCommand *command = new ResizeClipCommand(this, startInfo, endInfo, true);
    m_commandStack->push(command);
}

void CustomTrackView::setOutPoint() {
    ClipItem *clip = getActiveClipUnderCursor();
    if (clip == NULL) {
        emit displayMessage(i18n("You must select one clip for this action"), ErrorMessage);
        return;
    }
    ItemInfo startInfo = clip->info();
    ItemInfo endInfo = clip->info();
    endInfo.endPos = GenTime(m_cursorPos, m_document->fps());
    ResizeClipCommand *command = new ResizeClipCommand(this, startInfo, endInfo, true);
    m_commandStack->push(command);
}

void CustomTrackView::slotUpdateAllThumbs() {
    QList<QGraphicsItem *> itemList = items();
    ClipItem *item;
    Transition *transitionitem;
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            item = static_cast <ClipItem *>(itemList.at(i));
            item->refreshClip();
            qApp->processEvents();
        }
    }
}

#include "customtrackview.moc"
