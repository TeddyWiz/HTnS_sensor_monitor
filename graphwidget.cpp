#include "graphwidget.h"
#include "unitutil.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QFontMetrics>
#include <QtMath>
#include <limits>

namespace {
constexpr int kMarginLeft   = 76;
constexpr int kMarginRight  = 16;
constexpr int kMarginTop    = 16;
constexpr int kMarginBottom = 30;
constexpr int kGridX        = 6;
constexpr int kGridY        = 5;
}

GraphWidget::GraphWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 200);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::white);
    setPalette(pal);
}

void GraphWidget::appendPoint(QVector<QPointF> &v, double t, double val, int maxPoints)
{
    v.append(QPointF(t, val));
    if (v.size() > maxPoints)
        v.remove(0, v.size() - maxPoints);
}

void GraphWidget::addMeterPoint(double timeSec, double value)
{
    appendPoint(m_meter, timeSec, value, m_maxPoints);
    update();
}

void GraphWidget::addDebugPoint(double timeSec, double value)
{
    appendPoint(m_debug, timeSec, value, m_maxPoints);
    update();
}

void GraphWidget::setDebugVisible(bool on)
{
    m_debugVisible = on;
    update();
}

void GraphWidget::setWindowSeconds(double sec)
{
    if (sec > 1.0) {
        m_windowSec = sec;
        update();
    }
}

void GraphWidget::clearAll()
{
    m_meter.clear();
    m_debug.clear();
    update();
}

void GraphWidget::computeRange(double &xMin, double &xMax, double &yMin, double &yMax) const
{
    double lastT = 0.0;
    if (!m_meter.isEmpty())                    lastT = qMax(lastT, m_meter.last().x());
    if (m_debugVisible && !m_debug.isEmpty())  lastT = qMax(lastT, m_debug.last().x());

    xMax = qMax(lastT, m_windowSec);
    xMin = xMax - m_windowSec;
    if (xMin < 0.0) { xMin = 0.0; xMax = m_windowSec; }

    yMin =  std::numeric_limits<double>::max();
    yMax = -std::numeric_limits<double>::max();

    auto scan = [&](const QVector<QPointF> &v) {
        for (const QPointF &p : v) {
            if (p.x() < xMin) continue;
            yMin = qMin(yMin, p.y());
            yMax = qMax(yMax, p.y());
        }
    };
    scan(m_meter);
    if (m_debugVisible) scan(m_debug);

    if (yMin > yMax) {          // 데이터 없음
        yMin = 0.0;
        yMax = 1.0;
        return;
    }
    if (qFuzzyCompare(yMin, yMax)) {
        const double d = (qAbs(yMin) > 1e-12) ? qAbs(yMin) * 0.1 : 1.0;
        yMin -= d;
        yMax += d;
        return;
    }
    const double pad = (yMax - yMin) * 0.08;
    yMin -= pad;
    yMax += pad;
}

void GraphWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), Qt::white);

    const QRect plot(kMarginLeft,
                     kMarginTop,
                     qMax(10, width()  - kMarginLeft - kMarginRight),
                     qMax(10, height() - kMarginTop  - kMarginBottom));

    double xMin, xMax, yMin, yMax;
    computeRange(xMin, xMax, yMin, yMax);

    const double xSpan = (xMax - xMin) > 0 ? (xMax - xMin) : 1.0;
    const double ySpan = (yMax - yMin) > 0 ? (yMax - yMin) : 1.0;

    auto toPx = [&](const QPointF &pt) -> QPointF {
        const double x = plot.left()   + (pt.x() - xMin) / xSpan * plot.width();
        const double y = plot.bottom() - (pt.y() - yMin) / ySpan * plot.height();
        return QPointF(x, y);
    };

    // ---- 격자 -------------------------------------------------------------
    p.setPen(QPen(QColor(0xE2, 0xE6, 0xEA), 1));
    for (int i = 0; i <= kGridX; ++i) {
        const int x = plot.left() + plot.width() * i / kGridX;
        p.drawLine(x, plot.top(), x, plot.bottom());
    }
    for (int i = 0; i <= kGridY; ++i) {
        const int y = plot.top() + plot.height() * i / kGridY;
        p.drawLine(plot.left(), y, plot.right(), y);
    }

    // ---- 축 ---------------------------------------------------------------
    p.setPen(QPen(QColor(0x77, 0x7F, 0x88), 1));
    p.drawRect(plot);

    // ---- 눈금 라벨 ---------------------------------------------------------
    p.setPen(QColor(0x44, 0x4A, 0x50));
    QFont f = p.font();
    f.setPointSize(8);
    p.setFont(f);

    for (int i = 0; i <= kGridY; ++i) {
        const double v = yMax - (yMax - yMin) * i / kGridY;
        const int    y = plot.top() + plot.height() * i / kGridY;
        const QString s = UnitUtil::formatValue(v, 3);
        p.drawText(QRect(0, y - 9, kMarginLeft - 6, 18),
                   Qt::AlignRight | Qt::AlignVCenter, s);
    }
    for (int i = 0; i <= kGridX; ++i) {
        const double t = xMin + (xMax - xMin) * i / kGridX;
        const int    x = plot.left() + plot.width() * i / kGridX;
        const QString s = QString::number(t, 'f', 1) + QStringLiteral("s");
        p.drawText(QRect(x - 34, plot.bottom() + 4, 68, 18),
                   Qt::AlignCenter, s);
    }

    // ---- 데이터 -----------------------------------------------------------
    auto drawSeries = [&](const QVector<QPointF> &v, const QColor &c) {
        if (v.size() < 1) return;
        QPainterPath path;
        bool started = false;
        for (const QPointF &pt : v) {
            if (pt.x() < xMin) continue;
            const QPointF q = toPx(pt);
            if (!started) { path.moveTo(q); started = true; }
            else            path.lineTo(q);
        }
        if (!started) return;
        p.setPen(QPen(c, 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);

        // 마지막 점 강조
        const QPointF last = toPx(v.last());
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawEllipse(last, 3.0, 3.0);
    };

    const QColor kMeterColor(0x1F, 0x6F, 0xEB);
    const QColor kDebugColor(0xE8, 0x7A, 0x14);

    p.save();
    p.setClipRect(plot.adjusted(1, 1, -1, -1));
    drawSeries(m_meter, kMeterColor);
    if (m_debugVisible)
        drawSeries(m_debug, kDebugColor);
    p.restore();

    // ---- 범례 -------------------------------------------------------------
    int lx = plot.right() - 130;
    int ly = plot.top() + 8;

    p.setPen(Qt::NoPen);
    p.setBrush(kMeterColor);
    p.drawRect(lx, ly, 14, 3);
    p.setPen(QColor(0x33, 0x33, 0x33));
    p.drawText(lx + 20, ly + 6, QStringLiteral("Meter"));

    if (m_debugVisible) {
        ly += 16;
        p.setPen(Qt::NoPen);
        p.setBrush(kDebugColor);
        p.drawRect(lx, ly, 14, 3);
        p.setPen(QColor(0x33, 0x33, 0x33));
        p.drawText(lx + 20, ly + 6, QStringLiteral("Debug"));
    }

    if (m_meter.isEmpty() && (!m_debugVisible || m_debug.isEmpty())) {
        p.setPen(QColor(0x99, 0x99, 0x99));
        QFont bf = p.font();
        bf.setPointSize(11);
        p.setFont(bf);
        p.drawText(plot, Qt::AlignCenter, QStringLiteral("그래프 표시"));
    }
}
