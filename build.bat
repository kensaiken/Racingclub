@echo off
rem Build GTiPort with the bundled llvm-mingw toolchain (no Visual Studio needed).
setlocal
set TC=%USERPROFILE%\.local\share\retcomm\toolchains\cmake-clang-v1\latest
if not exist "%TC%\env.bat" (
  echo No se encuentra el toolchain en %TC%
  exit /b 1
)
call "%TC%\env.bat"
cd /d "%~dp0"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH="%TC%\deps" || exit /b 1
cmake --build build || exit /b 1
echo.
echo Compilado: %~dp0build\gtiport.exe
