#include "mainwindow.h"
#include "graphwidget.h"
#include "parsingsettingsdialog.h"
#include "unitutil.h"

#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QSerialPort>
#include <QTimer>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QScrollBar>
#include <QStatusBar>
#include <QScrollBar>
#include <QToolButton>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextBlock>
#include <QTextDocument>
#include <cmath>

namespace {

// 공통 스타일
const char *kStyleIdle       = "QPushButton{background:#E9ECEF;color:#212529;border:1px solid #ADB5BD;"
                               "border-radius:4px;padding:6px;font-weight:bold;}"
                               "QPushButton:hover{background:#DEE2E6;}";
const char *kStyleGreen      = "QPushButton{background:#2FA84F;color:white;border:1px solid #248C40;"
                               "border-radius:4px;padding:6px;font-weight:bold;}"
                               "QPushButton:hover{background:#279445;}";
const char *kStyleYellow     = "QPushButton{background:#F0C419;color:#212529;border:1px solid #C9A315;"
                               "border-radius:4px;padding:6px;font-weight:bold;}"
                               "QPushButton:hover{background:#E0B615;}";
const char *kStyleRed        = "QPushButton{background:#D9534F;color:white;border:1px solid #B5423E;"
                               "border-radius:4px;padding:6px;font-weight:bold;}"
                               "QPushButton:hover{background:#C64541;}";

const char *kLblConnected    = "QLabel{background:#2FA84F;color:white;border:1px solid #248C40;"
                               "border-radius:4px;padding:6px;font-weight:bold;}";
const char *kLblDisconnected = "QLabel{background:#D9534F;color:white;border:1px solid #B5423E;"
                               "border-radius:4px;padding:6px;font-weight:bold;}";

const char *kNumberPattern   = "[-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?";

const char *kExpectedIdn     = "FLUKE,45,7940026,1.7D2.0";   // 공백 제거 비교용
const char *kPrompt          = "=>";

// Meter 명령 종료 문자.
//  <CR> = 윈도우의 엔터키 이므로 CR+LF(0x0D 0x0A) 를 전송한다.
const char *kCmdTerminator   = "\r\n";

// 표시/저장 소수점 자리수
constexpr int kDisplayDecimals = 3;   // 화면의 "현재 Meter 값"
constexpr int kCsvDecimals     = 4;   // CSV 의 Meter값(변환)

// Debug 파싱 값의 단위.
//  Debug 로그에서 읽은 숫자는 uA(마이크로암페어) 이므로,
//  Meter 값과 같은 기준 단위(A)로 바꿔서 그래프에 그린다. (1 uA = 1e-6 A)
const char  *kDebugUnitName  = "uA";   // Debug 로그에 찍히는 값의 단위
const char  *kDebugBaseUnit  = "A";    // 환산 후 기준 단위
constexpr double kDebugUnitScale = 1e-6;

QString nowStamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
}

QString timeStamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
}

// CSV 에 기록할 숫자 문자열.
//  지수 표기(5.3e-05)로 쓰면 Excel 이 문자열로 인식하는 경우가 있어
//  항상 일반 소수 표기(0.000053)로 기록한다.
QString csvNumber(double v)
{
    if (v == 0.0)
        return QStringLiteral("0");

    // 유효숫자 12자리가 되도록 소수점 자리수를 정한다.
    //  (고정 자리수로 쓰면 1234.5678 이 1234.567800000000034 처럼 기록된다)
    const int exp10    = static_cast<int>(std::floor(std::log10(std::fabs(v))));
    int       decimals = 12 - exp10 - 1;
    if (decimals < 0)  decimals = 0;
    if (decimals > 20) decimals = 20;

    QString s = QString::number(v, 'f', decimals);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0'))) s.chop(1);
        if (s.endsWith(QLatin1Char('.')))    s.chop(1);
    }
    if (s == QLatin1String("-0")) s = QStringLiteral("0");
    return s;
}

QString squeeze(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (QChar c : s)
        if (!c.isSpace()) out += c.toUpper();
    return out;
}

} // namespace

// ================================================================= ctor / dtor

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_reNumber(QString::fromLatin1("^%1$").arg(QString::fromLatin1(kNumberPattern)))
{
    setWindowTitle(QStringLiteral("MeterMonitor - Fluke 45 / Debug Serial Monitor"));
    resize(1180, 760);

    m_debugPort = new QSerialPort(this);
    m_meterPort = new QSerialPort(this);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setTimerType(Qt::PreciseTimer);

    m_idnTimer = new QTimer(this);
    m_idnTimer->setSingleShot(true);
    m_idnTimer->setInterval(3000);

    m_logDir = QCoreApplication::applicationDirPath() + QStringLiteral("/log");

    buildUi();

    connect(m_debugPort, &QSerialPort::readyRead, this, &MainWindow::onDebugReadyRead);
    connect(m_meterPort, &QSerialPort::readyRead, this, &MainWindow::onMeterReadyRead);
    connect(m_debugPort, &QSerialPort::errorOccurred, this, &MainWindow::onDebugPortError);
    connect(m_meterPort, &QSerialPort::errorOccurred, this, &MainWindow::onMeterPortError);
    connect(m_pollTimer, &QTimer::timeout, this, &MainWindow::onPollTimeout);
    connect(m_idnTimer,  &QTimer::timeout, this, &MainWindow::onIdnTimeout);

    statusBar()->showMessage(QStringLiteral("로그 저장 폴더 : ") + QDir::toNativeSeparators(m_logDir));
}

