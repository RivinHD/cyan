/*
#
# Cyan <https://cyan.fxarena.net>
# Copyright Ole-André Rodlie, FxArena DA.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
*/
#include "editor.h"

void Editor::newTab(CyanImageFormat::CyanCanvas canvas)
{
    qDebug() << "new tab from canvas/project";
    QMdiSubWindow *tab = new QMdiSubWindow(mdi);
    tab->setAttribute(Qt::WA_DeleteOnClose);

    View *view = new View(tab);
    connectView(view);

    view->setCanvasSpecsFromImage(canvas.image);
    view->setLayersFromCanvas(canvas);
    view->setFit(viewZoomFitAct->isChecked());
    view->setBrushColor(colorPicker->currentColor());
    view->setDirty(false);

    tab->setWidget(view);
    tab->showMaximized();
    tab->setWindowIcon(QIcon::fromTheme("applications-graphics"));

    /*if (viewMoveAct->isChecked()) {
        view->setInteractiveMode(View::IteractiveMoveMode);
    } else if (viewDragAct->isChecked()) {
        view->setInteractiveMode(View::InteractiveDragMode);
    } else if (viewDrawAct->isChecked()) {
        view->setInteractiveMode(View::InteractiveDrawMode);
    }*/
    setViewTool(view);
    updateTabTitle(view);
    handleTabActivated(tab);
    setCurrentZoom();
}

void Editor::newTab(Magick::Image image, QSize geo)
{
    qDebug() << "new tab from image";
    //if (!image.isValid()) { return; }
    /*if ((image.columns()==0 || image.rows() == 0) && geo.width()==0) {
        emit statusMessage(tr("Invalid image/size"));
        return new View(this);
    }*/

    QMdiSubWindow *tab = new QMdiSubWindow(mdi);
    tab->setAttribute(Qt::WA_DeleteOnClose);

    View *view = new View(tab);
    connectView(view);

    if (geo.width()>0) {
        view->setupCanvas(geo.width(), geo.height());
    } else if (image.columns()>0) {
        view->setCanvasSpecsFromImage(image);
    }
    if (geo.width()>0) { view->addLayer(view->getCanvas()); }
    else { view->addLayer(image); }
    view->setFit(viewZoomFitAct->isChecked());
    view->setBrushColor(colorPicker->currentColor());

    tab->setWidget(view);
    tab->showMaximized();
    tab->setWindowIcon(QIcon::fromTheme("applications-graphics"));

    /*if (viewMoveAct->isChecked()) {
        view->setInteractiveMode(View::IteractiveMoveMode);
    } else if (viewDragAct->isChecked()) {
        view->setInteractiveMode(View::InteractiveDragMode);
    } else if (viewDrawAct->isChecked()) {
        view->setInteractiveMode(View::InteractiveDrawMode);
    }*/
    setViewTool(view);
    updateTabTitle(view);
    handleTabActivated(tab);
    setCurrentZoom();
    //return view;
}

void Editor::handleTabActivated(QMdiSubWindow *tab)
{
    qDebug() << "handle tab activated";
    if (!tab) { return; }
    View *view = qobject_cast<View*>(tab->widget());
    if (!view) { return; }

    /*if (viewDragAct->isChecked()) {
        view->setDragMode(QGraphicsView::ScrollHandDrag);
        view->setInteractive(false);
    } else {
        view->setDragMode(QGraphicsView::NoDrag);
        view->setInteractive(true);
    }*/
    updateTabTitle();
    handleBrushSize();
    viewZoomFitAct->setChecked(view->isFit());
    setCurrentZoom();
    handleCanvasStatus();
}

void Editor::updateTabTitle(View *view)
{
    if (!view) { view = qobject_cast<View*>(getCurrentCanvas()); }
    if (!view) { return; }
    QString title = CyanCommon::canvasWindowTitle(view->getCanvas());
    qDebug() << "update canvas title" << title;
    view->setWindowTitle(title);
}
