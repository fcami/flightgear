// BaseDiagram.hxx - part of GUI launcher using Qt5
// SPDX-FileCopyrightText: 2014 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include <QHash>
#include <QPainterPath>
#include <QPixmap>
#include <QQuickPaintedItem>
#include <QTransform>

#include <simgear/math/sg_geodesy.hxx>

#include <Navaids/positioned.hxx>
#include <Airports/airport.hxx>
#include <Navaids/PolyLine.hxx>
#include "LauncherController.hxx"

class BaseDiagram : public QQuickPaintedItem
{
    Q_OBJECT
public:
    BaseDiagram(QQuickItem* pr = nullptr);

    enum IconOption
    {
        NoOptions = 0,
        SmallIcons = 0x1,
        LargeAirportPlans = 0x2
    };

    Q_DECLARE_FLAGS(IconOptions, IconOption)

    static QPixmap iconForPositioned(const FGPositionedRef &pos, const IconOptions& options = NoOptions);
    static QPixmap iconForAirport(FGAirport *apt, const IconOptions& options = NoOptions);

    static QVector<QLineF> projectAirportRuwaysIntoRect(FGAirportRef apt, const QRectF& bounds);
    static QVector<QLineF> projectAirportRuwaysWithCenter(FGAirportRef apt, const SGGeod &c);

    void setAircraftType(LauncherController::AircraftType type);

    QRect rect() const;

    Q_INVOKABLE void resetZoom();
protected:
    void paint(QPainter* p) override;

    void mousePressEvent(QMouseEvent* me) override;
    void mouseMoveEvent(QMouseEvent* me) override;

    void wheelEvent(QWheelEvent* we) override;

    virtual void paintContents(QPainter*);


protected:
    void recomputeBounds(bool resetZoom);

    virtual void doComputeBounds();

    void extendBounds(const QPointF& p, double radiusM = 1.0);
    QPointF project(const SGGeod& geod) const;
    QTransform transform() const;

    void clearIgnoredNavaids();
    void addIgnoredNavaid(FGPositionedRef pos);

    SGGeod m_projectionCenter;
    double m_scale;
    QRectF m_bounds;
    bool m_autoScalePan;
    QPointF m_lastMousePos;
    int m_wheelAngleDeltaAccumulator;
    bool m_didPan;
    LauncherController::AircraftType m_aircraftType = LauncherController::Airplane;

    static void extendRect(QRectF& r, const QPointF& p);

    static QPointF project(const SGGeod &geod, const SGGeod &center);

    static SGGeod unproject(const QPointF &xy, const SGGeod &center);

    void paintAirplaneIcon(QPainter *painter, const SGGeod &geod, int headingDeg);
    void paintCarrierIcon(QPainter *painter, const SGGeod &geod, int headingDeg);
    void paintAirways(QPainter* painter, const FGPositionedList& navs);

    // Called after m_projectionCenter is updated during a pan. Subclasses that
    // cache pre-projected geometry must override this to re-project their data.
    virtual void onProjectionCenterChanged() {}

    QPointF projectedPosition(PositionedID pid) const;
    QPointF projectedPosition(FGPositionedRef pos) const;

private:
    enum LabelPosition
    {
        LABEL_RIGHT = 0,
        LABEL_ABOVE,
        LABEL_BELOW,
        LABEL_LEFT,
        LABEL_NE,
        LABEL_SE,
        LABEL_SW,
        LABEL_NW,
        LAST_POSITION // marker value
    };

    void paintNavaids(QPainter *p);

    bool isNavaidIgnored(const FGPositionedRef& pos) const;

    bool isLabelRectAvailable(const QRect& r) const;
    QRect rectAndFlagsForLabel(PositionedID guid, const QRect &item,
                               const QSize &bounds,
                               int & flags /* out parameter */) const;
    QRect labelPositioned(const QRect &itemRect, const QSize &bounds, LabelPosition lp) const;

    QTransform m_baseDeviceTransform;
    QTransform m_viewportTransform;
    QVector<FGPositionedRef> m_ignored;
    QPixmap m_carrierPixmap;

    mutable QHash<PositionedID, LabelPosition> m_labelPositions;
    mutable QVector<QRect> m_labelRects;

    mutable QHash<PositionedID, QPointF> m_projectedPositions;

    static int textFlagsForLabelPosition(LabelPosition pos);

    void splitItems(const FGPositionedList &in, FGPositionedList &navaids, FGPositionedList &ports);
    void paintNavaid(QPainter *painter, const FGPositionedRef &pos);
    void paintPolygonData(QPainter *painter);
    void paintGeodVec(QPainter *painter, const flightgear::SGGeodVec &vec);
    void fillClosedGeodVec(QPainter* painter, const flightgear::SGGeodVec& vec);

    void validatePolygonCache(const SGGeod& viewCenter, double drawRangeNm);

    struct PolygonDataCache {
        SGGeod viewCenter;
        double drawRangeNm = 0.0;
        flightgear::PolyLineList landLines;
        flightgear::PolyLineList gratLines;
        flightgear::PolyLineList coastLines;
        flightgear::PolyLineList nationalLines;
        flightgear::PolyLineList regionalLines;
        flightgear::PolyLineList urbanLines;
        flightgear::PolyLineList riverLines;
        flightgear::PolyLineList lakeLines;
        flightgear::PolyLineList geographicLines;
    };

    std::optional<PolygonDataCache> m_polygonCache;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(BaseDiagram::IconOptions)