MainWindow::~MainWindow()
{
    closeLogFiles();
    if (m_debugPort->isOpen()) m_debugPort->close();
    if (m_meterPort->isOpen()) m_meterPort->close();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (m_running)
        stopMeasure();
    closeLogFiles();
    e->accept();
}

// ================================================================= UI

void MainWindow::buildUi()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    // ---------------- 상단 버튼 (2행 x 4열) --------------------------------
    m_btnDebugConn  = new QPushButton(QStringLiteral("Debug 시리얼 연결"), central);
    m_lblDebugPort  = new QLabel(QStringLiteral("Debug 연결 해제"), central);
    m_btnParsing    = new QPushButton(QStringLiteral("디버그 parsing 설정"), central);
    m_btnDebugGraph = new QPushButton(QStringLiteral("Debug 그래프 표시\nON"), central);

    m_btnMeterConn  = new QPushButton(QStringLiteral("Meter 시리얼 연결"), central);
    m_lblMeterPort  = new QLabel(QStringLiteral("Meter 연결 해제"), central);
    m_btnStart      = new QPushButton(QStringLiteral("시작"), central);

    QWidget *periodBox   = new QWidget(central);
    QLabel  *lblPeriod   = new QLabel(QStringLiteral("Meter 데이터 요청시간\n(100ms) 단위 입력"), periodBox);
    lblPeriod->setAlignment(Qt::AlignCenter);
    m_spPeriod = new QSpinBox(periodBox);
    m_spPeriod->setRange(1, 6000);          // 100ms ~ 600s
    m_spPeriod->setValue(10);               // 기본 1초
    m_spPeriod->setSuffix(QStringLiteral(" x100ms"));
    m_spPeriod->setAlignment(Qt::AlignCenter);
    QVBoxLayout *periodLay = new QVBoxLayout(periodBox);
    periodLay->setContentsMargins(4, 2, 4, 2);
    periodLay->setSpacing(2);
    periodLay->addWidget(lblPeriod);
    periodLay->addWidget(m_spPeriod);

    for (QPushButton *b : { m_btnDebugConn, m_btnParsing, m_btnMeterConn }) {
        b->setMinimumHeight(46);
        b->setStyleSheet(QString::fromLatin1(kStyleIdle));
    }
    m_btnStart->setMinimumHeight(46);
    m_btnDebugGraph->setMinimumHeight(46);

    for (QLabel *l : { m_lblDebugPort, m_lblMeterPort }) {
        l->setMinimumHeight(46);
        l->setAlignment(Qt::AlignCenter);
        l->setStyleSheet(QString::fromLatin1(kLblDisconnected));
    }

    m_btnDebugGraph->setStyleSheet(QString::fromLatin1(kStyleGreen));   // 초기값 ON = 녹색
    setStartButtonState(0);

    QGridLayout *top = new QGridLayout;
    top->setHorizontalSpacing(10);
    top->setVerticalSpacing(8);
    top->addWidget(m_btnDebugConn,  0, 0);
    top->addWidget(m_lblDebugPort,  0, 1);
    top->addWidget(m_btnParsing,    0, 2);
    top->addWidget(m_btnDebugGraph, 0, 3);
    top->addWidget(m_btnMeterConn,  1, 0);
    top->addWidget(m_lblMeterPort,  1, 1);
    top->addWidget(periodBox,       1, 2);
    top->addWidget(m_btnStart,      1, 3);
    for (int c = 0; c < 4; ++c)
        top->setColumnStretch(c, 1);

    // ---------------- 좌측 : Debug 로그 + Meter 로그 + 명령 입력 -----------
    m_txtDebug = new QPlainTextEdit(central);
    m_txtDebug->setReadOnly(true);
    m_txtDebug->setMaximumBlockCount(5000);
    m_txtDebug->setStyleSheet(QStringLiteral("QPlainTextEdit{background:#101418;color:#E6E6E6;}"));
    m_txtDebug->setFont(QFont(QStringLiteral("Consolas"), 9));
    m_txtDebug->setPlaceholderText(QStringLiteral("Debug 내용 표시"));

    m_txtMeter = new QPlainTextEdit(central);
    m_txtMeter->setReadOnly(true);
    m_txtMeter->setMaximumBlockCount(5000);
    m_txtMeter->setStyleSheet(QStringLiteral("QPlainTextEdit{background:#101418;color:#9CDCFE;}"));
    m_txtMeter->setFont(QFont(QStringLiteral("Consolas"), 9));
    m_txtMeter->setPlaceholderText(QStringLiteral("Meter Log"));

    QGroupBox *gbDebug = new QGroupBox(QStringLiteral("Debug 내용 표시"), central);
    QVBoxLayout *dl = new QVBoxLayout(gbDebug);
    dl->setContentsMargins(6, 6, 6, 6);
    dl->addWidget(m_txtDebug);

    QGroupBox *gbMeter = new QGroupBox(QStringLiteral("Meter Log (송수신)"), central);
    QVBoxLayout *ml = new QVBoxLayout(gbMeter);
    ml->setContentsMargins(6, 6, 6, 6);
    ml->addWidget(m_txtMeter);

    QSplitter *logSplit = new QSplitter(Qt::Horizontal, central);
    logSplit->addWidget(gbDebug);
    logSplit->addWidget(gbMeter);
    logSplit->setStretchFactor(0, 3);
    logSplit->setStretchFactor(1, 2);

    m_edDebugCmd = new QLineEdit(central);
    m_edDebugCmd->setPlaceholderText(QStringLiteral("Debug 명령 입력창 - 입력 후 Enter 를 누르면 Debug 시리얼로 전송합니다."));
    m_edDebugCmd->setMinimumHeight(28);

    QWidget *leftPane = new QWidget(central);
    QVBoxLayout *leftLay = new QVBoxLayout(leftPane);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(6);
    leftLay->addWidget(logSplit, 1);
    leftLay->addWidget(m_edDebugCmd);

    // ---------------- 우측 : 현재 Meter 값 + 그래프 -------------------------
    m_lblMeterValue = new QLabel(QStringLiteral("현재 Meter 값 표시 (단위 변환)"), central);
    m_lblMeterValue->setAlignment(Qt::AlignCenter);
    m_lblMeterValue->setMinimumHeight(52);
    m_lblMeterValue->setStyleSheet(QStringLiteral(
        "QLabel{background:#F8F9FA;border:1px solid #ADB5BD;border-radius:4px;"
        "font-size:20px;font-weight:bold;color:#212529;}"));

    m_graph = new GraphWidget(central);
    m_graph->setDebugLabel(QStringLiteral("Debug (") + QString::fromLatin1(kDebugUnitName) + QLatin1Char(')'));

    // 지나간 구간을 확인할 수 있는 가로 스크롤
    m_graphScroll = new QScrollBar(Qt::Horizontal, central);
    m_graphScroll->setRange(0, 0);
    m_graphScroll->setEnabled(false);

    m_btnGraphLatest = new QToolButton(central);
    m_btnGraphLatest->setText(QStringLiteral("최신 ▶|"));
    m_btnGraphLatest->setToolTip(QStringLiteral("최신 구간으로 이동하고 계속 따라갑니다."));

    m_lblGraphRange = new QLabel(central);
    m_lblGraphRange->setMinimumWidth(150);
    m_lblGraphRange->setAlignment(Qt::AlignCenter);
    m_lblGraphRange->setStyleSheet(QStringLiteral("QLabel{color:#5A6470;}"));

    QHBoxLayout *scrollLay = new QHBoxLayout;
    scrollLay->setContentsMargins(0, 0, 0, 0);
    scrollLay->setSpacing(6);
    scrollLay->addWidget(m_graphScroll, 1);
    scrollLay->addWidget(m_lblGraphRange);
    scrollLay->addWidget(m_btnGraphLatest);

    QGroupBox *gbGraph = new QGroupBox(QStringLiteral("그래프 표시"), central);
    QVBoxLayout *gl = new QVBoxLayout(gbGraph);
    gl->setContentsMargins(6, 6, 6, 6);
    gl->setSpacing(4);
    gl->addWidget(m_graph, 1);
    gl->addLayout(scrollLay);

    QWidget *rightPane = new QWidget(central);
    QVBoxLayout *rightLay = new QVBoxLayout(rightPane);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(6);
    rightLay->addWidget(m_lblMeterValue);
    rightLay->addWidget(gbGraph, 1);

    QSplitter *mainSplit = new QSplitter(Qt::Horizontal, central);
    mainSplit->addWidget(leftPane);
    mainSplit->addWidget(rightPane);
    mainSplit->setStretchFactor(0, 4);
    mainSplit->setStretchFactor(1, 5);

    QVBoxLayout *root = new QVBoxLayout(central);
    root->setContentsMargins(10, 10, 10, 6);
    root->setSpacing(8);
    root->addLayout(top);
    root->addWidget(mainSplit, 1);

    // ---------------- 시그널 ------------------------------------------------
    connect(m_btnDebugConn,  &QPushButton::clicked, this, &MainWindow::onDebugConnectClicked);
    connect(m_btnMeterConn,  &QPushButton::clicked, this, &MainWindow::onMeterConnectClicked);
    connect(m_btnParsing,    &QPushButton::clicked, this, &MainWindow::onParsingSettingsClicked);
    connect(m_btnDebugGraph, &QPushButton::clicked, this, &MainWindow::onDebugGraphToggleClicked);
    connect(m_btnStart,      &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(m_edDebugCmd,    &QLineEdit::returnPressed, this, &MainWindow::onDebugCommandEntered);
    connect(m_graphScroll,   &QScrollBar::valueChanged, this, &MainWindow::onGraphScrolled);
    connect(m_graph,         &GraphWidget::viewChanged, this, &MainWindow::onGraphViewChanged);
    connect(m_btnGraphLatest, &QToolButton::clicked, this, [this]() {
        m_graph->setFollowLatest(true);
    });

    onGraphViewChanged();
}

