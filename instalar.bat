@echo off
rem GTI Club+ : asks for the RPCS3 folder and builds the game from the user's own copy (see LEEME_INSTALADOR.md)
setlocal
cd /d "%~dp0"
echo ==============================================================
echo  GTI Club+ para PC - instalador
echo.
echo  Necesitas TU copia del juego instalada en RPCS3:
echo    - version EUROPEA  NPEB00060  con la actualizacion 1.02
echo    - (opcional) el pack de coches
echo    - el firmware de PS3 instalado en RPCS3 (para la musica)
echo  Otras regiones o versiones NO sirven.
echo ==============================================================
echo.
rem Python: the one bundled with the installer (toolchain\python, with capstone) or, without it, the system's
set "PY=%~dp0toolchain\python\python.exe"
if exist "%PY%" goto have_python
set "PY=python"
where python >nul 2>nul
if errorlevel 1 (
  echo Falta Python 3. Instalalo desde https://www.python.org/downloads/ marcando "Add python.exe to PATH" y vuelve a ejecutar este fichero.
  pause
  exit /b 1
)
:have_python
"%PY%" -c "import capstone" >nul 2>nul
if errorlevel 1 (
  echo Instalando el modulo capstone de Python...
  "%PY%" -m pip install capstone==5.0.7
  if errorlevel 1 (
    echo No se ha podido instalar capstone. Hace falta conexion a internet.
    pause
    exit /b 1
  )
)
set /p RP=Arrastra aqui la carpeta de RPCS3 y pulsa Intro:
"%PY%" instalar.py --rpcs3 %RP% --out "%~dp0GTI Club+"
echo.
pause
