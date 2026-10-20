@echo off
setlocal enabledelayedexpansion
if ["%MVIMPACT_ACQUIRE_SOURCE_DIR%"]==[""] (
	if ["%MVIMPACT_ACQUIRE_DIR%"]==[""] (
		echo Either MVIMPACT_ACQUIRE_DIR or MVIMPACT_ACQUIRE_SOURCE_DIR must be set
		exit /b 1
	) else (
		GOTO ENDUSER
	)
) else (
REM Development stuff...
	set rootDir=%MVIMPACT_ACQUIRE_SOURCE_DIR%
)

set pythonWrapperDir=%rootDir%\LanguageBindings\Python
set BUILDDIR_DN=%pythonWrapperDir%\Output
@echo on
set PLATFORM=win
python "%pythonWrapperDir%\setup.py" install
cd "%rootDir%\LanguageBindings\Python"
exit /b 0

REM End-user stuff...
:ENDUSER
@echo off

REM Variables
SET MSPYTHONCOMPILERPATH=C:\Users\%USERNAME%\AppData\Local\Programs\Common\Microsoft\Visual C++ for Python\9.0
SET DISTUTILSDETECTIONWORKAROUNDLINK=C:\Users\%USERNAME%\AppData\Local\Programs\Common\Microsoft\VC
SET MVIMPACT_ACQUIRE_DIR_ORIGINAL=%MVIMPACT_ACQUIRE_DIR%
SET MVIMPACT_ACQUIRE_DIR_SPACELESS=%TEMP%\mvIA
SET PLATFORM=win

REM some checks...
net session >nul 2>&1
IF %errorLevel% NEQ 0 (
	echo Please execute this script as an Administrator!
	pause
	exit /b 5
)
IF "%MVIMPACT_ACQUIRE_DIR_ORIGINAL%"=="" ( REM Now redundant but it does not hurt to check again.
	echo Environment Variable 'MVIMPACT_ACQUIRE_DIR' not found on your system!
	pause
	exit /b 203
)

REM find out installed Python Version
python --version > %TEMP%\pv.txt 2>&1
IF %errorLevel% NEQ 0 (
	echo Python either not installed, or the path to the executable is not listed in the PATH environmental variable!
	pause
	exit /b 1
)

FOR /f "tokens=2,3 delims=. " %%a IN (%TEMP%\pv.txt) do set PYTHONVERMAJOR=%%a&&set PYTHONVERMINOR=%%b
del %TEMP%\pv.txt