// ================================================================= 시리얼 연결

void MainWindow::updatePortLabel(QLabel *lbl, QSerialPort *port, const QString &prefix)
{
    if (port->isOpen()) {
        lbl->setText(prefix + QStringLiteral(" 연결됨\n") + port->portName()
                     + QStringLiteral(" / ") + QString::number(port->baudRate()) + QStringLiteral(" bps"));
        lbl->setStyleSheet(QString::fromLatin1(kLblConnected));
    } else {
        lbl->setText(prefix + QStringLiteral(" 연결 해제"));
        lbl->setStyleSheet(QString::fromLatin1(kLblDisconnected));
    }
}

void MainWindow::toggleConnection(QSerialPort *port,
                                  SerialSettings &settings,
                                  QPushButton *btn,
                                  QLabel *lbl,
                                  const QString &title,
                                  const QString &btnConnectText,
                                  const QString &btnDisconnectText)
{
    // 이미 열려 있으면 -> 연결 해제
    if (port->isOpen()) {
        port->close();
        btn->setText(btnConnectText);
        btn->setStyleSheet(QString::fromLatin1(kStyleIdle));
        updatePortLabel(lbl, port, title);

        if (port == m_meterPort && m_running)
            stopMeasure();
        return;
    }

    // 설정 창 -> 연결
    SerialSettingsDialog dlg(title + QStringLiteral(" 시리얼 포트 설정"), settings, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    settings = dlg.settings();

    port->setPortName(settings.portName);
    port->setBaudRate(settings.baudRate);
    port->setDataBits(settings.dataBits);
    port->setParity(settings.parity);
    port->setStopBits(settings.stopBits);
    port->setFlowControl(settings.flowControl);

    if (!port->open(QIODevice::ReadWrite)) {
        QMessageBox::critical(this, title,
                              QStringLiteral("시리얼 포트를 열 수 없습니다.\n%1\n\n사유 : %2")
                              .arg(settings.portName, port->errorString()));
        updatePortLabel(lbl, port, title);
        return;
    }

    port->clear();
    port->setDataTerminalReady(true);
    port->setRequestToSend(true);

    btn->setText(btnDisconnectText);
    btn->setStyleSheet(QString::fromLatin1(kStyleGreen));
    updatePortLabel(lbl, port, title);

    const QString msg = QStringLiteral("[%1] %2 연결 : %3")
            .arg(timeStamp(), title, settings.summary());
    if (port == m_debugPort) appendDebugLog(msg);
    else                     appendMeterLog(msg);
}

void MainWindow::onDebugConnectClicked()
{
    toggleConnection(m_debugPort, m_debugSettings, m_btnDebugConn, m_lblDebugPort,
                     QStringLiteral("Debug"),
                     QStringLiteral("Debug 시리얼 연결"),
                     QStringLiteral("Debug 시리얼 연결 해제"));
}

void MainWindow::onMeterConnectClicked()
{
    toggleConnection(m_meterPort, m_meterSettings, m_btnMeterConn, m_lblMeterPort,
                     QStringLiteral("Meter"),
                     QStringLiteral("Meter 시리얼 연결"),
                     QStringLiteral("Meter 시리얼 연결 해제"));
}

void MainWindow::onDebugPortError(QSerialPort::SerialPortError err)
{
    if (err == QSerialPort::NoError) return;
    if (err == QSerialPort::ResourceError && m_debugPort->isOpen()) {
        appendDebugLog(QStringLiteral("[%1] Debug 포트 오류 : %2")
                       .arg(timeStamp(), m_debugPort->errorString()));
        m_debugPort->close();
        m_btnDebugConn->setText(QStringLiteral("Debug 시리얼 연결"));
        m_btnDebugConn->setStyleSheet(QString::fromLatin1(kStyleIdle));
        updatePortLabel(m_lblDebugPort, m_debugPort, QStringLiteral("Debug"));
    }
}

void MainWindow::onMeterPortError(QSerialPort::SerialPortError err)
{
    if (err == QSerialPort::NoError) return;
    if (err == QSerialPort::ResourceError && m_meterPort->isOpen()) {
        appendMeterLog(QStringLiteral("[%1] Meter 포트 오류 : %2")
                       .arg(timeStamp(), m_meterPort->errorString()));
        if (m_running) stopMeasure();
        m_meterPort->close();
        m_btnMeterConn->setText(QStringLiteral("Meter 시리얼 연결"));
        m_btnMeterConn->setStyleSheet(QString::fromLatin1(kStyleIdle));
        updatePortLabel(m_lblMeterPort, m_meterPort, QStringLiteral("Meter"));
    }
}

// ================================================================= 파싱 / 그래프 설정

void MainWindow::onParsingSettingsClicked()
{
    ParsingSettingsDialog dlg(m_parseKeyword, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    m_parseKeyword = dlg.keyword();
    appendDebugLog(QStringLiteral("[%1] 파싱 시작 단어 = \"%2\"")
                   .arg(timeStamp(), m_parseKeyword));
}

void MainWindow::onDebugGraphToggleClicked()
{
    m_debugGraphOn = !m_debugGraphOn;

    m_btnDebugGraph->setText(m_debugGraphOn
                             ? QStringLiteral("Debug 그래프 표시\nON")
                             : QStringLiteral("Debug 그래프 표시\nOFF"));
    m_btnDebugGraph->setStyleSheet(QString::fromLatin1(m_debugGraphOn ? kStyleGreen : kStyleRed));
    m_graph->setDebugVisible(m_debugGraphOn);
}

// ================================================================= 그래프 스크롤

void MainWindow::onGraphScrolled(int value)
{
    if (m_scrollSyncing)
        return;

    // 스크롤바 단위는 0.1 초
    m_graph->setViewStart(m_graph->firstTime() + value / 10.0);
}

void MainWindow::onGraphViewChanged()
{
    if (!m_graphScroll)
        return;

    const double first = m_graph->firstTime();
    const double last  = m_graph->lastTime();
    const double win   = m_graph->windowSeconds();
    const double span  = qMax(0.0, (last - first) - win);   // 스크롤 가능한 길이(초)

    m_scrollSyncing = true;
    m_graphScroll->setRange(0, qRound(span * 10.0));
    m_graphScroll->setPageStep(qMax(1, qRound(win * 10.0)));
    m_graphScroll->setSingleStep(qMax(1, qRound(win * 1.0)));   // 보이는 구간의 약 10%
    m_graphScroll->setEnabled(span > 0.0);
    m_graphScroll->setValue(m_graph->followLatest()
                            ? m_graphScroll->maximum()
                            : qRound((m_graph->viewStart() - first) * 10.0));
    m_scrollSyncing = false;

    const double vs = m_graph->viewStart();
    m_lblGraphRange->setText(QStringLiteral("%1 ~ %2 s%3")
                             .arg(vs, 0, 'f', 1)
                             .arg(vs + win, 0, 'f', 1)
                             .arg(m_graph->followLatest() ? QString() : QStringLiteral("  (정지)")));

    m_btnGraphLatest->setEnabled(!m_graph->followLatest());
}

// ================================================================= 로그 출력

// ---------------------------------------------------------------- ANSI (Tera Term 스타일)
namespace {
// Tera Term 기본 팔레트 (0~7 일반, 8~15 밝은색)
const QColor kAnsiColors[16] = {
    QColor(  0,   0,   0), QColor(255,   0,   0), QColor(  0, 255,   0), QColor(255, 255,   0),
    QColor(128, 128, 255), QColor(255,   0, 255), QColor(  0, 255, 255), QColor(255, 255, 255),
    QColor(128, 128, 128), QColor(255,  96,  96), QColor(128, 255, 128), QColor(255, 255, 128),
    QColor(160, 160, 255), QColor(255, 128, 255), QColor(128, 255, 255), QColor(255, 255, 255)
};

QColor ansi256(int n)
{
    if (n < 0 || n > 255) return QColor();
    if (n < 16) return kAnsiColors[n];
    if (n >= 232) { const int g = 8 + (n - 232) * 10; return QColor(g, g, g); }
    n -= 16;
    auto lv = [](int v) { return v == 0 ? 0 : 55 + v * 40; };
    return QColor(lv(n / 36), lv((n / 6) % 6), lv(n % 6));
}

const QColor kTimeStampColor(140, 140, 140);   // 타임스탬프는 회색
} // namespace

QString MainWindow::stripAnsi(const QString &s)
{
    static const QRegularExpression re(
        QStringLiteral("\\x1B(?:\\[[0-9;?]*[@-~]|[()][0-9A-Za-z]|[@-Z\\\\-_]?)"));
    QString out = s;
    out.remove(re);
    return out;
}

void MainWindow::applySgr(const QString &params)
{
    const QStringList p = params.isEmpty() ? QStringList{QStringLiteral("0")}
                                           : params.split(QLatin1Char(';'));
    for (int i = 0; i < p.size(); ++i) {
        const int v = p.at(i).isEmpty() ? 0 : p.at(i).toInt();
        if (v == 0) {
            m_ansiFg = QColor(); m_ansiBg = QColor(); m_ansiFgIdx = -1;
            m_ansiBold = m_ansiUnderline = m_ansiReverse = false;
        }
        else if (v == 1)  m_ansiBold = true;
        else if (v == 22) m_ansiBold = false;
        else if (v == 4)  m_ansiUnderline = true;
        else if (v == 24) m_ansiUnderline = false;
        else if (v == 7)  m_ansiReverse = true;
        else if (v == 27) m_ansiReverse = false;
        else if (v >= 30 && v <= 37)  { m_ansiFgIdx = v - 30; m_ansiFg = kAnsiColors[v - 30]; }
        else if (v == 39)             { m_ansiFgIdx = -1; m_ansiFg = QColor(); }
        else if (v >= 90 && v <= 97)  { m_ansiFgIdx = -1; m_ansiFg = kAnsiColors[v - 90 + 8]; }
        else if (v >= 40 && v <= 47)  m_ansiBg = kAnsiColors[v - 40];
        else if (v == 49)             m_ansiBg = QColor();
        else if (v >= 100 && v <= 107) m_ansiBg = kAnsiColors[v - 100 + 8];
        else if ((v == 38 || v == 48) && i + 1 < p.size()) {
            QColor col;
            const int mode = p.at(i + 1).toInt();
            if (mode == 5 && i + 2 < p.size()) { col = ansi256(p.at(i + 2).toInt()); i += 2; }
            else if (mode == 2 && i + 4 < p.size()) {
                col = QColor(p.at(i + 2).toInt(), p.at(i + 3).toInt(), p.at(i + 4).toInt());
                i += 4;
            } else { i += 1; }
            if (v == 38) { m_ansiFgIdx = -1; m_ansiFg = col; }
            else         { m_ansiBg = col; }
        }
    }
}

void MainWindow::appendDebugAnsi(const QString &prefix, const QString &line)
{
    QTextDocument *doc = m_txtDebug->document();
    QTextCursor cur(doc);
    cur.movePosition(QTextCursor::End);
    if (!doc->isEmpty() || doc->blockCount() > 1 || !doc->firstBlock().text().isEmpty())
        cur.insertBlock(QTextBlockFormat(), QTextCharFormat());

    // 타임스탬프 등 앞부분은 기본(회색) 서식
    if (!prefix.isEmpty()) {
        QTextCharFormat pf;
        pf.setForeground(kTimeStampColor);
        cur.insertText(prefix, pf);
    }

    auto currentFormat = [this]() {
        QTextCharFormat f;
        QColor fg = m_ansiFg;
        if (m_ansiBold && m_ansiFgIdx >= 0) fg = kAnsiColors[m_ansiFgIdx + 8];   // bold -> 밝은색
        QColor bg = m_ansiBg;
        if (m_ansiReverse) {
            QColor defFg = m_txtDebug->palette().color(QPalette::Text);
            QColor defBg = m_txtDebug->palette().color(QPalette::Base);
            QColor nf = bg.isValid() ? bg : defBg;
            QColor nb = fg.isValid() ? fg : defFg;
            fg = nf; bg = nb;
        }
        if (fg.isValid()) f.setForeground(fg);
        if (bg.isValid()) f.setBackground(bg);
        if (m_ansiBold)      f.setFontWeight(QFont::Bold);
        if (m_ansiUnderline) f.setFontUnderline(true);
        return f;
    };

    static const QRegularExpression reEsc(
        QStringLiteral("\\x1B(?:\\[([0-9;?]*)([@-~])|[()][0-9A-Za-z]|[@-Z\\\\-_]?)"));
    int pos = 0;
    QRegularExpressionMatchIterator it = reEsc.globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (m.capturedStart() > pos)
            cur.insertText(line.mid(pos, m.capturedStart() - pos), currentFormat());
        if (m.captured(2) == QLatin1String("m"))
            applySgr(m.captured(1));
        // 그 외 제어 시퀀스(커서 이동, 화면 지우기 등)는 무시
        pos = m.capturedEnd();
    }
    if (pos < line.size())
        cur.insertText(line.mid(pos), currentFormat());

    m_txtDebug->verticalScrollBar()->setValue(m_txtDebug->verticalScrollBar()->maximum());

    if (m_txtFile.isOpen()) {
        m_txtStream << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
                    << QStringLiteral("\t") << prefix << stripAnsi(line) << QStringLiteral("\n");
        m_txtStream.flush();
    }
}

void MainWindow::appendDebugLog(const QString &text)
{
    // 프로그램 내부 메시지는 ANSI 상태에 영향을 주지 않고 기본색으로 출력
    QTextDocument *doc = m_txtDebug->document();
    QTextCursor cur(doc);
    cur.movePosition(QTextCursor::End);
    if (!doc->isEmpty() || doc->blockCount() > 1 || !doc->firstBlock().text().isEmpty())
        cur.insertBlock(QTextBlockFormat(), QTextCharFormat());
    cur.insertText(stripAnsi(text), QTextCharFormat());
    m_txtDebug->verticalScrollBar()->setValue(m_txtDebug->verticalScrollBar()->maximum());

    if (m_txtFile.isOpen()) {
        m_txtStream << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
                    << QStringLiteral("\t") << stripAnsi(text) << QStringLiteral("\n");
        m_txtStream.flush();
    }
}

void MainWindow::appendMeterLog(const QString &text)
{
    m_txtMeter->appendPlainText(text);
    m_txtMeter->verticalScrollBar()->setValue(m_txtMeter->verticalScrollBar()->maximum());
}

// ================================================================= 수신 처리

void MainWindow::handleIncoming(QByteArray &buffer, const QByteArray &chunk, bool isDebug)
{
    buffer.append(chunk);

    // \r\n, \n, \r 모두 줄바꿈으로 처리
    for (;;) {
        int idx = -1;
        int skip = 1;
        for (int i = 0; i < buffer.size(); ++i) {
            const char c = buffer.at(i);
            if (c == '\n') { idx = i; skip = 1; break; }
            if (c == '\r') {
                idx = i;
                skip = (i + 1 < buffer.size() && buffer.at(i + 1) == '\n') ? 2 : 1;
                break;
            }
        }
        if (idx < 0) break;

        const QString line = QString::fromLatin1(buffer.left(idx)).trimmed();
        buffer.remove(0, idx + skip);

        if (line.isEmpty())
            continue;

        if (isDebug) processDebugLine(line);
        else         processMeterLine(line);
    }

    // 개행 없이 프롬프트("=>")만 오는 경우 처리
    if (!isDebug && buffer.contains(kPrompt)) {
        const QString rest = QString::fromLatin1(buffer).trimmed();
        buffer.clear();
        if (!rest.isEmpty())
            processMeterLine(rest);
    }

    if (buffer.size() > 8192)
        buffer.remove(0, buffer.size() - 8192);
}

void MainWindow::onDebugReadyRead()
{
    handleIncoming(m_debugBuf, m_debugPort->readAll(), true);
}

void MainWindow::onMeterReadyRead()
{
    handleIncoming(m_meterBuf, m_meterPort->readAll(), false);
}

double MainWindow::parseNumberAfterKeyword(const QString &line, const QString &keyword, bool *ok)
{
    if (ok) *ok = false;
    if (keyword.isEmpty()) return 0.0;

    const QString pattern = QRegularExpression::escape(keyword)
            + QStringLiteral("\\s*[:=]?\\s*(")
            + QString::fromLatin1(kNumberPattern)
            + QStringLiteral(")");

    QRegularExpression re(pattern, QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(line);
    if (!m.hasMatch()) return 0.0;

    bool conv = false;
    const double v = m.captured(1).toDouble(&conv);
    if (ok) *ok = conv;
    return conv ? v : 0.0;
}

void MainWindow::processDebugLine(const QString &line)
{
    appendDebugAnsi(QStringLiteral("[%1] ").arg(timeStamp()), line);

    if (m_parseKeyword.isEmpty())
        return;

    bool ok = false;
    const double v = parseNumberAfterKeyword(stripAnsi(line), m_parseKeyword, &ok);
    if (!ok)
        return;

    m_hasDebugValue  = true;
    m_lastDebugValue = v;                       // 파싱한 원래 값 (uA)

    // 그래프에는 Meter 값과 같은 기준 단위로 환산해서 그린다.
    if (m_debugGraphOn && m_running && m_elapsed.isValid())
        m_graph->addDebugPoint(m_elapsed.elapsed() / 1000.0, v * kDebugUnitScale);
}

void MainWindow::processMeterLine(const QString &line)
{
    appendMeterLog(QStringLiteral("[%1] RX < %2").arg(timeStamp(), line));

    if (line.contains(QString::fromLatin1(kPrompt)))
        m_promptSeen = true;

    // ---- *IDN? 응답 확인 ------------------------------------------------
    if (m_meterState == MeterState::WaitIdn) {
        if (line.contains(QStringLiteral("FLUKE"), Qt::CaseInsensitive)) {
            m_idnMatched = true;
            if (squeeze(line) == QString::fromLatin1(kExpectedIdn)) {
                appendMeterLog(QStringLiteral("    -> IDN 확인 : 정상 (FLUKE, 45, 7940026, 1.7 D2.0)"));
            } else {
                appendMeterLog(QStringLiteral("    -> IDN 확인 : FLUKE 응답이나 기대값과 다름"));
            }
        }

        if (m_idnMatched && m_promptSeen) {
            m_idnTimer->stop();
            m_meterState = MeterState::Polling;

            const int interval = m_spPeriod->value() * 100;
            m_pollTimer->start(interval);
            appendMeterLog(QStringLiteral("    -> 프롬프트(=>) 확인, %1 ms 주기로 VAL1? 요청을 시작합니다.")
                           .arg(interval));
            onPollTimeout();    // 첫 요청 즉시 실행
        }
        return;
    }

    // ---- 측정 값 파싱 ---------------------------------------------------
    if (m_meterState != MeterState::Polling)
        return;

    const QRegularExpressionMatch m = m_reNumber.match(line);
    if (!m.hasMatch())
        return;                             // 에코("VAL1?")나 프롬프트("=>")는 무시

    bool conv = false;
    const double value = line.toDouble(&conv);
    if (!conv)
        return;

    m_waitingValue = false;

    // 화면 표시용 (소수점 3자리) / 파일 저장용 (소수점 4자리)
    QString dispText, dispUnit;
    UnitUtil::splitValue(value, kDisplayDecimals, dispText, dispUnit);

    QString csvText, csvUnit;
    UnitUtil::splitValue(value, kCsvDecimals, csvText, csvUnit);

    m_lblMeterValue->setText(QStringLiteral("현재 Meter 값 : ") + dispText
                             + (dispUnit.isEmpty() ? QString() : QLatin1Char(' ') + dispUnit));

    const double t = m_elapsed.isValid() ? (m_elapsed.elapsed() / 1000.0) : 0.0;
    m_graph->addMeterPoint(t, value);

    writeCsvRow(QDateTime::currentDateTime(), t, value, csvText, csvUnit);
}

// ================================================================= Debug 명령 전송

void MainWindow::onDebugCommandEntered()
{
    const QString cmd = m_edDebugCmd->text();
    if (cmd.isEmpty())
        return;

    if (!m_debugPort->isOpen()) {
        QMessageBox::warning(this, QStringLiteral("Debug"),
                             QStringLiteral("Debug 시리얼이 연결되어 있지 않습니다."));
        return;
    }

    QByteArray data = cmd.toLatin1();
    data.append("\r\n");
    m_debugPort->write(data);

    appendDebugLog(QStringLiteral("[%1] TX > %2").arg(timeStamp(), cmd));
    m_edDebugCmd->clear();
}

// ================================================================= 파일

bool MainWindow::openLogFiles()
{
    QDir dir;
    if (!dir.mkpath(m_logDir)) {
        QMessageBox::critical(this, QStringLiteral("파일"),
                              QStringLiteral("로그 폴더를 만들 수 없습니다.\n%1")
                              .arg(QDir::toNativeSeparators(m_logDir)));
        return false;
    }

    const QString stamp = nowStamp();

    // Debug TXT
    m_txtFile.setFileName(m_logDir + QStringLiteral("/Debug_") + stamp + QStringLiteral(".txt"));
    if (!m_txtFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, QStringLiteral("파일"),
                              QStringLiteral("Debug 로그 파일을 만들 수 없습니다.\n%1")
                              .arg(QDir::toNativeSeparators(m_txtFile.fileName())));
        return false;
    }
    m_txtStream.setDevice(&m_txtFile);
    m_txtStream.setEncoding(QStringConverter::Utf8);
    m_txtStream << QStringLiteral("# MeterMonitor Debug Log  ")
                << QDateTime::currentDateTime().toString(Qt::ISODate) << QStringLiteral("\n");

    // Meter CSV (Excel 에서 바로 열림)
    m_csvFile.setFileName(m_logDir + QStringLiteral("/Meter_") + stamp + QStringLiteral(".csv"));
    if (!m_csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, QStringLiteral("파일"),
                              QStringLiteral("Meter 데이터 파일을 만들 수 없습니다.\n%1")
                              .arg(QDir::toNativeSeparators(m_csvFile.fileName())));
        m_txtFile.close();
        return false;
    }
    m_csvStream.setDevice(&m_csvFile);
    m_csvStream.setEncoding(QStringConverter::Utf8);
    m_csvStream.setGenerateByteOrderMark(true);      // Excel 한글 깨짐 방지
    m_csvStream << QStringLiteral("시간,경과시간(s),"
                                  "Meter값(기본단위),Meter값(변환),단위,"
                                  "Debug값(%1),Debug값(기본단위),Debug값(변환),Debug단위\n")
                   .arg(QString::fromLatin1(kDebugUnitName));
    m_csvStream.flush();

    statusBar()->showMessage(QStringLiteral("저장 : %1 , %2")
                             .arg(QDir::toNativeSeparators(m_txtFile.fileName()),
                                  QDir::toNativeSeparators(m_csvFile.fileName())));
    return true;
}

