@echo off
setlocal EnableDelayedExpansion

cls

W:\build\Microservices.exe extractorv2 ^
	W:\data\extractor\temp\ ^
	W:\data\screenshots\anagrams_italian\ ^
	--clear-output-dir

W:\build\Microservices.exe collectune W:\data\collectune\temp\ W:\data\masks\ W:\data\extractor\temp\ --clear-output-dir

REM W:\build\Microservices.exe extractorv2 ^
REM 	W:\data\extractor\main\ ^
REM 	W:\data\screenshots\anagrams_english_6\ ^
REM 	W:\data\screenshots\anagrams_english_7\ ^
REM 	W:\data\screenshots\anagrams_russian\ ^
REM 	W:\data\screenshots\anagrams_german\ ^
REM 	W:\data\screenshots\anagrams_french\ ^
REM 	W:\data\screenshots\anagrams_spanish\ ^
REM 	W:\data\screenshots\anagrams_italian\ ^
REM 	W:\data\screenshots\wordhunt_4x4\ ^
REM 	W:\data\screenshots\wordhunt_o\ ^
REM 	W:\data\screenshots\wordhunt_x\ ^
REM 	W:\data\screenshots\wordhunt_5x5\ ^
REM 	W:\data\screenshots\wordbites\ ^
REM 	--clear-output-dir
REM 
REM W:\build\Microservices.exe collectune W:\data\collectune\main\ W:\data\masks\ W:\data\extractor\main\ --clear-output-dir
REM W:\build\Microservices.exe maskiversev2 W:\data\collectune\main\masks.h W:\data\collectune\main\
