#include "mappixmapitem.h"
#include "log.h"

#define SWAP(a, b) do { if (a != b) { a ^= b; b ^= a; a ^= b; } } while (0)

void MapPixmapItem::paint(QGraphicsSceneMouseEvent *event) {
    if (map) {
        if (event->type() == QEvent::GraphicsSceneMouseRelease) {
            map->commit();
        } else {
            QPointF pos = event->pos();
            int x = static_cast<int>(pos.x()) / 16;
            int y = static_cast<int>(pos.y()) / 16;

            // Paint onto the map.
            bool smartPathsEnabled = event->modifiers() & Qt::ShiftModifier;
            QPoint selectionDimensions = this->metatileSelector->getSelectionDimensions();
            if ((this->settings->smartPathsEnabled || smartPathsEnabled) && selectionDimensions.x() == 3 && selectionDimensions.y() == 3) {
                paintSmartPath(x, y);
            } else {
                paintNormal(x, y);
            }
        }

        draw();
    }
}

void MapPixmapItem::shift(QGraphicsSceneMouseEvent *event) {
    if (map) {
        if (event->type() == QEvent::GraphicsSceneMouseRelease) {
            map->commit();
        } else {
            QPointF pos = event->pos();
            int x = static_cast<int>(pos.x()) / 16;
            int y = static_cast<int>(pos.y()) / 16;

            if (event->type() == QEvent::GraphicsSceneMousePress) {
                selection_origin = QPoint(x, y);
                selection.clear();
            } else if (event->type() == QEvent::GraphicsSceneMouseMove) {
                if (x != selection_origin.x() || y != selection_origin.y()) {
                    int xDelta = x - selection_origin.x();
                    int yDelta = y - selection_origin.y();
                    Blockdata *backupBlockdata = map->layout->blockdata->copy();
                    for (int i = 0; i < map->getWidth(); i++)
                    for (int j = 0; j < map->getHeight(); j++) {
                        int destX = i + xDelta;
                        int destY = j + yDelta;
                        if (destX < 0)
                            do { destX += map->getWidth(); } while (destX < 0);
                        if (destY < 0)
                            do { destY += map->getHeight(); } while (destY < 0);
                        destX %= map->getWidth();
                        destY %= map->getHeight();

                        int blockIndex = j * map->getWidth() + i;
                        Block srcBlock = backupBlockdata->blocks->at(blockIndex);
                        map->_setBlock(destX, destY, srcBlock);
                    }

                    delete backupBlockdata;
                    selection_origin = QPoint(x, y);
                    selection.clear();
                    draw();
                }
            }
        }
    }
}

void MapPixmapItem::paintNormal(int x, int y) {
    QPoint selectionDimensions = this->metatileSelector->getSelectionDimensions();
    QList<uint16_t> *selectedMetatiles = this->metatileSelector->getSelectedMetatiles();

    // Snap the selected position to the top-left of the block boundary.
    // This allows painting via dragging the mouse to tile the painted region.
    int xDiff = x - this->paint_tile_initial_x;
    int yDiff = y - this->paint_tile_initial_y;
    if (xDiff < 0 && xDiff % selectionDimensions.x() != 0) xDiff -= selectionDimensions.x();
    if (yDiff < 0 && yDiff % selectionDimensions.y() != 0) yDiff -= selectionDimensions.y();

    x = this->paint_tile_initial_x + (xDiff / selectionDimensions.x()) * selectionDimensions.x();
    y = this->paint_tile_initial_y + (yDiff / selectionDimensions.y()) * selectionDimensions.y();

    for (int i = 0; i < selectionDimensions.x() && i + x < map->getWidth(); i++)
    for (int j = 0; j < selectionDimensions.y() && j + y < map->getHeight(); j++) {
        int actualX = i + x;
        int actualY = j + y;
        Block *block = map->getBlock(actualX, actualY);
        if (block) {
            block->tile = selectedMetatiles->at(j * selectionDimensions.x() + i);
            map->_setBlock(actualX, actualY, *block);
        }
    }
}

// These are tile offsets from the top-left tile in the 3x3 smart path selection.
// Each entry is for one possibility from the marching squares value for a tile.
// (Marching Squares: https://en.wikipedia.org/wiki/Marching_squares)
QList<int> MapPixmapItem::smartPathTable = QList<int>({
    4, // 0000
    4, // 0001
    4, // 0010
    6, // 0011
    4, // 0100
    4, // 0101
    0, // 0110
    3, // 0111
    4, // 1000
    8, // 1001
    4, // 1010
    7, // 1011
    2, // 1100
    5, // 1101
    1, // 1110
    4, // 1111
});

