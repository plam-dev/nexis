#pragma once
#include <QPainterPath>
#include <QSizeF>

namespace Nexis {
inline QPainterPath notchPath(const QSizeF &size) {
    const qreal w = size.width(), h = size.height();
    const qreal shoulder = qMin(24.0, qMin(w / 8, h / 3));
    const qreal radius = qMin(28.0, h / 2);
    QPainterPath path;
    path.moveTo(0, 0);
    path.lineTo(w, 0);
    path.cubicTo(w - shoulder, 0, w - shoulder, shoulder / 2, w - shoulder, shoulder);
    path.lineTo(w - shoulder, h - radius);
    path.quadTo(w - shoulder, h, w - shoulder - radius, h);
    path.lineTo(shoulder + radius, h);
    path.quadTo(shoulder, h, shoulder, h - radius);
    path.lineTo(shoulder, shoulder);
    path.cubicTo(shoulder, shoulder / 2, shoulder, 0, 0, 0);
    path.closeSubpath();
    return path;
}
} // namespace Nexis
