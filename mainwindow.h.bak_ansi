#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

#include "serialsettingsdialog.h"

class QPushButton;
class QLabel;
class QPlainTextEdit;
class QLineEdit;
class QSpinBox;
class QSerialPort;
class QTimer;
class GraphWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onDebugConnectClicked();
    void onMeterConnectClicked();
    void onParsingSettingsClicked();
    void onDebugGraphToggleClicked();
    void onStartClicked();

    void onDebugReadyRead();
    void onMeterReadyRead();
    void onDebugCommandEntered();

    void onPollTimeout();
    void onIdnTimeout();

    void onDebugPortError(QSerialPort::SerialPortError err);
    void onMeterPortError(QSerialPort::SerialPortError err);

private:
    enum class MeterState { Idle, WaitIdn, Polling };

    void buildUi();

    // 시리얼
    void toggleConnection(QSerialPort *port,
                          SerialSettings &settings,
                          QPushButton *btn,
                          QLabel *lbl,
                          const QString &title,
                          const QString &btnConnectText,
                          const QString &btnDisconnectText);
    void updatePortLabel(QLabel *lbl, QSerialPort *port, const QString &prefix);

    // 수신 처리
    void handleIncoming(QByteArray &buffer, const QByteArray &chunk,
                        bool isDebug);
    void processDebugLine(const QString &line);
    void processMeterLine(const QString &line);

    // 로그 / 파일
    void appendDebugLog(const QString &text);
    void appendMeterLog(const QString &text);
    bool openLogFiles();
    void closeLogFiles();
    void writeCsvRow(const QDateTime &ts, double elapsed, double meterValue,
                     const QString &meterConverted, const QString &meterUnit);

    // 계측 제어
    void startMeasure();
    void stopMeasure();
    void setStartButtonState(int state);  // 0=시작(초기) 1=진행중(녹색) 2=시작(노랑)

    static double parseNumberAfterKeyword(const QString &line,
                                          const QString &keyword,
                                          bool *ok);

    // ---------------- UI ----------------
    QPushButton    *m_btnDebugConn   = nullptr;
    QLabel         *m_lblDebugPort   = nullptr;
    QPushButton    *m_btnParsing     = nullptr;
    QPushButton    *m_btnDebugGraph  = nullptr;

    QPushButton    *m_btnMeterConn   = nullptr;
    QLabel         *m_lblMeterPort   = nullptr;
    QSpinBox       *m_spPeriod       = nullptr;
    QPushButton    *m_btnStart       = nullptr;

    QPlainTextEdit *m_txtDebug       = nullptr;
    QPlainTextEdit *m_txtMeter       = nullptr;
    QLineEdit      *m_edDebugCmd     = nullptr;
    QLabel         *m_lblMeterValue  = nullptr;
    GraphWidget    *m_graph          = nullptr;

    // ---------------- 시리얼 ----------------
    QSerialPort    *m_debugPort      = nullptr;
    QSerialPort    *m_meterPort      = nullptr;
    SerialSettings  m_debugSettings;
    SerialSettings  m_meterSettings;
    QByteArray      m_debugBuf;
    QByteArray      m_meterBuf;

    // ---------------- 동작 상태 ----------------
    QTimer         *m_pollTimer      = nullptr;
    QTimer         *m_idnTimer       = nullptr;
    MeterState      m_meterState     = MeterState::Idle;
    bool            m_running        = false;
    bool            m_idnMatched     = false;
    bool            m_promptSeen     = false;
    bool            m_debugGraphOn   = true;
    bool            m_waitingValue   = false;

    QString         m_parseKeyword;
    QElapsedTimer   m_elapsed;

    bool            m_hasDebugValue  = false;
    double          m_lastDebugValue = 0.0;

    // ---------------- 파일 ----------------
    QFile           m_txtFile;
    QTextStream     m_txtStream;
    QFile           m_csvFile;
    QTextStream     m_csvStream;
    QString         m_logDir;

    QRegularExpression m_reNumber;
};

#endif // MAINWINDOW_H
