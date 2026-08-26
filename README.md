# MeterMonitor

Fluke 45 멀티미터 + Debug 시리얼 모니터링 프로그램 (Qt 6.6.2 / MSVC2019 64bit)

## 빌드

### Qt Creator
1. `MeterMonitor.pro` 열기
2. Kit : **Desktop Qt 6.6.2 MSVC2019 64bit** 선택
3. 빌드 후 실행

### 명령행
```
"x64 Native Tools Command Prompt for VS 2019" 실행
C:\Qt\6.6.2\msvc2019_64\bin\qmake.exe MeterMonitor.pro
nmake
```

> 사용 모듈 : `core gui widgets serialport` (Qt SerialPort 컴포넌트 필요)
> 소스는 UTF-8 이며 `.pro` 에서 MSVC 용 `/utf-8` 옵션을 지정하므로 한글이 깨지지 않습니다.

## 파일 구성

| 파일 | 설명 |
|---|---|
| `MeterMonitor.pro` | 프로젝트 파일 |
| `main.cpp` | 진입점 |
| `mainwindow.h/.cpp` | 메인 화면, 시리얼 제어, 계측 로직, 파일 저장 |
| `serialsettingsdialog.h/.cpp` | 시리얼 포트 설정 창 |
| `parsingsettingsdialog.h/.cpp` | 디버그 파싱 설정 창 |
| `graphwidget.h/.cpp` | QPainter 기반 실시간 그래프 |
| `unitutil.h` | 단위(SI 접두어) 변환 |

## 사용 순서

1. **Debug 시리얼 연결** / **Meter 시리얼 연결** 버튼 → 포트 설정 창에서
   포트 선택 + Baudrate / data / parity / stop bit / Flow control 설정 →
   **시리얼 포트 연결** 버튼
   - 연결되면 옆의 표시창이 **녹색 + 포트명**, 버튼을 한 번 더 누르면 해제되고 **빨간색 + 연결 해제**
2. **디버그 parsing 설정** → 파싱 시작 단어 입력 (예: `temp`)
   - Debug 수신 문자열 `temp = 25.3` 에서 `25.3` 을 값으로 사용
3. **Debug 그래프 표시 ON/OFF** → ON(녹색) 이면 파싱 값을 그래프/CSV 에 함께 기록
4. **Meter 데이터 요청시간** → 100ms 단위 (예: `10` = 1초 주기)
5. **시작** 버튼
   - `*IDN?<CR>` 전송 → `FLUKE, 45, 7940026, 1.7 D2.0` 및 프롬프트 `=>` 확인
   - `<CR>` 은 윈도우 엔터키와 동일하게 **CR+LF(0x0D 0x0A)** 로 전송합니다.
     (변경이 필요하면 `mainwindow.cpp` 상단의 `kCmdTerminator` 상수만 수정)
   - 이후 설정 주기마다 `VAL1?<CR>` 전송, 수신 값을 파싱해 현재 값/그래프/CSV 에 반영
   - 버튼이 **진행중(녹색)** 으로 바뀌고, 다시 누르면 **시작(노란색)** 으로 정지
6. **Debug 명령 입력창** → 내용 입력 후 Enter 시 Debug 시리얼로 전송 (`CR+LF`)

## 저장 파일

실행 파일과 같은 폴더의 `log\` 아래에 **시작 버튼을 누른 시각** 이름으로 생성됩니다.

- `Debug_yyyyMMdd_HHmmss.txt` : Debug 수신/송신 내용
- `Meter_yyyyMMdd_HHmmss.csv` : 시간, 경과시간(s), Meter 값(기본 단위), Meter 값(변환), 단위, Debug 값
  - `Meter값(변환)` 열에는 숫자만(`53.0000`), `단위` 열에 접두어(`u`, `m` 등)가 따로 들어갑니다.
  - UTF-8 BOM 으로 저장되어 Excel 에서 바로 열립니다.
  - Debug 그래프 표시가 ON 일 때만 Debug 값 열이 채워집니다.

## 값 단위 변환

Fluke 45 는 `+41.41E-3` 형식으로 값을 보냅니다.
`E-3` → `m`, `E-6` → `u` 처럼 SI 접두어로 환산해 표시합니다. (예: `41.4100 m`)
CSV 에는 변환 전 기본 단위 값과 변환된 표시 문자열이 모두 저장됩니다.
