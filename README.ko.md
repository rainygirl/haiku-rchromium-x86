# R Chromium (x86)

[English](README.md) | [日本語](README.ja.md) | [Italiano](README.it.md) | **한국어**

32비트 Haiku(i386)용 Chromium 기반 웹 브라우저입니다. Qt 없이 Haiku 자체
윈도우 시스템 위에서 동작합니다. JavaScript를 포함한 최신 사이트
(google.com, news.naver.com, news.google.co.kr)를 렌더링하고, 아이콘만으로
구성된 뒤로 / 앞으로 / 새로 고침 버튼과 주소 입력란을 갖춘 네이티브 툴바를
제공하며, 날짜별로 묶인 검색 가능한 북마크를 저장하고, 파란 Chromium
아이콘으로 데스크톱에 설치됩니다.

이 문서는 최종 사용자용 설치 안내입니다. 소스 빌드, 포팅 노트 등 개발자용
내용은 [`AGENTS.md`](AGENTS.md)에 있습니다.

![VAIO P의 R Chromium에서 렌더링된 한국어 위키백과](docs/screenshots/x86-wikipedia.png)

![날짜별로 묶인 북마크 창](docs/screenshots/x86-bookmarks.png)


## 요구 사항

- 32비트 x86 Haiku (Sony VAIO P에서 테스트: Intel Atom Z520, RAM 2 GB).
- Haiku 표준 글꼴 (`/boot/system/data/fonts` 아래의 `NotoSans*`와
  `NotoSansCJKjp-VF.otf`). 한글은 CJK 글꼴로 렌더링됩니다.
- 빌드된 R Chromium (`content_shell` 바이너리와 같은 위치의
  `content_shell.pak`, `icudtl.dat`, `locales/`). 이 저장소의 빌드 트리가
  있다면 이미 들어 있습니다. 없다면 [`AGENTS.md`](AGENTS.md)의 설명대로
  빌드하세요.

## pkgman으로 설치

빌드된 브라우저가 `pkgman.rainygirl.com` 패키지 저장소에 올라가 있어서, 빌드나
체크아웃 없이 설치할 수 있습니다. `x86_gcc2` 하이브리드(일반적인 32비트 설치,
VAIO P가 쓰는 것)에서는 x86 보조 툴체인으로 빌드된 `rchromium_x86` 패키지입니다:

```sh
pkgman add-repo https://pkgman.rainygirl.com/x86_gcc2
pkgman install rchromium_x86
```

약 200 MB이니 `/boot`에 그만큼 여유 공간이 있어야 합니다.
`/boot/system/apps/RChromium/`에 fontconfig 파일과 함께 설치되고,
**Deskbar -> Applications**에 **R Chromium**이, `rchromium` 명령이 추가됩니다.
제거는 `pkgman uninstall rchromium_x86`입니다.

`pkgman add-repo`가 `Operation not supported`로 실패하면 그 빌드의 pkgman
네트워크 킷이 TLS를 못 하는 것이니 HTTP 주소를 사용합니다:

```sh
yes | pkgman add-repo http://pkgman.rainygirl.com/x86_gcc2
pkgman install rchromium_x86
```

## 체크아웃에서 설치

Haiku 머신에서 이 저장소를 체크아웃한 뒤 원샷 설치 스크립트를 실행합니다:

```sh
sh install.sh
```

이것으로 끝입니다. Haiku에 없는 fontconfig 파일을 준비하고, 브라우저
바이너리를 검증(이 머신의 링커가 손상시킨 경우 복구)하고, R Chromium을
`/boot/home/RChromium/`에 복사하고, 파란 Chromium 아이콘이 달린
**R Chromium** 실행기를 데스크톱에 놓습니다.

빌드가 기본 위치가 아닌 다른 곳에 있다면 그 디렉터리를 넘겨 주세요:

```sh
sh install.sh /path/to/dir/with/content_shell
```

(소스에서 빌드하는 일은 별도의 훨씬 긴 작업입니다 -- [`AGENTS.md`](AGENTS.md)
참고. 설치 스크립트는 이미 빌드된 바이너리를 설치합니다.)

## 실행

