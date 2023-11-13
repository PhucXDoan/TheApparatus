@echo off
setlocal EnableDelayedExpansion

cls

REM W:\build\Microservices.exe eaglepeek ^
REM 	W:\data\screenshots\anagrams_6\ ^
REM 	W:\data\screenshots\anagrams_7\ ^
REM 	W:\data\screenshots\wordhunt_4x4\ ^
REM 	W:\data\screenshots\wordhunt_o\ ^
REM 	W:\data\screenshots\wordhunt_x\ ^
REM 	W:\data\screenshots\wordhunt_5x5\ ^
REM 	W:\data\screenshots\wordbites\

REM W:\build\Microservices.exe extractorv2 W:\data\extractor\temp\ W:\data\screenshots\wordbites\ --clear-output-dir
REM W:\build\Microservices.exe collectune W:\data\collectune\temp\ W:\data\masks\ W:\data\extractor\temp\ --clear-output-dir
REM W:\build\Microservices.exe maskiversev2 W:\data\masks.h W:\data\collectune\temp\

W:\build\Microservices.exe extractorv2 ^
	W:\data\extractor\main\ ^
	W:\data\screenshots\anagrams_6\ ^
	W:\data\screenshots\anagrams_7\ ^
	W:\data\screenshots\wordhunt_4x4\ ^
	W:\data\screenshots\wordhunt_o\ ^
	W:\data\screenshots\wordhunt_x\ ^
	W:\data\screenshots\wordhunt_5x5\ ^
	W:\data\screenshots\wordbites\ ^
	--clear-output-dir
W:\build\Microservices.exe collectune W:\data\collectune\main\ W:\data\masks\ W:\data\extractor\main\ --clear-output-dir
W:\build\Microservices.exe maskiversev2 W:\data\collectune\main\masks.h W:\data\collectune\main\
