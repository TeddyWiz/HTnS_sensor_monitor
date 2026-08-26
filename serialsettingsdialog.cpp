#include "serialsettingsdialog.h"

#include <QSerialPortInfo>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QIntValidator>

// ---------------------------------------------------------------- SerialSettings

QString SerialSettings::summary() const
{
    QString p;
    switch (parity) {
    case QSerialPort::NoParity:    p = QStringLiteral("N"); break;
    case QSerialPort::EvenParity:  p = QStringLiteral("E"); break;
    case QSerialPort::OddParity:   p = QStringLiteral("O"); break;
    case QSerialPort::MarkParity:  p = QStringLiteral("M"); break;
    case QSerialPort::SpaceParity: p = QStringLiteral("S"); break;
    default:                       p = QStringLiteral("N"); break;
    }
    const int stop = (stopBits == QSerialPort::TwoStop) ? 2 : 1;
    return QStringLiteral("%1 %2-%3%4%5")
            .arg(portName)
            .arg(baudRate)
            .arg(static_cast<int>(dataBits))
            .arg(p)
            .arg(stop);
}

// ---------------------------------------------------------------- ctor

SerialSettingsDialog::SerialSettingsDialog(const QString &title,
                                           const SerialSettings &current,
                                           QWidget *parent)
    : QDialog(parent)
    , m_settings(current)
{
    setWindowTitle(title.isEmpty() ? QStringLiteral("시리얼 포트 설정") : title);
    setMinimumSize(560, 340);

    buildUi();
    refreshPorts();
    applyToUi(current);
}

// ---------------------------------------------------------------- UI

void SerialSettingsDialog::buildUi()
{
    QGroupBox *box = new QGroupBox(QStringLiteral("시리얼 포트 설정"), this);

    // --- 좌측 : 포트 리스트 ---------------------------------------------------
    m_portList = new QListWidget(box);
    m_portList->setMinimumWidth(240);

    QLabel *lblList = new QLabel(QStringLiteral("연결 가능 시리얼 포트 리스트"), box);
    lblList->setAlignment(Qt::AlignCenter);

    m_btnRefresh = new QPushButton(QStringLiteral("포트 목록 새로 고침"), box);

    QVBoxLayout *leftLay = new QVBoxLayout;
    leftLay->addWidget(lblList);
    leftLay->addWidget(m_portList, 1);
    leftLay->addWidget(m_btnRefresh);

    // --- 우측 : 연결 버튼 + 설정 ----------------------------------------------
    m_btnConnect = new QPushButton(QStringLiteral("시리얼 포트 연결"), box);
    m_btnConnect->setMinimumHeight(34);
    m_btnConnect->setStyleSheet(QStringLiteral("font-weight:bold;"));

    // Baudrate (직접 입력도 가능하도록 editable)
    m_cbBaud = new QComboBox(box);
    m_cbBaud->setEditable(true);
    m_cbBaud->setValidator(new QIntValidator(50, 4000000, m_cbBaud));
    const QList<qint32> bauds = { 110, 300, 600, 1200, 2400, 4800, 9600, 14400,
                                  19200, 38400, 57600, 115200, 230400, 460800, 921600 };
    for (qint32 b : bauds)
        m_cbBaud->addItem(QString::number(b), b);

    // data bits : 7bit / 8bit
    m_cbData = new QComboBox(box);
    m_cbData->addItem(QStringLiteral("7bit"), static_cast<int>(QSerialPort::Data7));
    m_cbData->addItem(QStringLiteral("8bit"), static_cast<int>(QSerialPort::Data8));

    // parity : none / odd / even / mark / space
    m_cbParity = new QComboBox(box);
    m_cbParity->addItem(QStringLiteral("none"),  static_cast<int>(QSerialPort::NoParity));
    m_cbParity->addItem(QStringLiteral("odd"),   static_cast<int>(QSerialPort::OddParity));
    m_cbParity->addItem(QStringLiteral("even"),  static_cast<int>(QSerialPort::EvenParity));
    m_cbParity->addItem(QStringLiteral("mark"),  static_cast<int>(QSerialPort::MarkParity));
    m_cbParity->addItem(QStringLiteral("space"), static_cast<int>(QSerialPort::SpaceParity));

    // stop bit : 1bit / 2bit
    m_cbStop = new QComboBox(box);
    m_cbStop->addItem(QStringLiteral("1 bit"), static_cast<int>(QSerialPort::OneStop));
    m_cbStop->addItem(QStringLiteral("2 bit"), static_cast<int>(QSerialPort::TwoStop));

    // flow control : none / Xon-Xoff / RTS-CTS / DSR-DTR(SW 처리)
    m_cbFlow = new QComboBox(box);
    m_cbFlow->addItem(QStringLiteral("none"),     static_cast<int>(QSerialPort::NoFlowControl));
    m_cbFlow->addItem(QStringLiteral("Xon/Xoff"), static_cast<int>(QSerialPort::SoftwareControl));
    m_cbFlow->addItem(QStringLiteral("RTS/CTS"),  static_cast<int>(QSerialPort::HardwareControl));
    m_cbFlow->addItem(QStringLiteral("DSR/DTR"),  static_cast<int>(QSerialPort::HardwareControl));

    QFormLayout *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->addRow(QStringLiteral("Baudrate"),     m_cbBaud);
    form->addRow(QStringLiteral("data"),         m_cbData);
    form->addRow(QStringLiteral("parity"),       m_cbParity);
    form->addRow(QStringLiteral("stop bit"),     m_cbStop);
    form->addRow(QStringLiteral("Flow control"), m_cbFlow);

    QVBoxLayout *rightLay = new QVBoxLayout;
    rightLay->addWidget(m_btnConnect, 0, Qt::AlignRight);
    rightLay->addSpacing(8);
    rightLay->addLayout(form);
    rightLay->addStretch(1);

    QHBoxLayout *boxLay = new QHBoxLayout(box);
    boxLay->addLayout(leftLay, 1);
    boxLay->addSpacing(12);
    boxLay->addLayout(rightLay, 1);

    QPushButton *btnCancel = new QPushButton(QStringLiteral("취소"), this);

    QHBoxLayout *bottom = new QHBoxLayout;
    bottom->addStretch(1);
    bottom->addWidget(btnCancel);

    QVBoxLayout *main = new QVBoxLayout(this);
    main->addWidget(box, 1);
    main->addLayout(bottom);

    connect(m_btnRefresh, &QPushButton::clicked, this, &SerialSettingsDialog::refreshPorts);
    connect(m_btnConnect, &QPushButton::clicked, this, &SerialSettingsDialog::onConnectClicked);
    connect(btnCancel,    &QPushButton::clicked, this, &QDialog::reject);
    connect(m_portList,   &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem *) { onConnectClicked(); });
}

