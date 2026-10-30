#include "graphwidget.h"
#include "unitutil.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QtMath>
#include <cmath>
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
    emit viewChanged();
}

void GraphWidget::addDebugPoint(double timeSec, double value)
{
    appendPoint(m_debug, timeSec, value, m_maxPoints);
    update();
    emit viewChanged();
}

void GraphWidget::setDebugVisible(bool on)
{
    m_debugVisible = on;
    update();
}

void GraphWidget::setDebugLabel(const QString &label)
{
    m_debugLabel = label;
    update();
}

void GraphWidget::setWindowSeconds(double sec)
{
    if (sec > 1.0) {
        m_windowSec = sec;
        update();
        emit viewChanged();
    }
}

void GraphWidget::clearAll()
{
    m_meter.clear();
    m_debug.clear();
    m_follow    = true;
    m_viewStart = 0.0;
    update();
    emit viewChanged();
}

// ---------------------------------------------------------------- 보기 구간

double GraphWidget::firstTime() const
{
    double t = std::numeric_limits<double>::max();
    if (!m_meter.isEmpty())                   t = qMin(t, m_meter.first().x());
    if (m_debugVisible && !m_debug.isEmpty()) t = qMin(t, m_debug.first().x());
    return (t == std::numeric_limits<double>::max()) ? 0.0 : t;
}

double GraphWidget::lastTime() const
{
    double t = 0.0;
    if (!m_meter.isEmpty())                   t = qMax(t, m_meter.last().x());
    if (m_debugVisible && !m_debug.isEmpty()) t = qMax(t, m_debug.last().x());
    return t;
}

double GraphWidget::viewStart() const
{
    if (m_follow)
        return qMax(0.0, qMax(lastTime(), m_windowSec) - m_windowSec);
    return m_viewStart;
}

void GraphWidget::setFollowLatest(bool on)
{
    if (m_follow == on)
        return;
    m_follow = on;
    update();
    emit viewChanged();
}

void GraphWidget::setViewStart(double t)
{
    const double maxStart = qMax(0.0, lastTime() - m_windowSec);
    const double v = qBound(0.0, t, maxStart);

    // 오른쪽 끝까지 오면 다시 최신 구간을 따라간다.
    const bool atEnd = (v >= maxStart - 1e-6);

    if (!m_follow && qFuzzyCompare(m_viewStart + 1.0, v + 1.0) && !atEnd)
        return;

    m_viewStart = v;
    m_follow    = atEnd;
    update();
    emit viewChanged();
}

double GraphWidget::niceStep(double range, int targetTicks)
{
    if (range <= 0.0 || targetTicks < 1)
        return 1.0;

    const double raw  = range / targetTicks;
    const double mag  = std::pow(10.0, std::floor(std::log10(raw)));
    const double norm = raw / mag;

    double s;
    if      (norm <= 1.0) s = 1.0;
    else if (norm <= 2.0) s = 2.0;
    else if (norm <= 5.0) s = 5.0;
    else                  s = 10.0;

    return s * mag;
}

void GraphWidget::computeRange(double &xMin, double &xMax, double &xStep,
                               double &yMin, double &yMax, double &yStep) const
{
    if (m_follow) {
        xMax = qMax(lastTime(), m_windowSec);
        xMin = xMax - m_windowSec;
    } else {
        xMin = m_viewStart;
        xMax = xMin + m_windowSec;
    }
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

    if (yMin > yMax) {                          // 데이터 없음
        yMin = 0.0;
        yMax = 1.0;
    } else if (qFuzzyCompare(yMin, yMax)) {     // 값이 하나뿐이거나 모두 같음
        const double d = (qAbs(yMin) > 1e-12) ? qAbs(yMin) * 0.1 : 1.0;
        yMin -= d;
        yMax += d;
    } else {
        const double pad = (yMax - yMin) * 0.08;
        yMin -= pad;
        yMax += pad;
    }

    // 눈금이 200.0 / 220.0 처럼 떨어지도록 위/아래 경계를 눈금 간격의 배수로 맞춘다.
    yStep = niceStep(yMax - yMin, kGridY);
    if (yStep > 0.0) {
        const double eps = yStep * 1e-6;
        const double lo = std::floor((yMin + eps) / yStep) * yStep;
        const double hi = std::ceil ((yMax - eps) / yStep) * yStep;
        if (hi > lo) {
            yMin = lo;
            yMax = hi;
        }
    }

    // X 축(시간)은 표시 구간을 그대로 두고 눈금 위치만 정수에 맞춘다.
    xStep = niceStep(xMax - xMin, kGridX);
}

void GraphWidget::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        QWidget::wheelEvent(event);
        return;
    }
    // 한 칸에 보이는 구간의 20% 씩 좌우로 이동
    const double stepSec = m_windowSec * 0.2 * (delta > 0 ? -1.0 : 1.0);
    setViewStart(viewStart() + stepSec);
    event->accept();
}

void GraphWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging  = true;
        m_dragPixel = event->position().toPoint().x();
        m_dragStart = viewStart();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void GraphWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        const int plotW = qMax(10, width() - kMarginLeft - kMarginRight);
        const int dx    = event->position().toPoint().x() - m_dragPixel;
        setViewStart(m_dragStart - double(dx) * m_windowSec / plotW);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void GraphWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragging) {
        m_dragging = false;
        unsetCursor();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
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

    double xMin, xMax, xStep, yMin, yMax, yStep;
    computeRange(xMin, xMax, xStep, yMin, yMax, yStep);

    const double xSpan = (xMax - xMin) > 0 ? (xMax - xMin) : 1.0;
    const double ySpan = (yMax - yMin) > 0 ? (yMax - yMin) : 1.0;

    auto toPx = [&](const QPointF &pt) -> QPointF {
        const double x = plot.left()   + (pt.x() - xMin) / xSpan * plot.width();
        const double y = plot.bottom() - (pt.y() - yMin) / ySpan * plot.height();
        return QPointF(x, y);
    };

    // ---- 눈금 값 계산 ------------------------------------------------------
    //  Y : 축 전체가 하나의 단위 접두어(u, m, k ...)를 공유하도록 하고,
    //      눈금 간격에 맞춰 소수점 자리수를 정한다. (예: 200 / 220 / 240 u)
    int          yPrefixIdx = 4;
    const QString yPrefix   = UnitUtil::siPrefix(qMax(qAbs(yMin), qAbs(yMax)), yPrefixIdx);
    const double  yFactor   = UnitUtil::siFactor(yPrefixIdx);
    const double  yStepDisp = yStep / yFactor;
    const int     yDecimals = qBound(0, static_cast<int>(std::ceil(-std::log10(yStepDisp * 1.0001))), 4);

    const int     xDecimals = (xStep >= 1.0) ? 0 : 1;

    // ---- 격자 -------------------------------------------------------------
    const QPen gridPen(QColor(0xE2, 0xE6, 0xEA), 1);
    QFont f = p.font();
    f.setPointSize(8);
    p.setFont(f);

    // 세로 격자 + X 라벨 (눈금 간격의 배수 위치)
    if (xStep > 0.0) {
        const double first = std::ceil(xMin / xStep) * xStep;
        for (double t = first; t <= xMax + xStep * 1e-6; t += xStep) {
            const int x = qRound(plot.left() + (t - xMin) / (xMax - xMin) * plot.width());
            if (x < plot.left() || x > plot.right()) continue;

            p.setPen(gridPen);
            p.drawLine(x, plot.top(), x, plot.bottom());

            p.setPen(QColor(0x44, 0x4A, 0x50));
            p.drawText(QRect(x - 40, plot.bottom() + 4, 80, 18), Qt::AlignCenter,
                       QString::number(t, 'f', xDecimals) + QStringLiteral("s"));
        }
    }

    // 가로 격자 + Y 라벨
    if (yStep > 0.0) {
        const int count = qBound(1, qRound((yMax - yMin) / yStep), 20);
        for (int i = 0; i <= count; ++i) {
            const double v = yMin + yStep * i;
            const int    y = qRound(plot.bottom() - (v - yMin) / (yMax - yMin) * plot.height());

            p.setPen(gridPen);
            p.drawLine(plot.left(), y, plot.right(), y);

            QString s = QString::number(v / yFactor, 'f', yDecimals);
            if (!yPrefix.isEmpty()) s += QLatin1Char(' ') + yPrefix;

            p.setPen(QColor(0x44, 0x4A, 0x50));
            p.drawText(QRect(0, y - 9, kMarginLeft - 6, 18),
                       Qt::AlignRight | Qt::AlignVCenter, s);
        }
    }

    // ---- 축 ---------------------------------------------------------------
    p.setPen(QPen(QColor(0x77, 0x7F, 0x88), 1));
    p.drawRect(plot);

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
    int lx = plot.right() - 150;
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
        p.drawText(lx + 20, ly + 6, m_debugLabel);
    }

    // 과거 구간을 보고 있으면 표시
    if (!m_follow) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xF0, 0xC4, 0x19));
        const QRect badge(plot.left() + 8, plot.top() + 6, 104, 18);
        p.drawRoundedRect(badge, 3, 3);
        p.setPen(QColor(0x21, 0x25, 0x29));
        p.drawText(badge, Qt::AlignCenter, QStringLiteral("과거 구간 보기"));
    }

    if (m_meter.isEmpty() && (!m_debugVisible || m_debug.isEmpty())) {
        p.setPen(QColor(0x99, 0x99, 0x99));
        QFont bf = p.font();
        bf.setPointSize(11);
        p.setFont(bf);
        p.drawText(plot, Qt::AlignCenter, QStringLiteral("그래프 표시"));
    }
}