void MainWindow::closeLogFiles()
{
    if (m_txtFile.isOpen()) { m_txtStream.flush(); m_txtFile.close(); }
    if (m_csvFile.isOpen()) { m_csvStream.flush(); m_csvFile.close(); }
}

void MainWindow::writeCsvRow(const QDateTime &ts, double elapsed, double meterValue,
                             const QString &meterConverted, const QString &meterUnit)
{
    if (!m_csvFile.isOpen())
        return;

    // Debug 값 : 원본(uA) / 기준 단위 환산값(A) / 단위 변환값 + 단위
    QString dbgRaw, dbgBase, dbgConv, dbgUnit;
    if (m_debugGraphOn && m_hasDebugValue) {         // 그래프 표시가 ON 일 때만 함께 저장
        const double base = m_lastDebugValue * kDebugUnitScale;
        dbgRaw  = csvNumber(m_lastDebugValue);
        dbgBase = csvNumber(base);
        UnitUtil::splitValue(base, kCsvDecimals, dbgConv, dbgUnit);
        dbgUnit += QString::fromLatin1(kDebugBaseUnit);
    }

    m_csvStream << ts.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")) << QLatin1Char(',')
                << QString::number(elapsed, 'f', 3)                       << QLatin1Char(',')
                << csvNumber(meterValue)                                  << QLatin1Char(',')
                << meterConverted                                         << QLatin1Char(',')
                << meterUnit                                              << QLatin1Char(',')
                << dbgRaw                                                 << QLatin1Char(',')
                << dbgBase                                                << QLatin1Char(',')
                << dbgConv                                                << QLatin1Char(',')
                << dbgUnit << QLatin1Char('\n');
    m_csvStream.flush();
}