void SerialSettingsDialog::applyToUi(const SerialSettings &s)
{
    // baudrate
    const int bi = m_cbBaud->findData(s.baudRate);
    if (bi >= 0) m_cbBaud->setCurrentIndex(bi);
    else         m_cbBaud->setCurrentText(QString::number(s.baudRate));

    const int di = m_cbData->findData(static_cast<int>(s.dataBits));
    if (di >= 0) m_cbData->setCurrentIndex(di);

    const int pi = m_cbParity->findData(static_cast<int>(s.parity));
    if (pi >= 0) m_cbParity->setCurrentIndex(pi);

    const int si = m_cbStop->findData(static_cast<int>(s.stopBits));
    if (si >= 0) m_cbStop->setCurrentIndex(si);

    const int fi = m_cbFlow->findData(static_cast<int>(s.flowControl));
    if (fi >= 0) m_cbFlow->setCurrentIndex(fi);

    // 이전에 쓰던 포트가 목록에 있으면 선택
    if (!s.portName.isEmpty()) {
        for (int i = 0; i < m_portList->count(); ++i) {
            if (m_portList->item(i)->data(Qt::UserRole).toString() == s.portName) {
                m_portList->setCurrentRow(i);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------- slots

void SerialSettingsDialog::refreshPorts()
{
    const QString keep = m_portList->currentItem()
            ? m_portList->currentItem()->data(Qt::UserRole).toString()
            : m_settings.portName;

    m_portList->clear();

    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        QString text = info.portName();
        if (!info.description().isEmpty())
            text += QStringLiteral("  (") + info.description() + QLatin1Char(')');

        QListWidgetItem *item = new QListWidgetItem(text, m_portList);
        item->setData(Qt::UserRole, info.portName());
    }

    if (m_portList->count() == 0) {
        QListWidgetItem *item = new QListWidgetItem(QStringLiteral("(사용 가능한 포트 없음)"), m_portList);
        item->setFlags(Qt::NoItemFlags);
        return;
    }

    for (int i = 0; i < m_portList->count(); ++i) {
        if (m_portList->item(i)->data(Qt::UserRole).toString() == keep) {
            m_portList->setCurrentRow(i);
            return;
        }
    }
    m_portList->setCurrentRow(0);
}

void SerialSettingsDialog::onConnectClicked()
{
    QListWidgetItem *item = m_portList->currentItem();
    const QString port = item ? item->data(Qt::UserRole).toString() : QString();

    if (port.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("시리얼 포트 설정"),
                             QStringLiteral("연결할 시리얼 포트를 리스트에서 선택하세요."));
        return;
    }

    bool ok = false;
    const qint32 baud = m_cbBaud->currentText().trimmed().toInt(&ok);
    if (!ok || baud <= 0) {
        QMessageBox::warning(this, QStringLiteral("시리얼 포트 설정"),
                             QStringLiteral("Baudrate 값이 올바르지 않습니다."));
        return;
    }

    m_settings.portName    = port;
    m_settings.baudRate    = baud;
    m_settings.dataBits    = static_cast<QSerialPort::DataBits>(m_cbData->currentData().toInt());
    m_settings.parity      = static_cast<QSerialPort::Parity>(m_cbParity->currentData().toInt());
    m_settings.stopBits    = static_cast<QSerialPort::StopBits>(m_cbStop->currentData().toInt());
    m_settings.flowControl = static_cast<QSerialPort::FlowControl>(m_cbFlow->currentData().toInt());

    accept();   // 메인 창으로 돌아가서 실제 연결을 수행한다.
}
