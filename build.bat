@echo off
REM tokenpizder: Windows x64 C++23 Modules build
REM Author: DoubleLuc (https://github.com/t0mil0v-rev)

if not exist "lib" mkdir "lib"

:: Try MSVC (cl.exe) with C++23 Modules
if exist "C:\PROGRA~2\MICROS~1\2022\BUILDT~1\VC\AUXILI~1\Build\vcvars64.bat" goto compile_msvc
if exist "C:\PROGRA~1\MICROS~2\2022\COMMUN~1\VC\Auxiliary\Build\vcvars64.bat" goto compile_msvc
where cl >nul 2>nul
if not errorlevel 1 goto compile_msvc

:: Try Clang-CL
where clang-cl >nul 2>nul
if not errorlevel 1 goto compile_clang_cl

:: Try GCC (g++) from MSYS2 or PATH
if exist "C:\msys64\ucrt64\bin\g++.exe" goto compile_msys_gcc
where g++ >nul 2>nul
if not errorlevel 1 goto compile_gcc

echo [!] Error: No C++20/23 compiler found.
exit /b 1

:compile_msvc
echo [*] Compiling static library (lib/tokenpizder.lib) and executable (token.exe) via MSVC C++23 Modules...
call "C:\PROGRA~2\MICROS~1\2022\BUILDT~1\VC\AUXILI~1\Build\vcvars64.bat" >nul 2>nul || call "C:\PROGRA~1\MICROS~2\2022\COMMUN~1\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
cl /std:c++latest /EHsc /O2 /utf-8 /c memory.ixx process.ixx
lib /nologo memory.obj process.obj /out:lib\tokenpizder.lib
cl /std:c++latest /EHsc /O2 /utf-8 main.cxx memory.obj process.obj /Fe:token.exe /link Psapi.lib
if not errorlevel 1 (echo [+] Build OK: lib\tokenpizder.lib, token.exe) else (echo [!] Build FAILED)
exit /b %errorlevel%

:compile_clang_cl
echo [*] Compiling via Clang-CL...
clang-cl /std:c++latest /O2 /utf-8 -Wno-everything /c memory.ixx process.ixx
llvm-ar rcs lib\tokenpizder.lib memory.obj process.obj
clang-cl /std:c++latest /O2 /utf-8 -Wno-everything main.cxx memory.obj process.obj /Fe:token.exe /link Psapi.lib
if not errorlevel 1 (echo [+] Build OK: lib\tokenpizder.lib, token.exe) else (echo [!] Build FAILED)
exit /b %errorlevel%

:compile_msys_gcc
echo [*] Compiling via GCC (C:\msys64\ucrt64\bin\g++.exe)...
"C:\msys64\ucrt64\bin\g++.exe" -std=c++23 -O2 -c memory.ixx process.ixx
"C:\msys64\ucrt64\bin\ar.exe" rcs lib\tokenpizder.a memory.o process.o
"C:\msys64\ucrt64\bin\g++.exe" -std=c++23 -O2 main.cxx memory.o process.o -lpsapi -o token.exe
if not errorlevel 1 (echo [+] Build OK: lib\tokenpizder.a, token.exe) else (echo [!] Build FAILED)
exit /b %errorlevel%

:compile_gcc
echo [*] Compiling via GCC (g++)...
g++ -std=c++23 -O2 -c memory.ixx process.ixx
ar rcs lib\tokenpizder.a memory.o process.o
g++ -std=c++23 -O2 main.cxx memory.o process.o -lpsapi -o token.exe
if not errorlevel 1 (echo [+] Build OK: lib\tokenpizder.a, token.exe) else (echo [!] Build FAILED)
exit /b %errorlevel%

