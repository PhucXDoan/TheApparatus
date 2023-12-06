@echo off
setlocal EnableDelayedExpansion

cls

W:\build\Microservices.exe extractorv2 ^
	W:\data\extractor\main\ ^
	W:\data\screenshots\anagrams_english_6\ ^
	W:\data\screenshots\anagrams_english_7\ ^
	W:\data\screenshots\anagrams_russian\ ^
	W:\data\screenshots\anagrams_german\ ^
	W:\data\screenshots\anagrams_french\ ^
	W:\data\screenshots\anagrams_spanish\ ^
	W:\data\screenshots\anagrams_italian\ ^
	W:\data\screenshots\wordhunt_4x4\ ^
	W:\data\screenshots\wordhunt_o\ ^
	W:\data\screenshots\wordhunt_x\ ^
	W:\data\screenshots\wordhunt_5x5\ ^
	W:\data\screenshots\wordbites\ ^
	--clear-output-dir

W:\build\Microservices.exe collectune W:\data\collectune\main\ W:\data\masks\ W:\data\extractor\main\ --clear-output-dir
W:\build\Microservices.exe maskiversev2 W:\data\collectune\main\masks.h W:\data\collectune\main\
