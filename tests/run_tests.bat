@echo off
rem Build and run the unit tests. Each tests\*_tests.cpp becomes its own exe.
rem Usage: tests\run_tests.bat   (exit code 0 = all passed)
setlocal enabledelayedexpansion
set "REPO=%~dp0.."
set "VSROOT=D:\yingyonganzhuang\vscp"
if not exist "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" goto try_vswhere
goto build
:try_vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto no_toolchain
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSROOT=%%i"
:build
if not exist "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" goto no_toolchain
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
set "OUT=%TEMP%\ifdream_tests"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
set "FAILED=0"
for %%f in ("%REPO%\tests\*_tests.cpp") do (
  if /i "%%~nf"=="ai_tests" (
    echo === skipping %%~nf ^(needs src/cpp sources; run tests\run_ai_tests.bat^) ===
  ) else (
    echo === testing %%~nf ===
    cl /nologo /std:c++17 /EHsc /utf-8 /W4 /I "%REPO%\src" "%%~ff" /Fe:%%~nf.exe
    if errorlevel 1 (
      set "FAILED=1"
    ) else (
      .\%%~nf.exe
      if errorlevel 1 set "FAILED=1"
    )
  )
)
popd
if "!FAILED!"=="1" exit /b 1
echo === all test targets passed ===
exit /b 0
:no_toolchain
echo [ERROR] Visual Studio C++ toolchain not found.
exit /b 1
