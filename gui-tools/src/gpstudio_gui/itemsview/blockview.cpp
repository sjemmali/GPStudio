/****************************************************************************
** Copyright (C) 2016 Dream IP
** 
** This file is part of GPStudio.
**
** GPStudio is a free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/

#include "blockview.h"

#include <QDebug>
#include <QMouseEvent>
#include <QMimeData>
#include <qmath.h>
#include <QMenu>

#include "blockitem.h"
#include "blockconnectoritem.h"
#include "blockportitem.h"

BlockView::BlockView(QWidget *parent)
    : QGraphicsView(parent)
{
    _project = NULL;
    _editMode = false;
    _scene = new BlockScene();
    scale(0.75, 0.75);

    _startConnectItem = NULL;
    _lineConector = NULL;

    setScene(_scene);

    setAcceptDrops(true);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    setRenderHint(QPainter::TextAntialiasing, true);

    setDragMode(QGraphicsView::ScrollHandDrag);

    connect(_scene, SIGNAL(selectionChanged()), this, SLOT(updateSelection()));
}

BlockView::~BlockView()
{
}

void BlockView::attachProject(GPNodeProject *project)
{
    if(_project)
        disconnect(_project);

    _project = project;

    connect(_project, SIGNAL(nodeChanged(ModelNode*)),
            this, SLOT(changeNode(ModelNode*)));
    connect(_project, SIGNAL(blockUpdated(ModelBlock*)),
            this, SLOT(updateBlock(ModelBlock*)));
    connect(_project, SIGNAL(blockAdded(ModelBlock*)),
            this, SLOT(addBlock(ModelBlock*)));
    connect(_project, SIGNAL(blockRemoved(QString)),
            this, SLOT(removeBlock(QString)));
    connect(_project, SIGNAL(blockConnected(ModelFlowConnect)),
            this, SLOT(connectBlock(ModelFlowConnect)));
    connect(_project, SIGNAL(blockDisconected(ModelFlowConnect)),
            this, SLOT(disconnectBlock(ModelFlowConnect)));

    connect(this, SIGNAL(blockMoved(QString,QPoint,QPoint)),
            project, SLOT(moveBlock(QString,QPoint,QPoint)));
    connect(this, SIGNAL(blockDeleted(ModelBlock*)),
            project, SLOT(removeBlock(ModelBlock*)));
    connect(this, SIGNAL(blockPortConnected(ModelFlowConnect)),
            project, SLOT(connectBlockFlows(ModelFlowConnect)));
    connect(this, SIGNAL(blockPortDisconnected(ModelFlowConnect)),
            project, SLOT(disConnectBlockFlows(ModelFlowConnect)));

    if(project->node())
    {
        changeNode(project->node());
    }
}

void BlockView::dragEnterEvent(QDragEnterEvent *event)
{
    QGraphicsView::dragEnterEvent(event);
    if(_editMode)
    {
        if(event->mimeData()->hasText())
            event->accept();
    }
}

void BlockView::dragMoveEvent(QDragMoveEvent *event)
{
    QGraphicsView::dragMoveEvent(event);
    if(_editMode)
    {
        if(event->mimeData()->hasText())
            event->accept();
    }
}

void BlockView::dropEvent(QDropEvent *event)
{
    QGraphicsView::dropEvent(event);
    if(_editMode)
    {
        QString driver = event->mimeData()->text();
        QPoint pos = mapToScene(event->pos()).toPoint();

        _project->addBlock(driver, pos);
    }
}

void BlockView::mousePressEvent(QMouseEvent *event)
{
    QGraphicsView::mousePressEvent(event);

    if(event->button() == Qt::LeftButton)
    {
        if(_editMode)
        {
            BlockPortItem *processItem = qgraphicsitem_cast<BlockPortItem*>(itemAt(event->pos()));
            if(processItem)
            {
                _startConnectItem = processItem;
                _lineConector = new BlockConnectorItem(_startConnectItem);
                blockScene()->addItem(_lineConector);
                _lineConector->setEndPos(mapToScene(event->pos()).toPoint());
            }
        }
    }
}

void BlockView::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsView::mouseMoveEvent(event);

    if(_startConnectItem)
        _lineConector->setEndPos(mapToScene(event->pos()).toPoint());
}

void BlockView::mouseReleaseEvent(QMouseEvent *event)
{
    QGraphicsView::mouseReleaseEvent(event);

    BlockItem *blockItem = qgraphicsitem_cast<BlockItem*>(itemAt(event->pos()));
    if(blockItem)
        if(blockItem->pos() != blockItem->modelBlock()->pos())
            emit blockMoved(blockItem->name(), blockItem->modelBlock()->pos(), blockItem->pos().toPoint());

    if(_startConnectItem)
    {
        BlockPortItem *processItem = qgraphicsitem_cast<BlockPortItem*>(itemAt(event->pos()));
        if(processItem && processItem!=_startConnectItem)
            emit blockPortConnected(ModelFlowConnect(_startConnectItem->blockName(), _startConnectItem->name(),
                                                     processItem->blockName(),       processItem->name()));

        _lineConector->disconnectPorts();
        scene()->removeItem(_lineConector);
        delete _lineConector;
        _startConnectItem = NULL;
        _lineConector = NULL;
    }
}

void BlockView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QGraphicsView::mouseDoubleClickEvent(event);

    QGraphicsItem *item = _scene->itemAt(mapToScene(event->pos()), QTransform());
    BlockItem *blockItem = qgraphicsitem_cast<BlockItem *>(item);
    if(blockItem)
        emit blockDetailsRequest(blockItem->name());
}

void BlockView::updateSelection()
{
    if(_scene->selectedItems().count()==0)
    {
        emit blockSelected("");
        return;
    }

    QGraphicsItem *item = _scene->selectedItems().at(0);
    BlockItem *blockItem = qgraphicsitem_cast<BlockItem *>(item);
    if(blockItem)
        emit blockSelected(blockItem->name());
}

void BlockView::selectBlock(QString blockName)
{
    _scene->blockSignals(true);
    _scene->clearSelection();

    BlockItem *blockItem = _scene->block(blockName);
    if(blockItem)
    {
        blockItem->setSelected(true);
        blockItem->ensureVisible();
    }

    _scene->blockSignals(false);
}

void BlockView::updateBlock(ModelBlock *block)
{
    BlockItem *blockItem = _scene->block(block);
    if(blockItem)
    {
        blockItem->updatePos();
    }
}

void BlockView::addBlock(ModelBlock *block)
{
    _scene->addBlock(block);
}

void BlockView::removeBlock(const QString &block_name)
{
    _scene->removeBlock(block_name);
}

void BlockView::connectBlock(const ModelFlowConnect &flowConnect)
{
    _scene->connectBlockPort(flowConnect);
}

void BlockView::disconnectBlock(const ModelFlowConnect &flowConnect)
{
    _scene->disconnectBlockPort(flowConnect);
}

void BlockView::changeNode(ModelNode *node)
{
    loadFromNode(node);
}

void BlockView::setBlockScene(BlockScene *scene)
{
    _scene = scene;
    setScene(scene);
}

BlockScene *BlockView::blockScene() const
{
    return _scene;
}

bool BlockView::loadFromNode(const ModelNode *node)
{
    return _scene->loadFromNode(node);
}

bool BlockView::loadFromCam(const Camera *camera)
{
    return _scene->loadFromCamera(camera);
}

void BlockView::zoomIn()
{
    setZoomLevel(1);
}

void BlockView::zoomOut()
{
    setZoomLevel(-1);
}

void BlockView::zoomFit()
{
    fitInView(_scene->itemsBoundingRect(), Qt::KeepAspectRatio);
}

bool BlockView::editMode() const
{
    return _editMode;
}

void BlockView::setEditMode(bool editMode)
{
    _editMode = editMode;
}

void BlockView::setZoomLevel(int step)
{
    double zoom = qPow(1.25,step);
    scale(zoom, zoom);
}

void BlockView::wheelEvent(QWheelEvent *event)
{
    int numDegrees = event->delta() / 8;
    int numSteps = numDegrees / 15;

    setZoomLevel(numSteps);
}

void BlockView::keyPressEvent(QKeyEvent *event)
{
    if(event->key()==Qt::Key_Plus)
        zoomIn();
    if(event->key()==Qt::Key_Minus)
        zoomOut();
    if(event->key()==Qt::Key_Asterisk)
        zoomFit();
    if(event->key()==Qt::Key_F2)
        zoomFit();
    if(event->key()==Qt::Key_Delete || event->key()==Qt::Key_Backspace)
    {
        if(_scene->selectedItems().count()>0)
        {
            QGraphicsItem *item = _scene->selectedItems().at(0);
            BlockConnectorItem *connectorItem = qgraphicsitem_cast<BlockConnectorItem *>(item);
            if(connectorItem)
            {
                emit blockPortDisconnected(ModelFlowConnect(connectorItem->portItem1()->blockName(),
                                                            connectorItem->portItem1()->name(),
                                                            connectorItem->portItem2()->blockName(),
                                                            connectorItem->portItem2()->name()));
            }
            else
            {
                BlockItem *blockItem = qgraphicsitem_cast<BlockItem *>(item);
                if(blockItem)
                    emit blockDeleted(blockItem->modelBlock());
            }
        }
    }
    QGraphicsView::keyPressEvent(event);
}

void BlockView::contextMenuEvent(QContextMenuEvent *event)
{
    QGraphicsItem *item;
    if(_scene->selectedItems().count()>0)
        item = _scene->selectedItems().at(0);
    else
        item = _scene->itemAt(event->globalPos(), QTransform());

    if(item)
    {
        BlockItem *blockItem = qgraphicsitem_cast<BlockItem *>(item);
        if(blockItem)
        {
            QMenu menu;
            QAction *removeAction = menu.addAction("Remove");
            QAction *markAction = menu.addAction("Mark");
            menu.exec(event->globalPos());
            //emit blockDeleted(blockItem->modelBlock());
        }
    }
}