// ================================================================= 시작 / 정지

void MainWindow::setStartButtonState(int state)
{
    switch (state) {
    case 1:     // 진행중 (녹색)
        m_btnStart->setText(QStringLiteral("진행중"));
        m_btnStart->setStyleSheet(QString::fromLatin1(kStyleGreen));
        break;
    case 2:     // 정지 후 다시 시작 (노랑)
        m_btnStart->setText(QStringLiteral("시작"));
        m_btnStart->setStyleSheet(QString::fromLatin1(kStyleYellow));
        break;
    default:    // 초기
        m_btnStart->setText(QStringLiteral("시작"));
        m_btnStart->setStyleSheet(QString::fromLatin1(kStyleIdle));
        break;
    }
}

void MainWindow::onStartClicked()
{
    if (m_running) stopMeasure();
    else           startMeasure();
}

void MainWindow::startMeasure()
{
    if (!m_meterPort->isOpen()) {
        QMessageBox::warning(this, QStringLiteral("시작"),
                             QStringLiteral("Meter 시리얼이 연결되어 있지 않습니다.\n"
                                            "먼저 [Meter 시리얼 연결] 을 실행하세요."));
        return;
    }

    if (!openLogFiles())
        return;

    m_running      = true;
    m_idnMatched   = false;
    m_promptSeen   = false;
    m_waitingValue = false;
    m_meterState   = MeterState::WaitIdn;

    m_graph->clearAll();
    m_graph->setWindowSeconds(qMax(20.0, m_spPeriod->value() * 100 * 60 / 1000.0));
    m_elapsed.restart();

    m_meterBuf.clear();
    m_meterPort->clear();

    setStartButtonState(1);
    m_spPeriod->setEnabled(false);

    appendDebugLog(QStringLiteral("[%1] ===== 측정 시작 =====").arg(timeStamp()));

    // *IDN?<CR> 전송  ( <CR> = 엔터키 = CR+LF )
    m_meterPort->write(QByteArray("*IDN?") + kCmdTerminator);
    appendMeterLog(QStringLiteral("[%1] TX > *IDN?<CR>").arg(timeStamp()));

    m_idnTimer->start();
}

