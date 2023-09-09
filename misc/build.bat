@echo off
setlocal EnableDelayedExpansion

REM {avrdude -c avrisp} : List of supported AVR devices.
REM {avrdude -c asd   } : List of supported programmers.

REM In the case of strange errors relating to accessing register values,
REM try using the `--param=min-pagesize=0` argument. This seems to be a
REM bug on AVR-GCC's end.
set AVR_GCC_FLAGS= ^
	-std=c2x -fshort-enums ^
	-Werror -Wall -Wextra -fmax-errors=1 ^
	-Wno-unused-function -Wno-unused-label -Wno-unused-variable

if not exist W:\build\ (
	mkdir W:\build\
)

pushd W:\build\
	del *.o *.elf *.hex > nul 2>&1

	REM I can't find much information online at all about what bootloader is used
	REM specifically for the Arduino Leonardo. The IDE calls AVRDUDE with AVR109 as
	REM the programmer, but other than that, there's no official documentation I
	REM could find for this.

	set PROGRAM_MCU=ATmega32U4
	set PROGRAM_NAME=Diplomat
	set PROGRAMMER=avr109
	set COM_INDEX=4

	avr-gcc !AVR_GCC_FLAGS! -Os -mmcu=%PROGRAM_MCU% -c W:\src\%PROGRAM_NAME%.c
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)

	avr-gcc -mmcu=%PROGRAM_MCU% -o %PROGRAM_NAME%.elf %PROGRAM_NAME%.o
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)

	avr-objcopy -O ihex %PROGRAM_NAME%.elf %PROGRAM_NAME%.hex
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)

	avrdude -p %PROGRAM_MCU% -c %PROGRAMMER% -V -P COM%COM_INDEX% -D -Uflash:w:%PROGRAM_NAME%.hex
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)

	REM TODO Once we can get a virtual COM port set up,
	REM we can begin diagnostic outputs.
	REM
	REM start putty.exe -load "COM3_settings"

	del *.o > nul 2>&1
	:ABORT
popd W:\build\
