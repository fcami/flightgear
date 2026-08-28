#include "TipBackgroundBox.hxx"

#include <cmath>

#include <QPainter>
#include <QPainterPath>
#include <QDebug>

namespace {

void pathLineBy(QPainterPath& pp, double x, double y)
{
    const auto c = pp.currentPosition();
    pp.lineTo(c.x() + x, c.y() + y);
}

QPointF unitVector(QPointF v)
{
    const double len = std::hypot(v.x(), v.y());
    return (len > 0.0) ? v / len : QPointF(0.0, 0.0);
}

// draws a rounded turn at 'corner', from the path's current position, heading
// on towards 'next' (the caller is still responsible for a final lineTo(next)
// unless 'next' is itself the target of a following cornerTo/closeSubpath)
void pathCornerTo(QPainterPath& pp, QPointF corner, QPointF next, double radius)
{
    const QPointF dirIn = unitVector(corner - pp.currentPosition());
    const QPointF dirOut = unitVector(next - corner);
    pp.lineTo(corner - dirIn * radius);
    pp.quadTo(corner, corner + dirOut * radius);
}

const double arrowDim1 = 20.0;
const double arrowEndOffset = 30;
const double cornerRadius = 10.0;

// how much the box needs padding out on each side to fit a border of this
// width, which is stroked centered on the path and so bleeds half its width
// outside it. Rounded UP to a whole pixel: a fractional margin here leads to
// fractional item positions/sizes throughout the tip's geometry, which in
// practice breaks Qt Quick mouse-event hit-testing for the content inside
// (see flightgear/#3510) - and since padding only needs to be generous
// enough to avoid clipping, rounding up is always safe.
double marginForBorderWidth(double borderWidth)
{
    return borderWidth > 0.0 ? std::ceil(borderWidth * 0.5) : 0.0;
}

QPainterPath pathFromArrowAndGeometry(GettingStartedTip::Arrow arrow, const QRectF& g)
{
    QPainterPath pp;

    switch (arrow) {
    case GettingStartedTip::Arrow::TopCenter:
        // arrow is on the top edge, away from any corner: round all four
        pp.moveTo(g.center().x(), 0.0);
        pp.lineTo(g.center().x() + arrowDim1, arrowDim1);
        pathCornerTo(pp, {g.right(), arrowDim1}, {g.right(), g.bottom()}, cornerRadius);
        pathCornerTo(pp, {g.right(), g.bottom()}, {g.left(), g.bottom()}, cornerRadius);
        pathCornerTo(pp, {g.left(), g.bottom()}, {g.left(), arrowDim1}, cornerRadius);
        pathCornerTo(pp, {g.left(), arrowDim1}, {g.center().x() - arrowDim1, arrowDim1}, cornerRadius);
        pp.lineTo(g.center().x() - arrowDim1, arrowDim1);
        pp.closeSubpath();
        break;

    case GettingStartedTip::Arrow::BottomCenter:
        // arrow is on the bottom edge, away from any corner: round all four
        pp.moveTo(g.center().x(), g.bottom());
        pp.lineTo(g.center().x() + arrowDim1, g.bottom() - arrowDim1);
        pathCornerTo(pp, {g.right(), g.bottom() - arrowDim1}, {g.right(), g.top()}, cornerRadius);
        pathCornerTo(pp, {g.right(), g.top()}, {g.left(), g.top()}, cornerRadius);
        pathCornerTo(pp, {g.left(), g.top()}, {g.left(), g.bottom() - arrowDim1}, cornerRadius);
        pathCornerTo(pp, {g.left(), g.bottom() - arrowDim1}, {g.center().x() - arrowDim1, g.bottom() - arrowDim1}, cornerRadius);
        pp.lineTo(g.center().x() - arrowDim1, g.bottom() - arrowDim1);
        pp.closeSubpath();
        break;

    case GettingStartedTip::Arrow::BottomRight:
        // arrow sits at the bottom-right corner: no radius there
        pp.moveTo(g.right() - arrowEndOffset, g.bottom());
        pathLineBy(pp, arrowDim1, -arrowDim1);
        pp.lineTo(g.right(), g.bottom() - arrowDim1);
        pathCornerTo(pp, {g.right(), g.top()}, {g.left(), g.top()}, cornerRadius);
        pathCornerTo(pp, {g.left(), g.top()}, {g.left(), g.bottom() - arrowDim1}, cornerRadius);
        pathCornerTo(pp, {g.left(), g.bottom() - arrowDim1}, {g.right() - arrowEndOffset - arrowDim1, g.bottom() - arrowDim1}, cornerRadius);
        pp.lineTo(g.right() - arrowEndOffset - arrowDim1, g.bottom() - arrowDim1);
        pp.closeSubpath();
        break;

    case GettingStartedTip::Arrow::TopRight:
        // arrow sits at the top-right corner: no radius there
        pp.moveTo(g.right() - arrowEndOffset, 0.0);
        pathLineBy(pp, arrowDim1, arrowDim1);
        pp.lineTo(g.right(), arrowDim1);
        pathCornerTo(pp, {g.right(), g.bottom()}, {g.left(), g.bottom()}, cornerRadius);
        pathCornerTo(pp, {g.left(), g.bottom()}, {g.left(), arrowDim1}, cornerRadius);
        pathCornerTo(pp, {g.left(), arrowDim1}, {g.right() - (arrowEndOffset + arrowDim1), arrowDim1}, cornerRadius);
        pp.lineTo(g.right() - (arrowEndOffset + arrowDim1), arrowDim1);
        pp.closeSubpath();
        break;

    case GettingStartedTip::Arrow::TopLeft:
        // arrow sits at the top-left corner: no radius there
        pp.moveTo(arrowEndOffset, 0.0);
        pathLineBy(pp, arrowDim1, arrowDim1);
        pathCornerTo(pp, {g.right(), arrowDim1}, {g.right(), g.bottom()}, cornerRadius);
        pathCornerTo(pp, {g.right(), g.bottom()}, {g.left(), g.bottom()}, cornerRadius);
        pathCornerTo(pp, {g.left(), g.bottom()}, {g.left(), arrowDim1}, cornerRadius);
        pp.lineTo(g.left(), arrowDim1);
        pp.lineTo(arrowEndOffset - arrowDim1, arrowDim1);
        pp.closeSubpath();
        break;

    case GettingStartedTip::Arrow::LeftCenter:
        // arrow is on the left edge, away from any corner: round all four
        pp.moveTo(0.0, g.center().y());
        pathLineBy(pp, arrowDim1, -arrowDim1);
        pathCornerTo(pp, {arrowDim1, g.top()}, {g.right(), g.top()}, cornerRadius);
        pathCornerTo(pp, {g.right(), g.top()}, {g.right(), g.bottom()}, cornerRadius);
        pathCornerTo(pp, {g.right(), g.bottom()}, {arrowDim1, g.bottom()}, cornerRadius);
        pathCornerTo(pp, {arrowDim1, g.bottom()}, {arrowDim1, g.center().y() + arrowDim1}, cornerRadius);
        pp.lineTo(arrowDim1, g.center().y() + arrowDim1);
        pp.closeSubpath();
        break;

    case GettingStartedTip::Arrow::RightCenter:
        // arrow is on the right edge, away from any corner: round all four
        pp.moveTo(g.right(), g.center().y());
        pathLineBy(pp, -arrowDim1, arrowDim1);
        pathCornerTo(pp, {g.right() - arrowDim1, g.bottom()}, {g.left(), g.bottom()}, cornerRadius);
        pathCornerTo(pp, {g.left(), g.bottom()}, {g.left(), g.top()}, cornerRadius);
        pathCornerTo(pp, {g.left(), g.top()}, {g.right() - arrowDim1, g.top()}, cornerRadius);
        pathCornerTo(pp, {g.right() - arrowDim1, g.top()}, {g.right() - arrowDim1, g.center().y() - arrowDim1}, cornerRadius);
        pp.lineTo(g.right() - arrowDim1, g.center().y() - arrowDim1);
        pp.closeSubpath();
        break;


    case GettingStartedTip::Arrow::LeftTop:
        // arrow sits at the top-left corner: no radius there
        pp.moveTo(0.0, g.top() + arrowDim1);
        pathLineBy(pp, arrowDim1, -arrowDim1);
        pathCornerTo(pp, {g.right(), g.top()}, {g.right(), g.bottom()}, cornerRadius);
        pathCornerTo(pp, {g.right(), g.bottom()}, {g.left() + arrowDim1, g.bottom()}, cornerRadius);
        pathCornerTo(pp, {g.left() + arrowDim1, g.bottom()}, {g.left() + arrowDim1, -g.left() + arrowDim1}, cornerRadius);
        pp.lineTo(g.left() + arrowDim1, -g.left() + arrowDim1);
        pp.closeSubpath();
        break;

    case GettingStartedTip::Arrow::NoArrow:
        // no arrow at all: round all four corners
        pp.moveTo(cornerRadius, g.top());
        pathCornerTo(pp, {g.right(), g.top()}, {g.right(), g.bottom()}, cornerRadius);
        pathCornerTo(pp, {g.right(), g.bottom()}, {0.0, g.bottom()}, cornerRadius);
        pathCornerTo(pp, {0.0, g.bottom()}, {0.0, g.top()}, cornerRadius);
        pathCornerTo(pp, {0.0, g.top()}, {cornerRadius, g.top()}, cornerRadius);
        pp.closeSubpath();
        break;


    default:
        qWarning() << Q_FUNC_INFO << "unhandled:" << arrow;
        break;
    }

    return pp;
}


} // of anonymous

