#ifndef PARSINGSETTINGSDIALOG_H
#define PARSINGSETTINGSDIALOG_H

#include <QDialog>

class QLineEdit;

//
// "디버그 파싱 설정" 창
//   파싱 시작 단어를 입력하면, Debug 시리얼로 수신된 문자열에서
//   그 단어 뒤에 오는 숫자를 값으로 가져온다.
//
class ParsingSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ParsingSettingsDialog(const QString &keyword, QWidget *parent = nullptr);

    QString keyword() const;

private:
    QLineEdit *m_edKeyword = nullptr;
};

#endif // PARSINGSETTINGSDIALOG_H
