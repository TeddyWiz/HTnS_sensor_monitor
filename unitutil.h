#ifndef UNITUTIL_H
#define UNITUTIL_H

#include <QString>
#include <QtMath>

//
// 단위 변환 유틸리티
//  Fluke 45 는 "+41.41E-3" 과 같이 지수 표기로 값을 보낸다.
//  E-3 => m(milli), E-6 => u(micro) ... 형태로 접두어를 붙여서 표시한다.
//
namespace UnitUtil {

inline QString siPrefix(double value, int &prefixIndex)
{
    static const double kFactor[] = { 1e-12, 1e-9, 1e-6, 1e-3, 1.0, 1e3, 1e6, 1e9 };
    static const char  *kName[]   = { "p",   "n",  "u",  "m",  "",   "k",  "M",  "G"  };

    const double a = qAbs(value);
    prefixIndex = 4; // 기본 = 배수 없음

    if (a > 0.0) {
        for (int i = 7; i >= 0; --i) {
            if (a >= kFactor[i]) { prefixIndex = i; break; }
        }
        if (a < kFactor[0]) prefixIndex = 0;
    }
    return QString::fromLatin1(kName[prefixIndex]);
}

inline double siFactor(int prefixIndex)
{
    static const double kFactor[] = { 1e-12, 1e-9, 1e-6, 1e-3, 1.0, 1e3, 1e6, 1e9 };
    if (prefixIndex < 0) prefixIndex = 0;
    if (prefixIndex > 7) prefixIndex = 7;
    return kFactor[prefixIndex];
}

// 값을 "숫자" 와 "단위 접두어" 로 분리해서 돌려준다.
//   예) 5.3e-05 -> numText = "53.0000", prefixText = "u"
inline void splitValue(double value, int decimals, QString &numText, QString &prefixText)
{
    int idx = 4;
    prefixText = siPrefix(value, idx);
    numText    = QString::number(value / siFactor(idx), 'f', decimals);
}

// 값(기본 단위)을 사람이 읽기 좋은 문자열로 변환한다. 예) 0.04141 -> "41.4100 m"
inline QString formatValue(double value, int decimals = 4, const QString &unit = QString())
{
    int idx = 4;
    const QString p = siPrefix(value, idx);
    const double scaled = value / siFactor(idx);

    QString s = QString::number(scaled, 'f', decimals);
    if (!p.isEmpty())    s += QLatin1Char(' ') + p;
    if (!unit.isEmpty()) s += (p.isEmpty() ? QStringLiteral(" ") : QString()) + unit;
    return s;
}

} // namespace UnitUtil

#endif // UNITUTIL_H
