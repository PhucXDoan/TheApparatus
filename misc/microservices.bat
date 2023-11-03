@echo off
setlocal EnableDelayedExpansion

cls

set FOLDER=anagrams_7

REM W:\build\Microservices.exe extractor    W:\data\extractor\!FOLDER!\ W:\data\screenshots\anagrams_6\ W:\data\screenshots\anagrams_7\ --clear-output-dir
REM W:\build\Microservices.exe monochromize W:\data\monochromize\!FOLDER!\ W:\data\extractor\!FOLDER!\ --clear-output-dir
REM W:\build\Microservices.exe stretchie    W:\data\stretchie\!FOLDER!\ W:\data\monochromize\!FOLDER!\ --clear-output-dir
REM W:\build\Microservices.exe collectune   W:\data\collectune\!FOLDER!\ W:\data\masks\ W:\data\stretchie\!FOLDER!\ --clear-output-dir
REM 
REM del W:\data\meltingpot\!FOLDER!\* /Q
REM for /D %%i in (W:\data\collectune\!FOLDER!\*) do (
REM 	W:\build\Microservices.exe meltingpot W:\data\meltingpot\!FOLDER!\%%~nxi.bmp %%i --or
REM )

W:\build\Microservices.exe maskiverse W:\data\meltingpot\!FOLDER!\

REM W:\build\Microservices.exe eaglepeek W:\data\screenshots\anagrams_6\ W:\data\screenshots\anagrams_7\
REM W:\build\Microservices.exe catchya W:\data\masks\ "W:\data\screenshots\anagrams_6\Image (9).bmp"
REM W:\build\Microservices.exe meltingpot W:\data\meltingpot\!FOLDER!.bmp W:\data\monochromize\!FOLDER!\ --or