#define IS_SMART_PATH_TILE(block) (selectedMetatiles->contains(block->tile))

void MapPixmapItem::paintSmartPath(int x, int y) {
    QPoint selectionDimensions = this->metatileSelector->getSelectionDimensions();
    QList<uint16_t> *selectedMetatiles = this->metatileSelector->getSelectedMetatiles();

    // Smart path should never be enabled without a 3x3 block selection.
    if (selectionDimensions.x() != 3 || selectionDimensions.y() != 3) return;

    // Shift to the middle tile of the smart path selection.
    uint16_t openTile = selectedMetatiles->at(4);

    // Fill the region with the open tile.
    for (int i = 0; i <= 1; i++)
    for (int j = 0; j <= 1; j++) {
        // Check if in map bounds.
        if (!(i + x < map->getWidth() && i + x >= 0 && j + y < map->getHeight() && j + y >= 0))
            continue;
        int actualX = i + x;
        int actualY = j + y;
        Block *block = map->getBlock(actualX, actualY);
        if (block) {
            block->tile = openTile;
            map->_setBlock(actualX, actualY, *block);
        }
    }

    // Go back and resolve the edge tiles
    for (int i = -1; i <= 2; i++)
    for (int j = -1; j <= 2; j++) {
        // Check if in map bounds.
        if (!(i + x < map->getWidth() && i + x >= 0 && j + y < map->getHeight() && j + y >= 0))
            continue;
        // Ignore the corners, which can't possible be affected by the smart path.
        if ((i == -1 && j == -1) || (i == 2 && j == -1) ||
            (i == -1 && j ==  2) || (i == 2 && j ==  2))
            continue;

        // Ignore tiles that aren't part of the smart path set.
        int actualX = i + x;
        int actualY = j + y;
        Block *block = map->getBlock(actualX, actualY);
        if (!block || !IS_SMART_PATH_TILE(block)) {
            continue;
        }

        int id = 0;
        Block *top = map->getBlock(actualX, actualY - 1);
        Block *right = map->getBlock(actualX + 1, actualY);
        Block *bottom = map->getBlock(actualX, actualY + 1);
        Block *left = map->getBlock(actualX - 1, actualY);

        // Get marching squares value, to determine which tile to use.
        if (top && IS_SMART_PATH_TILE(top))
            id += 1;
        if (right && IS_SMART_PATH_TILE(right))
            id += 2;
        if (bottom && IS_SMART_PATH_TILE(bottom))
            id += 4;
        if (left && IS_SMART_PATH_TILE(left))
            id += 8;

        block->tile = selectedMetatiles->at(smartPathTable[id]);
        map->_setBlock(actualX, actualY, *block);
    }
}

void MapPixmapItem::updateMetatileSelection(QGraphicsSceneMouseEvent *event) {
    QPointF pos = event->pos();
    int x = static_cast<int>(pos.x()) / 16;
    int y = static_cast<int>(pos.y()) / 16;

    // Snap point to within map bounds.
    if (x < 0) x = 0;
    if (x >= map->getWidth()) x = map->getWidth() - 1;
    if (y < 0) y = 0;
    if (y >= map->getHeight()) y = map->getHeight() - 1;

    // Update/apply copied metatiles.
    if (event->type() == QEvent::GraphicsSceneMousePress) {
        selection_origin = QPoint(x, y);
        selection.clear();
        selection.append(QPoint(x, y));
        uint16_t metatileId = map->getBlock(x, y)->tile;
        this->metatileSelector->select(metatileId);
    } else if (event->type() == QEvent::GraphicsSceneMouseMove) {
        QPoint pos = QPoint(x, y);
        int x1 = selection_origin.x();
        int y1 = selection_origin.y();
        int x2 = pos.x();
        int y2 = pos.y();
        if (x1 > x2) SWAP(x1, x2);
        if (y1 > y2) SWAP(y1, y2);
        selection.clear();
        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++) {
                selection.append(QPoint(x, y));
            }
        }

        QList<uint16_t> metatiles;
        for (QPoint point : selection) {
            metatiles.append(map->getBlock(point.x(), point.y())->tile);
        }

        this->metatileSelector->setExternalSelection(x2 - x1 + 1, y2 - y1 + 1, &metatiles);
    }
}