int TipBackgroundBox::arrowSideOffset()
{
    return static_cast<int>(arrowEndOffset);
}

int TipBackgroundBox::arrowHeight()
{
    return static_cast<int>(arrowDim1);
}

QRectF TipBackgroundBox::boxGeometryForArrow(GettingStartedTip::Arrow arrow, double boxWidth, double contentHeight, double borderWidth)
{
    const double halfBoxWidth = boxWidth * 0.5;
    const double rightSideOffset = -boxWidth + arrowEndOffset;
    const double topHeightOffset = -arrowDim1;

    QRectF g;
    Qt::Alignment verticalAlignment = Qt::AlignVCenter;
    bool addArrowHeight = true; // whether the arrow eats into the content height

    switch (arrow) {
    case GettingStartedTip::Arrow::BottomRight:
        g = QRectF{rightSideOffset, 0.0, boxWidth, 0.0};
        verticalAlignment = Qt::AlignBottom;
        break;

    case GettingStartedTip::Arrow::BottomCenter:
        g = QRectF{-halfBoxWidth, 0.0, boxWidth, 0.0};
        verticalAlignment = Qt::AlignBottom;
        break;

    case GettingStartedTip::Arrow::TopCenter:
        g = QRectF{-halfBoxWidth, 0.0, boxWidth, 0.0};
        verticalAlignment = Qt::AlignTop;
        break;

    case GettingStartedTip::Arrow::TopRight:
        g = QRectF{rightSideOffset, 0.0, boxWidth, 0.0};
        verticalAlignment = Qt::AlignTop;
        break;

    case GettingStartedTip::Arrow::TopLeft:
        g = QRectF{-arrowEndOffset, 0.0, boxWidth, 0.0};
        verticalAlignment = Qt::AlignTop;
        break;

    case GettingStartedTip::Arrow::LeftCenter:
        g = QRectF{0.0, 0.0, boxWidth, 0.0};
        verticalAlignment = Qt::AlignVCenter;
        addArrowHeight = false;
        break;

    case GettingStartedTip::Arrow::RightCenter:
        g = QRectF{-(boxWidth + arrowDim1), 0.0, boxWidth, 0.0};
        verticalAlignment = Qt::AlignVCenter;
        addArrowHeight = false;
        break;

    case GettingStartedTip::Arrow::LeftTop:
        g = QRectF{0.0, topHeightOffset, boxWidth, 0.0};
        verticalAlignment = Qt::AlignTop;
        addArrowHeight = false;
        break;

    case GettingStartedTip::Arrow::NoArrow:
        g = QRectF{-halfBoxWidth, 0.0, boxWidth, 0.0};
        verticalAlignment = Qt::AlignVCenter;
        addArrowHeight = false;
        break;

    default:
        qWarning() << Q_FUNC_INFO << "unhandled:" << arrow;
        return {};
    }

    g.setHeight(addArrowHeight ? (contentHeight + arrowDim1) : contentHeight);

    switch (verticalAlignment) {
    case Qt::AlignBottom:
        g.moveBottom(0);
        break;

    case Qt::AlignTop:
        g.moveTop(0);
        break;

    default:
        g.moveTop(contentHeight * -0.5);
        break;
    }

    // grow the box symmetrically so its border isn't clipped to the
    // (otherwise exact-fitting) item bounds
    const double margin = marginForBorderWidth(borderWidth);
    g.adjust(-margin, -margin, margin, margin);

    return g;
}

