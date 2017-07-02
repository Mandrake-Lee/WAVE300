@echo off

set WIRESHARK_DIR="C:\Program Files\Wireshark"

if /I "%1" == "/h" (
    goto usage
)

if /I "%1" == "" (
    goto usage
)
set IP_ADDRESS=%1

if /I "%2" == "" (
    goto usage
)
set SCD_DIR=%2

if /I "%3" == "/t" (
    if /I "%4" == "" (
        goto usage
    )
    set FILENAME=%4

    if /I "%5" == "" (
        goto usage
    )
    set FILESIZE=%5
)

if not exist %WIRESHARK_DIR% (
    echo . The Wireshark directory %WIRESHARK_DIR% does not exist.
    goto usage
)

if not exist %SCD_DIR% (
    echo . The ^<SCD^> directory %2 does not exist.
    goto usage
)

:: Copy DLL to the wireshark plugins directory
FOR /D %%A IN (%WIRESHARK_DIR%\plugins\*) DO copy mtlklog.dll "%%A" >NUL

:: Merge all scd files into one file
DEL all.scdm 2>NUL
FOR %%A IN (%SCD_DIR%\*.scd) DO type %%A >> all.scdm

:: Run the command
if /I "%3" == "/t" (
    nc.exe %IP_ADDRESS% 2008 | logconv -p -s all.scdm | %WIRESHARK_DIR%\Wireshark.exe -k -i - -w %FILENAME% -b filesize:%FILESIZE%
) else (
    nc.exe %IP_ADDRESS% 2008 | logconv -p -s all.scdm | %WIRESHARK_DIR%\Wireshark.exe -k -i -
)

goto end

:usage

echo .
echo . This script runs wireshark and attaches it to specified metalink device
echo . Usage^:
echo .     run-wireshark-logger.cmd ^<device DNS name or IP^> ^<Path to SCD folder with files^> ^[/t ^<output files name^> ^<output files size^>^]
echo .
echo .     /t - cut and save stream to ^<output file name^> with ^<output file size^> in Kbytes
echo .          default^: open stream in wireshark
echo .

:end