echo(
echo Python %PYTHONVERMAJOR%.%PYTHONVERMINOR% detected on your system...
echo(
echo NOTE: If you need to build mvIMPACT Acquire for another version of Python please
echo include the path to the desired version executable in the PATH environment Variable.
echo(

REM since windows batch files are really the lowest kind of low, we have to do some logic here using variables...
IF %PYTHONVERMAJOR% EQU 2 SET NEEDVC90=1
IF %PYTHONVERMAJOR% EQU 3 IF %PYTHONVERMINOR% LEQ 2 SET NEEDVC90=1
IF %PYTHONVERMAJOR% EQU 3 IF %PYTHONVERMINOR% EQU 3 SET NEEDVC100=1
IF %PYTHONVERMAJOR% EQU 3 IF %PYTHONVERMINOR% EQU 4 SET NEEDVC100=1
IF %PYTHONVERMAJOR% EQU 3 IF %PYTHONVERMINOR% GEQ 5 SET NEEDVC140=1

IF DEFINED NEEDVC90 (
	echo Microsoft Visual C++ Compiler 9.0 needed for building mvIMPACT Acquire for Python %PYTHONVERMAJOR%.%PYTHONVERMINOR%...
	echo(
	IF NOT EXIST "%MSPYTHONCOMPILERPATH%" (
		IF "%VS90COMNTOOLS%"=="" (
			echo Cannot find 'Microsoft Visual C++ Compiler for Python 2.7' or 'MS Visual Studio 2008' on your system!
			pause
			exit /b 3
		)
	)

	REM making 'Microsoft Visual C++ Compiler for Python 2.7' detectable by python distutils
	IF "%VS90COMNTOOLS%"=="" (
		REM this means Visual Studio 2008 is not installed, so we may use the VS90COMNTOOLS variable for the 'Microsoft Visual C++ Compiler for Python 2.7'
		IF NOT EXIST %DISTUTILSDETECTIONWORKAROUNDLINK%\NUL (
			echo Trying to reconcile 'Microsoft Visual C++ Compiler for Python 2.7' and 'distutils'...
			mklink /J "%DISTUTILSDETECTIONWORKAROUNDLINK%" "%MSPYTHONCOMPILERPATH%" 2>&1 1>NUL
			REM One could make it also permanent with: setx VS90COMNTOOLS "%MSPYTHONCOMPILERPATH%\\" /M 2>&1 1>NUL
			SET VS90COMNTOOLS=%MSPYTHONCOMPILERPATH%\
		)
	) ELSE (
		IF NOT EXIST %DISTUTILSDETECTIONWORKAROUNDLINK%\NUL (
			echo MS Visual Studio 2008 installation detected, distutils should detect the MS C++ Compiler without problems...
		)
	)
) ELSE (
	IF DEFINED NEEDVC100 (
		echo Microsoft Visual C++ Compiler 10.0 needed for building mvIMPACT Acquire for Python %PYTHONVERMAJOR%.%PYTHONVERMINOR%...
		echo(
		IF "%VS100COMNTOOLS%"=="" (
			echo Cannot find a version of 'MS Visual Studio 2010' or another matching C++ 10.0 compiler on your system!
			echo Python 'distutils' will nevertheless try to locate and use a suitable compiler...
		)
	)
	IF DEFINED NEEDVC140 (
		echo Microsoft Visual C++ Compiler 14.0 needed for building mvIMPACT Acquire for Python %PYTHONVERMAJOR%.%PYTHONVERMINOR%...
			echo(
		IF "%VS140COMNTOOLS%"=="" (
			SET VSTOOLPATH="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
			IF EXIST "%VSTOOLPATH%" (
				FOR /F "usebackq tokens=*" %%i in (`%VSTOOLPATH% -latest -property installationPath`) do (
					SET VSPATH=%%i
				)  
			)
			IF "%VSPATH%"=="" (
				echo Cannot find a version of 'MS Visual Studio 2015' or later [i.e. Microsoft Visual C++ Compiler 14.0 or above] on your system!
				echo Python 'distutils' will nevertheless try to locate and use a suitable compiler...
			)
		)
	)
)

echo(
REM creating symlink without spaces for seamless mvIMPACT Acquire headers and libraries access
mklink /J "%MVIMPACT_ACQUIRE_DIR_SPACELESS%" "%MVIMPACT_ACQUIRE_DIR_ORIGINAL%" 2>&1 1>NUL

REM tricking the setup script for a little while to use the spaceless paths
SET MVIMPACT_ACQUIRE_DIR=%MVIMPACT_ACQUIRE_DIR_SPACELESS%
SET BUILDDIR_DN=%MVIMPACT_ACQUIRE_DIR%\LanguageBindings\Python\Output
echo Building mvIMPACT Acquire Python wrapper...
python "%MVIMPACT_ACQUIRE_DIR_SPACELESS%\LanguageBindings\Python\setup.py" install
set returnStatus=%ERRORLEVEL% 
if /I %returnStatus% EQU 0 ( 
	echo mvIMPACT Acquire Python wrapper ready!
	SET RESULTSTRING=built!
) else ( 
	echo Something went wrong! mvIMPACT Acquire Python wrapper not built! ERROR: %returnStatus%
	SET RESULTSTRING=NOT built!
)
SET MVIMPACT_ACQUIRE_DIR=%MVIMPACT_ACQUIRE_DIR_ORIGINAL%

REM removing symlinks and unsetting env. variables
echo Tidying up...
rmdir "%MVIMPACT_ACQUIRE_DIR_SPACELESS%" 2>&1 1>NUL
IF DEFINED NEEDVC90 (
	rmdir "%DISTUTILSDETECTIONWORKAROUNDLINK%" 2>&1 1>NUL
)
SET MSPYTHONCOMPILERPATH=
SET DISTUTILSDETECTIONWORKAROUNDLINK=
SET MVIMPACT_ACQUIRE_DIR_ORIGINAL=
SET MVIMPACT_ACQUIRE_DIR_SPACELESS=
SET BUILDDIR_DN=
SET PLATFORM=
SET NEEDVC90=
SET NEEDVC100=
SET NEEDVC140=
SET PYTHONVER=
SET PYTHONVERMAJOR=
SET PYTHONVERMINOR=
echo(
echo ------------------------------------------------------------------------
echo              mvIMPACT Acquire Python wrapper %RESULTSTRING%
echo ------------------------------------------------------------------------

IF "%RESULTSTRING%"=="NOT built!" (
	echo(
	echo Regarding issues compiling the mvIMPACT Acquire wrapper for Python please visit
	echo                  https://wiki.python.org/moin/WindowsCompilers
	echo for more information about installing and using a suitable compiler.
	echo(
	exit /b 1
)
SET RESULTSTRING=
pause
