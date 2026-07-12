@echo off
REM ============================================================================
REM  build_windows.bat - compila Whisk3D para Windows (Visual Studio / MSVC).
REM  Nativo: corre en cmd / doble-click, SIN necesitar Git Bash ni WSL.
REM  Requiere: git, cmake y las C++ Build Tools de Visual Studio en el PATH
REM  (ver platform\windows\README.md). Genera platform\windows\build\Release\whisk3d.exe
REM  con la carpeta res\ al lado.
REM
REM  Uso:   platform\windows\build_windows.bat            (Release)
REM         platform\windows\build_windows.bat Debug      (o el config que quieras)
REM ============================================================================
setlocal

REM raiz del repo = 2 niveles arriba de este script (platform\windows\)
set "ROOT=%~dp0..\.."
pushd "%ROOT%" || exit /b 1

where cmake >nul 2>nul || (echo ERROR: cmake no esta en el PATH. Instalalo de https://cmake.org/download/ && goto :fail)
where git   >nul 2>nul || (echo ERROR: git no esta en el PATH. && goto :fail)

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"

echo == Actualizando submodulos (SDL2, Whisk3DCore, WhiskUI)...
git submodule update --init --recursive || goto :fail

echo == Configurando (platform\windows\build)... (cmake elige el Visual Studio instalado; x64 por defecto)
cmake -S . -B platform\windows\build || goto :fail

echo == Compilando (%CONFIG%)...
cmake --build platform\windows\build --config %CONFIG% --parallel || goto :fail

echo.
echo Whisk3D compilado:
echo   %ROOT%\platform\windows\build\%CONFIG%\whisk3d.exe
echo.
echo (Para el instalador .exe:  cd platform\windows\build ^&^& cpack -G NSIS   -- necesita NSIS)
popd
pause
exit /b 0

:fail
echo.
echo *** La compilacion FALLO. Revisa el error de arriba. ***
popd
pause
exit /b 1
