@echo off
setlocal EnableDelayedExpansion

REM {avrdude -c avrisp} : List of supported AVR devices.
REM {avrdude -c asd   } : List of supported programmers.

set AVR_GCC_FLAGS= ^
	-std=c2x -fshort-enums -I W:/ ^
	-Werror -Wall -Wextra -fmax-errors=1 ^
	-Wno-unused-function -Wno-unused-label -Wno-unused-variable ^
	-Wno-implicit-fallthrough -Wno-unused-but-set-variable ^
	--param=min-pagesize=0

REM There is this strange warning saying
REM     > array subscript 0 is outside array bounds of 'volatile uint8_t[0]' {aka 'volatile unsigned char[]'}
REM in certain usages to registers.
REM
REM The following code would trigger such warning:
REM     > UDCON &= ~(1 << DETACH)
REM
REM Using
REM     > --param=min-pagesize=0
REM seems to calm AVR-GCC down.

if not exist W:\build\ (
	mkdir W:\build\
)

pushd W:\build\
	*.o *.elf *.hex > nul 2>&1

	REM TODO(Programmer?).
	REM     I can't find much information online at all about what bootloader is used
	REM     specifically for the Arduino Leonardo. The IDE calls AVRDUDE with AVR109 as
	REM     the programmer, but other than that, there's no official documentation I
	REM     could find for this.

	set PROGRAM_MCU=ATmega32U4
	set PROGRAM_NAME=Diplomat
	set PROGRAMMER=avr109
	set BOOTLOADER_COM=4
	set DIAGNOSTIC_COM=5

	mode | findstr "COM!BOOTLOADER_COM!:" > nul
	if not !ERRORLEVEL! == 0 (
		mode COM!DIAGNOSTIC_COM!: BAUD=1200 > nul
	)

	avr-gcc !AVR_GCC_FLAGS! -Os -mmcu=!PROGRAM_MCU! -c W:\src\!PROGRAM_NAME!.c
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)

	avr-gcc -mmcu=!PROGRAM_MCU! -o !PROGRAM_NAME!.elf !PROGRAM_NAME!.o
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)

	avr-objcopy -O ihex !PROGRAM_NAME!.elf !PROGRAM_NAME!.hex
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)

	for /L %%n in (1,1,100) do (
		mode | findstr "COM!BOOTlOADER_COM!:" > nul
		if !ERRORLEVEL! == 0 (
			goto BREAK_WAITING_FOR_BOOTlOADER_COM
		) else (
			ping 127.0.0.1 -n 1 -w 500 >nul
		)
	)
	echo No bootloader found.
	goto ABORT
	:BREAK_WAITING_FOR_BOOTlOADER_COM

	avrdude -p !PROGRAM_MCU! -c !PROGRAMMER! -V -P COM!BOOTLOADER_COM! -D -Uflash:w:!PROGRAM_NAME!.hex
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)

	for /L %%n in (1,1,500) do (
		mode | findstr "COM5:" > nul
		if !ERRORLEVEL! == 0 (
			goto BREAK_WAITING_FOR_DIAGNOSTIC_COM
		) else (
			ping 127.0.0.1 -n 1 -w 500 >nul
		)
	)
	echo No diagnostic COM found.
	goto ABORT
	:BREAK_WAITING_FOR_DIAGNOSTIC_COM

	start putty.exe -load "Default Settings" -serial COM!DIAGNOSTIC_COM!

	del *.o > nul 2>&1
	:ABORT
popd W:\build\
