# LightBrowser (Phase 1)

Minimal Qt 6 + QtWebEngine browser: address bar, back/forward/reload, one tab.

## Build

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="D:\Qt\6.8.3\msvc2022_64"
cmake --build build --config Release
```

## Run

```powershell
.\build\Release\LightBrowser.exe
```

If it fails to find Qt DLLs, deploy them next to the exe:

```powershell
D:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe --release .\build\Release\LightBrowser.exe
```
