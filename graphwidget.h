#ifndef GRAPHWIDGET_H
#define GRAPHWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPointF>

//
// QPainter 로 직접 그리는 실시간 그래프 위젯.
//   - Meter 값 : 파란색
//   - Debug 파싱 값 : 주황색 (ON/OFF 가능)
//   - X 축은 시작 시점부터의 경과 시간(초), 최근 구간만 스크롤 표시
//
class GraphWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GraphWidget(QWidget *parent = nullptr);

    void addMeterPoint(double timeSec, double value);
    void addDebugPoint(double timeSec, double value);

    void setDebugVisible(bool on);
    bool debugVisible() const { return m_debugVisible; }

    void setWindowSeconds(double sec);
    void clearAll();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void computeRange(double &xMin, double &xMax, double &yMin, double &yMax) const;
    static void appendPoint(QVector<QPointF> &v, double t, double val, int maxPoints);

    QVector<QPointF> m_meter;
    QVector<QPointF> m_debug;

    bool   m_debugVisible = true;
    double m_windowSec    = 60.0;
    int    m_maxPoints    = 20000;
};

#endif // GRAPHWIDGET_H