데스크톱의 **R Chromium**을 더블클릭합니다. Google이 열립니다. 상단 입력란에
주소를 입력하고 Enter를 누르세요 -- `news.naver.com`처럼 호스트만 입력하면
`https://news.naver.com/`이 됩니다.

셸에서 실행하려면:

```sh
"/boot/home/Desktop/R Chromium" https://news.naver.com/
```

## 사용법

- **뒤로 / 앞으로 / 새로 고침**은 왼쪽의 아이콘 버튼 세 개입니다. 페이지를
  불러오는 동안 새로 고침 버튼은 중지 버튼으로 바뀝니다.
- **북마크**: 별(★)은 현재 페이지를 북마크하고, 목록(≡)은 북마크 창을
  엽니다. 북마크는 날짜별(오늘, 어제, 그 다음은 날짜)로 묶이며, 검색란에
  입력하는 즉시 제목과 URL로 필터링됩니다. 항목을 더블클릭하면 열립니다.
  같은 페이지를 다시 북마크하면 중복되지 않고 오늘로 옮겨집니다. 북마크는
  일반 텍스트 파일 `~/config/settings/RChromium/bookmarks`에 북마크당 한 줄
  `<unix 초> <url> <제목>` 형식(탭 구분)으로 저장되므로 재설치 후에도
  남고, 직접 편집하거나 백업할 수 있습니다.
- **창**: 창 모서리를 끌어 크기를 바꾸면 페이지가 새 크기에 맞게 다시
  배치됩니다. 새 창을 여는 링크는 현재 창에서 비켜난 별도의 R Chromium
  창으로 열립니다. 창을 닫으면 그 페이지가 닫히고, 마지막 창을 닫으면
  R Chromium이 종료됩니다.

## 알려진 제한

- **웹 저장소가 실행 사이에 유지되지 않습니다.** 쿠키, localStorage, 사이트
  로그인은 한 세션 동안만 유효합니다. 의도된 동작입니다(브라우저가 저장소를
  메모리에서 운용합니다). 북마크는 영향을 받지 않습니다.
- **무거운 페이지는 Atom에서 느립니다.** news.naver.com은 1.5-2.5초,
  news.google.co.kr은 JavaScript가 1.33 GHz 코어에서 CPU 병목이라 6-8초가
  걸립니다. 하드웨어 한계이며 버그가 아닙니다.
- 하드웨어 가속이 없습니다. 모두 소프트웨어 렌더링입니다(Haiku에는 Chromium이
  쓸 GL이 없음). 실행기가 `--disable-gpu`를 넘기는 이유입니다.
- Chromium 87의 비공식 포트입니다. Chromium 일정에 맞춘 상류 보안 업데이트를
  받지 않으므로 민감한 계정에는 사용하지 마세요.

## 문제 해결

- **설치 스크립트가 "embedded blob verification FAILED -- not installing"이라고
  합니다.** 지정한 바이너리가 손상된 링크 결과물입니다(이 머신의 링커가 V8
  일부를 손상시킬 수 있음). 검증된 빌드로 설치하세요 --
  `/boot/home/content_shell.last-good`은 항상 검증된 빌드입니다 -- 또는
  검증 링크 스크립트로 다시 빌드하세요(`AGENTS.md`).
- **글자가 보이지 않거나 / 페이지에 글자가 나타나자마자 브라우저가 종료됩니다.**
  `/boot/home/rchromium-fonts.conf`가 없거나 읽을 수 없습니다. 설치
  스크립트를 다시 실행하거나 `assets/rchromium-fonts.conf`를 직접 복사하세요.
- **실행 직후 페이지가 빈 채로 남아 있습니다.** 데스크톱 실행기나 실행기의
  플래그로 시작했는지 확인하세요. 특히 `--disable-gpu-compositing`은 이
  백엔드에서 필수입니다.
- **Qt가 없는지 확인:** `readelf -d /boot/home/RChromium/content_shell | grep NEEDED`
  결과에 `libbe.so` 등이 나열되고 `libQt5*`는 없어야 합니다.

## AI 고지

이 프로그램은 Claude와 함께 작성되었습니다.
