@echo off
rem Build and run the new-engine vs legacy-engine self-play benchmark.
rem Usage: tests\run_ai_bench.bat [number_of_games]   (default 4 games)
rem Exit code 0 = benchmark ran without illegal moves / crashes.
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
set "OUT=%TEMP%\ifdream_ai_bench"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"
echo [build] ai_bench.exe
cl /nologo /std:c++17 /EHsc /utf-8 /W4 /O2 /I "%REPO%\src" ^
   "%REPO%\tests\ai_bench.cpp" ^
   "%REPO%\src\core\GameState.cpp" ^
   "%REPO%\src\core\Rules.cpp" ^
   "%REPO%\src\ai\Evaluator.cpp" ^
   "%REPO%\src\ai\Search.cpp" ^
   "%REPO%\src\ai\LegacyAi.cpp" ^
   /Fe:ai_bench.exe
if errorlevel 1 (
  echo [ERROR] ai_bench.exe build failed
  popd
  exit /b 1
)
echo [run] ai_bench.exe %*
.\ai_bench.exe %*
set "RC=%ERRORLEVEL%"
popd
exit /b %RC%
:no_toolchain
echo [ERROR] Visual Studio C++ toolchain not found.
exit /b 1
