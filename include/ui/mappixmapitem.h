#ifndef MAPPIXMAPITEM_H
#define MAPPIXMAPITEM_H

#include "map.h"
#include "settings.h"
#include "metatileselector.h"
#include <QGraphicsPixmapItem>

class MapPixmapItem : public QObject, public QGraphicsPixmapItem {
    Q_OBJECT
public:
    MapPixmapItem(Map *map_, MetatileSelector *metatileSelector, Settings *settings) {
        this->map = map_;
        this->metatileSelector = metatileSelector;
        this->settings = settings;
        setAcceptHoverEvents(true);
    }
    Map *map;
    MetatileSelector *metatileSelector;
    Settings *settings;
    bool active;
    bool right_click;
    int paint_tile_initial_x;
    int paint_tile_initial_y;
    QPoint selection_origin;
    QList<QPoint> selection;
    virtual void paint(QGraphicsSceneMouseEvent*);
    virtual void floodFill(QGraphicsSceneMouseEvent*);
    void _floodFill(int x, int y);
    void _floodFillSmartPath(int initialX, int initialY);
    virtual void pick(QGraphicsSceneMouseEvent*);
    virtual void select(QGraphicsSceneMouseEvent*);
    virtual void shift(QGraphicsSceneMouseEvent*);
    virtual void draw(bool ignoreCache = false);
    void updateMetatileSelection(QGraphicsSceneMouseEvent *event);

private:
    void paintNormal(int x, int y);
    void paintSmartPath(int x, int y);
    static QList<int> smartPathTable;

signals:
    void mouseEvent(QGraphicsSceneMouseEvent *, MapPixmapItem *);
    void hoveredMapMetatileChanged(int x, int y);
    void hoveredMapMetatileCleared();

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent*);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*);
    void mousePressEvent(QGraphicsSceneMouseEvent*);
    void mouseMoveEvent(QGraphicsSceneMouseEvent*);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent*);
};

#endif // MAPPIXMAPITEM_H