void MapPixmapItem::floodFill(QGraphicsSceneMouseEvent *event) {
    if (map) {
        if (event->type() == QEvent::GraphicsSceneMouseRelease) {
            map->commit();
        } else {
            QPointF pos = event->pos();
            int x = static_cast<int>(pos.x()) / 16;
            int y = static_cast<int>(pos.y()) / 16;
            Block *block = map->getBlock(x, y);
            QList<uint16_t> *selectedMetatiles = this->metatileSelector->getSelectedMetatiles();
            QPoint selectionDimensions = this->metatileSelector->getSelectionDimensions();
            int tile = selectedMetatiles->first();
            if (block && block->tile != tile) {
                bool smartPathsEnabled = event->modifiers() & Qt::ShiftModifier;
                if ((this->settings->smartPathsEnabled || smartPathsEnabled) && selectionDimensions.x() == 3 && selectionDimensions.y() == 3)
                    this->_floodFillSmartPath(x, y);
                else
                    this->_floodFill(x, y);
            }
        }

        draw();
    }
}

void MapPixmapItem::_floodFill(int initialX, int initialY) {
    QPoint selectionDimensions = this->metatileSelector->getSelectionDimensions();
    QList<uint16_t> *selectedMetatiles = this->metatileSelector->getSelectedMetatiles();

    QList<QPoint> todo;
    todo.append(QPoint(initialX, initialY));
    while (todo.length()) {
        QPoint point = todo.takeAt(0);
        int x = point.x();
        int y = point.y();

        Block *block = map->getBlock(x, y);
        if (!block) {
            continue;
        }

        int xDiff = x - initialX;
        int yDiff = y - initialY;
        int i = xDiff % selectionDimensions.x();
        int j = yDiff % selectionDimensions.y();
        if (i < 0) i = selectionDimensions.x() + i;
        if (j < 0) j = selectionDimensions.y() + j;
        uint16_t tile = selectedMetatiles->at(j * selectionDimensions.x() + i);
        uint16_t old_tile = block->tile;
        if (old_tile == tile) {
            continue;
        }

        block->tile = tile;
        map->_setBlock(x, y, *block);
        if ((block = map->getBlock(x + 1, y)) && block->tile == old_tile) {
            todo.append(QPoint(x + 1, y));
        }
        if ((block = map->getBlock(x - 1, y)) && block->tile == old_tile) {
            todo.append(QPoint(x - 1, y));
        }
        if ((block = map->getBlock(x, y + 1)) && block->tile == old_tile) {
            todo.append(QPoint(x, y + 1));
        }
        if ((block = map->getBlock(x, y - 1)) && block->tile == old_tile) {
            todo.append(QPoint(x, y - 1));
        }
    }
}

void MapPixmapItem::_floodFillSmartPath(int initialX, int initialY) {
    QPoint selectionDimensions = this->metatileSelector->getSelectionDimensions();
    QList<uint16_t> *selectedMetatiles = this->metatileSelector->getSelectedMetatiles();

    // Smart path should never be enabled without a 3x3 block selection.
    if (selectionDimensions.x() != 3 || selectionDimensions.y() != 3) return;

    // Shift to the middle tile of the smart path selection.
    uint16_t openTile = selectedMetatiles->at(4);

    // Flood fill the region with the open tile.
    QList<QPoint> todo;
    todo.append(QPoint(initialX, initialY));
    while (todo.length()) {
        QPoint point = todo.takeAt(0);
        int x = point.x();
        int y = point.y();

        Block *block = map->getBlock(x, y);
        if (!block) {
            continue;
        }

        uint16_t old_tile = block->tile;
        if (old_tile == openTile) {
            continue;
        }

        block->tile = openTile;
        map->_setBlock(x, y, *block);
        if ((block = map->getBlock(x + 1, y)) && block->tile == old_tile) {
            todo.append(QPoint(x + 1, y));
        }
        if ((block = map->getBlock(x - 1, y)) && block->tile == old_tile) {
            todo.append(QPoint(x - 1, y));
        }
        if ((block = map->getBlock(x, y + 1)) && block->tile == old_tile) {
            todo.append(QPoint(x, y + 1));
        }
        if ((block = map->getBlock(x, y - 1)) && block->tile == old_tile) {
            todo.append(QPoint(x, y - 1));
        }
    }

    // Go back and resolve the flood-filled edge tiles.
    // Mark tiles as visited while we go.
    int numMetatiles = map->getWidth() * map->getHeight();
    bool *visited = new bool[numMetatiles];
    for (int i = 0; i < numMetatiles; i++)
        visited[i] = false;

    todo.append(QPoint(initialX, initialY));
    while (todo.length()) {
        QPoint point = todo.takeAt(0);
        int x = point.x();
        int y = point.y();
        visited[x + y * map->getWidth()] = true;

        Block *block = map->getBlock(x, y);
        if (!block) {
            continue;
        }

        int id = 0;
        Block *top = map->getBlock(x, y - 1);
        Block *right = map->getBlock(x + 1, y);
        Block *bottom = map->getBlock(x, y + 1);
        Block *left = map->getBlock(x - 1, y);

        // Get marching squares value, to determine which tile to use.
        if (top && IS_SMART_PATH_TILE(top))
            id += 1;
        if (right && IS_SMART_PATH_TILE(right))
            id += 2;
        if (bottom && IS_SMART_PATH_TILE(bottom))
            id += 4;
        if (left && IS_SMART_PATH_TILE(left))
            id += 8;

        block->tile = selectedMetatiles->at(smartPathTable[id]);
        map->_setBlock(x, y, *block);

        // Visit neighbors if they are smart-path tiles, and don't revisit any.
        if (!visited[x + 1 + y * map->getWidth()] && (block = map->getBlock(x + 1, y)) && IS_SMART_PATH_TILE(block)) {
            todo.append(QPoint(x + 1, y));
            visited[x + 1 + y * map->getWidth()] = true;
        }
        if (!visited[x - 1 + y * map->getWidth()] && (block = map->getBlock(x - 1, y)) && IS_SMART_PATH_TILE(block)) {
            todo.append(QPoint(x - 1, y));
            visited[x - 1 + y * map->getWidth()] = true;
        }
        if (!visited[x + (y + 1) * map->getWidth()] && (block = map->getBlock(x, y + 1)) && IS_SMART_PATH_TILE(block)) {
            todo.append(QPoint(x, y + 1));
            visited[x + (y + 1) * map->getWidth()] = true;
        }
        if (!visited[x + (y - 1) * map->getWidth()] && (block = map->getBlock(x, y - 1)) && IS_SMART_PATH_TILE(block)) {
            todo.append(QPoint(x, y - 1));
            visited[x + (y - 1) * map->getWidth()] = true;
        }
    }

    delete[] visited;
}

