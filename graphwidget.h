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

    // 범례에 표시할 이름 (예: "Debug (uA)")
    void setDebugLabel(const QString &label);

    void setWindowSeconds(double sec);
    void clearAll();

    // ---- 보기 구간 이동 (지나간 데이터 확인) -------------------------------
    double firstTime() const;                    // 보관 중인 가장 오래된 시각
    double lastTime() const;                     // 가장 최근 시각
    double windowSeconds() const { return m_windowSec; }
    double viewStart() const;                    // 현재 보이는 구간의 시작 시각
    bool   followLatest() const { return m_follow; }
    void   setFollowLatest(bool on);             // 최신 구간을 계속 따라가기
    void   setViewStart(double t);               // 지나간 구간으로 이동

signals:
    void viewChanged();                          // 데이터 추가 / 구간 이동 시

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void computeRange(double &xMin, double &xMax, double &xStep,
                      double &yMin, double &yMax, double &yStep) const;
    static void appendPoint(QVector<QPointF> &v, double t, double val, int maxPoints);

    // 눈금 간격을 1 / 2 / 5 x 10^n 형태의 "보기 좋은" 값으로 고른다.
    static double niceStep(double range, int targetTicks);

    QVector<QPointF> m_meter;
    QVector<QPointF> m_debug;

    QString m_debugLabel  = QStringLiteral("Debug");
    bool   m_debugVisible = true;
    double m_windowSec    = 60.0;
    int    m_maxPoints    = 20000;

    bool   m_follow       = true;    // true = 최신 구간을 따라간다
    double m_viewStart    = 0.0;     // follow 가 아닐 때 보이는 구간의 시작
    bool   m_dragging     = false;
    int    m_dragPixel    = 0;
    double m_dragStart    = 0.0;
};

#endif // GRAPHWIDGET_H
