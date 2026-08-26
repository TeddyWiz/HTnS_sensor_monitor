#include "parsingsettingsdialog.h"

#include <QLineEdit>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

ParsingSettingsDialog::ParsingSettingsDialog(const QString &keyword, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("디버그 파싱 설정"));
    setMinimumWidth(360);

    QGroupBox *box = new QGroupBox(QStringLiteral("디버그 파싱 설정"), this);

    QLabel *lbl = new QLabel(QStringLiteral("파싱 시작 단어"), box);

    m_edKeyword = new QLineEdit(keyword, box);
    m_edKeyword->setPlaceholderText(QStringLiteral("word"));
    m_edKeyword->setMinimumWidth(160);

    QHBoxLayout *row = new QHBoxLayout;
    row->addWidget(lbl);
    row->addSpacing(12);
    row->addWidget(m_edKeyword, 1);

    QLabel *hint = new QLabel(
        QStringLiteral("입력한 단어 뒤에 오는 숫자를 값으로 사용합니다.\n"
                       "예) 파싱 시작 단어가 \"temp\" 이고 수신 문자열이\n"
                       "    \"temp = 25.3 C\" 이면 25.3 을 가져옵니다."), box);
    hint->setStyleSheet(QStringLiteral("color:#555;"));

    QVBoxLayout *boxLay = new QVBoxLayout(box);
    boxLay->addLayout(row);
    boxLay->addSpacing(6);
    boxLay->addWidget(hint);

    QPushButton *btnOk     = new QPushButton(QStringLiteral("확인"), this);
    QPushButton *btnCancel = new QPushButton(QStringLiteral("취소"), this);
    btnOk->setDefault(true);

    QHBoxLayout *bottom = new QHBoxLayout;
    bottom->addStretch(1);
    bottom->addWidget(btnOk);
    bottom->addWidget(btnCancel);

    QVBoxLayout *main = new QVBoxLayout(this);
    main->addWidget(box);
    main->addLayout(bottom);

    connect(btnOk,     &QPushButton::clicked, this, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

QString ParsingSettingsDialog::keyword() const
{
    return m_edKeyword->text().trimmed();
}
