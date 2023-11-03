@echo off
setlocal EnableDelayedExpansion

cls

set FOLDER=anagrams_6

REM W:\build\Microservices.exe eaglepeek W:\data\screenshots\!FOLDER!\

W:\build\Microservices.exe extractor    W:\data\screenshots\!FOLDER!\ W:\data\extractor\!FOLDER!\ --clear-output-dir
W:\build\Microservices.exe monochromize W:\data\extractor\!FOLDER!\ W:\data\monochromize\!FOLDER!\ --clear-output-dir
W:\build\Microservices.exe stretchie    W:\data\monochromize\!FOLDER!\ W:\data\stretchie\!FOLDER!\ --clear-output-dir
W:\build\Microservices.exe collectune   W:\data\masks\ W:\data\stretchie\!FOLDER!\ W:\data\collectune\!FOLDER!\ --clear-output-dir

del W:\data\meltingpot\!FOLDER!\* /Q
for /D %%i in (W:\data\collectune\!FOLDER!\*) do (
	W:\build\Microservices.exe meltingpot %%i W:\data\meltingpot\!FOLDER!\%%~nxi.bmp --or
)


REM W:\build\Microservices.exe catchya W:\data\masks\ "W:\data\screenshots\anagrams_6\Image (9).bmp"

REM W:\build\Microservices.exe meltingpot W:\data\monochromize\!FOLDER!\ W:\data\meltingpot\!FOLDER!.bmp --or

REM W:\build\Microservices.exe maskiverse W:\data\masks\