QRectF TipBackgroundBox::contentGeometryForArrow(GettingStartedTip::Arrow arrow, double boxWidth, double borderWidth)
{
    // offset by the same margin boxGeometryForArrow() pads the box by, so
    // content lines up with the box's shape rather than its outer (padded) edge
    const double margin = marginForBorderWidth(borderWidth);
    QRectF g(margin, margin, boxWidth, 200.0); // height is a placeholder; callers size their own content

    switch (arrow) {
    case GettingStartedTip::Arrow::TopCenter:
    case GettingStartedTip::Arrow::TopLeft:
    case GettingStartedTip::Arrow::TopRight:
        g.moveTop(arrowDim1 + margin);
        break;

    case GettingStartedTip::Arrow::RightCenter:
        g.setWidth(boxWidth - arrowDim1);
        break;

    case GettingStartedTip::Arrow::LeftCenter:
        g.setWidth(boxWidth - arrowDim1);
        g.moveLeft(arrowDim1 + margin);
        break;

    default:
        break;
    }

    return g;
}

TipBackgroundBox::TipBackgroundBox(QQuickItem* pr) :
    QQuickPaintedItem(pr)
{
}

GettingStartedTip::Arrow TipBackgroundBox::arrowPosition() const
{
    return _arrow;
}

QColor TipBackgroundBox::borderColor() const
{
    return _borderColor;
}

