@echo off
setlocal EnableDelayedExpansion

cls

set FOLDER=temp

REM W:\build\Microservices.exe extractor    W:\data\extractor\!FOLDER!\ W:\data\screenshots\!FOLDER!\ --clear-output-dir
W:\build\Microservices.exe extractorv2 ^
	W:\data\extractor\!FOLDER!\ ^
	W:\data\screenshots\anagrams_6\ ^
	W:\data\screenshots\anagrams_7\ ^
	W:\data\screenshots\wordhunt_4x4\ ^
	W:\data\screenshots\wordhunt_o\ ^
	W:\data\screenshots\wordhunt_x\ ^
	W:\data\screenshots\wordhunt_5x5\ ^
	--clear-output-dir
W:\build\Microservices.exe collectune W:\data\collectune\!FOLDER!\ W:\data\masks\ W:\data\extractor\!FOLDER!\ --clear-output-dir