void MainWindow::stopMeasure()
{
    m_pollTimer->stop();
    m_idnTimer->stop();

    m_running    = false;
    m_meterState = MeterState::Idle;

    appendDebugLog(QStringLiteral("[%1] ===== 측정 정지 =====").arg(timeStamp()));
    appendMeterLog(QStringLiteral("[%1] 측정을 정지했습니다.").arg(timeStamp()));

    closeLogFiles();

    m_spPeriod->setEnabled(true);
    setStartButtonState(2);     // "시작" / 노란색
}

void MainWindow::onIdnTimeout()
{
    appendMeterLog(QStringLiteral("[%1] *IDN? 응답 시간 초과").arg(timeStamp()));
    QMessageBox::warning(this, QStringLiteral("Meter"),
                         QStringLiteral("*IDN? 에 대한 응답을 확인하지 못했습니다.\n"
                                        "시리얼 설정과 장비 연결 상태를 확인하세요."));
    stopMeasure();
}

void MainWindow::onPollTimeout()
{
    if (!m_running || !m_meterPort->isOpen())
        return;

    if (m_waitingValue)
        appendMeterLog(QStringLiteral("[%1] 이전 요청의 응답이 아직 없습니다.").arg(timeStamp()));

    m_meterPort->write(QByteArray("VAL1?") + kCmdTerminator);
    m_waitingValue = true;
    appendMeterLog(QStringLiteral("[%1] TX > VAL1?<CR>").arg(timeStamp()));
}
