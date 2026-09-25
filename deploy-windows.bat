@echo off
rem =====================================================================
rem  CrudQt - deploy-windows.bat
rem  Compila en Release, empaqueta DLLs y plugins con windeployqt y genera
rem  CrudQt-win64.zip listo para distribuir a cualquier Windows x64.
rem
rem  Uso:
rem     deploy-windows.bat                     (Qt en el PATH)
rem     deploy-windows.bat "C:\Qt\6.8.0\mingw_64\bin" [build-dir]
rem =====================================================================
setlocal enabledelayedexpansion

set "QT_BIN=%~1"
set "BUILD_DIR=%~2"

if not "%QT_BIN%"=="" (
    if not exist "%QT_BIN%\windeployqt.exe" (
        echo ERROR: no se encuentra windeployqt.exe en "%QT_BIN%"
        exit /b 1
    )
    set "PATH=%QT_BIN%;%PATH%"
)

where windeployqt >nul 2>nul
if errorlevel 1 (
    echo ERROR: windeployqt no esta en el PATH.
    echo Pasa la carpeta bin de Qt como primer argumento, por ejemplo:
    echo    deploy-windows.bat "C:\Qt\6.8.0\mingw_64\bin"
    exit /b 1
)

if "%BUILD_DIR%"=="" set "BUILD_DIR=build-win"

echo.
echo [1/4] Configurando y compilando en Release...
cmake -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 exit /b 1

set "APP=%BUILD_DIR%\CrudQt.exe"
if not exist "%APP%" (
    rem Con el generador Visual Studio el exe queda en Release\.
    if exist "%BUILD_DIR%\Release\CrudQt.exe" set "APP=%BUILD_DIR%\Release\CrudQt.exe"
)
if not exist "%APP%" (
    echo ERROR: no se encontro CrudQt.exe en "%BUILD_DIR%"
    exit /b 1
)

echo.
echo [2/4] Empaquetando DLLs y plugins de Qt con windeployqt...
windeployqt --release --compiler-runtime "%APP%"
if errorlevel 1 exit /b 1

rem windeployqt no copia el runtime de MinGW: se copia a mano si g++ existe.
where g++ >nul 2>nul
if not errorlevel 1 (
    for /f "delims=" %%P in ('where g++') do (
        set "GCC_DIR=%%~dpP"
        goto gcc_found
    )
)
:gcc_found
if defined GCC_DIR (
    echo.
    echo [3/4] Copiando runtime de MinGW...
    for %%D in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
        if exist "%GCC_DIR%%%D" (
            copy /y "%GCC_DIR%%%D" "%%~dpAPP%%" >nul
        )
    )
) else (
    echo.
    echo [3/4] Runtime de MinGW no detectado; windeployqt --compiler-runtime
    echo        ya cubre el runtime de MSVC si compilaste con Visual Studio.
)

for %%F in ("%APP%") do set "APP_DIR=%%~dpF"

echo.
echo [4/4] Generando CrudQt-win64.zip...
set "ZIP=%CD%\CrudQt-win64.zip"
powershell -NoProfile -Command "Compress-Archive -Path \"%APP_DIR%*\" -DestinationPath \"%ZIP%\" -Force"
if errorlevel 1 exit /b 1

echo.
echo LISTO: %ZIP%
echo La carpeta empaquetada incluye CrudQt.exe, las DLLs de Qt, plugins y runtime.
echo Distribuye el .zip a cualquier Windows x64 (con la MISMA version de Qt del build).
echo Probar con la maquina destino; si falta vcruntime/msvcp, copialas al lado del exe.
endlocal