int TipBackgroundBox::borderWidth() const
{
    return _borderWidth;
}

QColor TipBackgroundBox::fill() const
{
    return _fill;
}

void TipBackgroundBox::setArrowPosition(GettingStartedTip::Arrow arrow)
{
    if (_arrow == arrow)
        return;

    _arrow = arrow;
    emit arrowPositionChanged(_arrow);
    update();
}

void TipBackgroundBox::setBorderColor(QColor borderColor)
{
    if (_borderColor == borderColor)
        return;

    _borderColor = borderColor;
    emit borderColorChanged(_borderColor);
    update();
}

void TipBackgroundBox::setBorderWidth(int borderWidth)
{
    if (_borderWidth == borderWidth)
        return;

    _borderWidth = borderWidth;
    emit borderWidthChanged(_borderWidth);
    update();
}

void TipBackgroundBox::setFill(QColor fill)
{
    if (_fill == fill)
        return;

    _fill = fill;
    emit fillChanged(_fill);
    update();
}

void TipBackgroundBox::paint(QPainter *painter)
{
    // mirrors the margin boxGeometryForArrow() pads the item by, so the
    // shape is drawn inset from the (larger) item bounds, leaving room for
    // the stroked border to bleed outside the shape without being clipped
    const double margin = marginForBorderWidth(_borderWidth);
    QRectF g{margin, margin, width() - 2.0 * margin, height() - 2.0 * margin};

    QPainterPath pp = pathFromArrowAndGeometry(_arrow, g);
    painter->setBrush(_fill);
    painter->setPen(QPen{_borderColor, static_cast<double>(_borderWidth)});
    painter->drawPath(pp);
}
