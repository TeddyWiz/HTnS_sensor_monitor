#ifndef SERIALSETTINGSDIALOG_H
#define SERIALSETTINGSDIALOG_H

#include <QDialog>
#include <QSerialPort>
#include <QString>

class QListWidget;
class QComboBox;
class QPushButton;

//
// 시리얼 포트 설정 값 묶음
//
struct SerialSettings
{
    QString                  portName;
    qint32                   baudRate    = 115200;
    QSerialPort::DataBits    dataBits    = QSerialPort::Data8;
    QSerialPort::Parity      parity      = QSerialPort::NoParity;
    QSerialPort::StopBits    stopBits    = QSerialPort::OneStop;
    QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;

    QString summary() const;
};

//
// "시리얼 포트 설정" 창
//   - 좌측 : 연결 가능한 시리얼 포트 리스트
//   - 우측 : 시리얼 포트 연결 버튼 + Baudrate / data / parity / stop bit / Flow control
//
class SerialSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SerialSettingsDialog(const QString &title,
                                  const SerialSettings &current,
                                  QWidget *parent = nullptr);

    SerialSettings settings() const { return m_settings; }

private slots:
    void refreshPorts();
    void onConnectClicked();

private:
    void buildUi();
    void applyToUi(const SerialSettings &s);

    QListWidget *m_portList     = nullptr;
    QComboBox   *m_cbBaud       = nullptr;
    QComboBox   *m_cbData       = nullptr;
    QComboBox   *m_cbParity     = nullptr;
    QComboBox   *m_cbStop       = nullptr;
    QComboBox   *m_cbFlow       = nullptr;
    QPushButton *m_btnConnect   = nullptr;
    QPushButton *m_btnRefresh   = nullptr;

    SerialSettings m_settings;
};

#endif // SERIALSETTINGSDIALOG_H
