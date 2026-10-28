@echo off
rem Build and run the independent tactical tests of the new Connect-6 AI.
rem Only the tests are built here; src\ must not be modified by this script.
rem Usage: tests\run_ai_tests.bat   (exit code 0 = all cases passed)
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
set "OUT=%TEMP%\ifdream_ai_tests"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
echo [build] ai_tests.exe
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 /I "%REPO%\src" ^
   "%REPO%\tests\ai_tests.cpp" ^
   "%REPO%\src\core\GameState.cpp" ^
   "%REPO%\src\core\Rules.cpp" ^
   "%REPO%\src\ai\Evaluator.cpp" ^
   "%REPO%\src\ai\Search.cpp" ^
   "%REPO%\src\ai\LegacyAi.cpp" ^
   /Fe:ai_tests.exe
if errorlevel 1 (
  echo [ERROR] ai_tests.exe build failed
  popd
  exit /b 1
)
echo [run] ai_tests.exe
.\ai_tests.exe
set "RC=%ERRORLEVEL%"
popd
exit /b %RC%
:no_toolchain
echo [ERROR] Visual Studio C++ toolchain not found.
exit /b 1
