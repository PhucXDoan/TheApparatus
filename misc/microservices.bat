@echo off
setlocal EnableDelayedExpansion

cls

set FOLDER=anagrams_6

REM W:\build\Microservices.exe extractor    W:\data\screenshots\!FOLDER!\ W:\data\extractor\!FOLDER!\ --clear-output-dir
REM W:\build\Microservices.exe monochromize W:\data\extractor\!FOLDER!\ W:\data\monochromize\!FOLDER!\ --clear-output-dir
REM W:\build\Microservices.exe stretchie    W:\data\monochromize\!FOLDER!\ W:\data\stretchie\!FOLDER!\ --clear-output-dir
REM W:\build\Microservices.exe collectune   W:\data\masks\ W:\data\stretchie\!FOLDER!\ W:\data\collectune\!FOLDER!\ --clear-output-dir
REM 
REM for /D %%i in (W:\data\collectune\!FOLDER!\*) do (
REM 	W:\build\Microservices.exe meltingpot %%i W:\data\meltingpot\!FOLDER!\%%~nxi.bmp --or
REM )

REM W:\build\Microservices.exe meltingpot W:\data\monochromize\!FOLDER!\ W:\data\meltingpot\!FOLDER!.bmp --or

W:\build\Microservices.exe maskiverse W:\data\masks\