void MapPixmapItem::pick(QGraphicsSceneMouseEvent *event) {
    QPointF pos = event->pos();
    int x = static_cast<int>(pos.x()) / 16;
    int y = static_cast<int>(pos.y()) / 16;
    Block *block = map->getBlock(x, y);
    if (block) {
        this->metatileSelector->select(block->tile);
    }
}

void MapPixmapItem::select(QGraphicsSceneMouseEvent *event) {
    QPointF pos = event->pos();
    int x = static_cast<int>(pos.x()) / 16;
    int y = static_cast<int>(pos.y()) / 16;
    if (event->type() == QEvent::GraphicsSceneMousePress) {
        selection_origin = QPoint(x, y);
        selection.clear();
    } else if (event->type() == QEvent::GraphicsSceneMouseMove) {
        if (event->buttons() & Qt::LeftButton) {
            selection.clear();
            selection.append(QPoint(x, y));
        }
    } else if (event->type() == QEvent::GraphicsSceneMouseRelease) {
        if (!selection.isEmpty()) {
            QPoint pos = selection.last();
            int x1 = selection_origin.x();
            int y1 = selection_origin.y();
            int x2 = pos.x();
            int y2 = pos.y();
            if (x1 > x2) SWAP(x1, x2);
            if (y1 > y2) SWAP(y1, y2);
            selection.clear();
            for (int y = y1; y <= y2; y++) {
                for (int x = x1; x <= x2; x++) {
                    selection.append(QPoint(x, y));
                }
            }
            logInfo(QString("selected (%1, %2) -> (%3, %4)").arg(x1).arg(y1).arg(x2).arg(y2));
        }
    }
}

void MapPixmapItem::draw(bool ignoreCache) {
    if (map) {
        setPixmap(map->render(ignoreCache));
    }
}

void MapPixmapItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    int x = static_cast<int>(event->pos().x()) / 16;
    int y = static_cast<int>(event->pos().y()) / 16;
    emit this->hoveredMapMetatileChanged(x, y);
    if (this->settings->betterCursors){
        setCursor(this->settings->mapCursor);
    }
}
void MapPixmapItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *) {
    emit this->hoveredMapMetatileCleared();
    if (this->settings->betterCursors){
        unsetCursor();
    }
}
void MapPixmapItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    QPointF pos = event->pos();
    int x = static_cast<int>(pos.x()) / 16;
    int y = static_cast<int>(pos.y()) / 16;
    this->paint_tile_initial_x = x;
    this->paint_tile_initial_y = y;
    emit mouseEvent(event, this);
}
void MapPixmapItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    int x = static_cast<int>(event->pos().x()) / 16;
    int y = static_cast<int>(event->pos().y()) / 16;
    emit this->hoveredMapMetatileChanged(x, y);
    emit mouseEvent(event, this);
}
void MapPixmapItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    emit mouseEvent(event, this);
}
