#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QRectF>

#include <GUI/GettingStartedTip.hxx>

class TipBackgroundBox : public QQuickPaintedItem
{
    Q_OBJECT

    Q_PROPERTY(GettingStartedTip::Arrow arrow READ arrowPosition WRITE setArrowPosition NOTIFY arrowPositionChanged)

    Q_PROPERTY(QColor fill READ fill WRITE setFill NOTIFY fillChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY borderColorChanged)
    Q_PROPERTY(int borderWidth READ borderWidth WRITE setBorderWidth NOTIFY borderWidthChanged)

public:
    TipBackgroundBox(QQuickItem* parent = nullptr);

    GettingStartedTip::Arrow arrowPosition() const;

    QColor borderColor() const;

    int borderWidth() const;

    QColor fill() const;

    static int arrowSideOffset();

    static int arrowHeight();

    // where the background box should be positioned/sized, in coordinates
    // relative to the tip's anchor point (0,0), for a given arrow position,
    // box width, desired content height and border width. The box is padded
    // to fit the border stroke, which is centered on (and so bleeds outside)
    // the drawn shape.
    static QRectF boxGeometryForArrow(GettingStartedTip::Arrow arrow, double boxWidth, double contentHeight, double borderWidth);

    // where the content (text) area should be positioned/sized within the
    // background box, leaving room for the arrow and the border padding
    // boxGeometryForArrow() applies
    static QRectF contentGeometryForArrow(GettingStartedTip::Arrow arrow, double boxWidth, double borderWidth);

public slots:
    void setArrowPosition(GettingStartedTip::Arrow arrow);

    void setBorderColor(QColor borderColor);

    void setBorderWidth(int borderWidth);

    void setFill(QColor fill);

signals:
    void arrowPositionChanged(GettingStartedTip::Arrow arrow);

    void borderColorChanged(QColor borderColor);

    void borderWidthChanged(int borderWidth);

    void fillChanged(QColor fill);

protected:
    void paint(QPainter* painter) override;

private:
    GettingStartedTip::Arrow _arrow = GettingStartedTip::Arrow::TopCenter;

    QColor _borderColor;
    int _borderWidth = -1; // off
    QColor _fill;
};

