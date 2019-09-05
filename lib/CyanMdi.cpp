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

#include "CyanMdi.h"

#include <QMimeData>

Mdi::Mdi(QWidget *parent)
    : QMdiArea(parent)
{
    setAcceptDrops(true);
}

void Mdi::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void Mdi::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void Mdi::dragLeaveEvent(QDragLeaveEvent *event)
{
    event->accept();
}

void Mdi::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        emit openImages(mimeData->urls());
    }
}
