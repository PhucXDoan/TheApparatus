@echo off
setlocal EnableDelayedExpansion

cls

set FOLDER=temp

W:\build\Microservices.exe extractor W:\data\screenshots\!FOLDER!\ W:\data\extractor\!FOLDER!\ --clear-output-dir
W:\build\Microservices.exe monochromize W:\data\extractor\!FOLDER!\ W:\data\monochromize\!FOLDER!\ --clear-output-dir
W:\build\Microservices.exe meltingpot W:\data\monochromize\!FOLDER!\ W:\data\meltingpot\!FOLDER!.bmp --or
