@echo off
setlocal EnableDelayedExpansion

REM To list AVRDUDE's supported AVR devices: "avrdude -c avrisp".
REM To list AVRDUDE's supported programmers: "avrdude -c asd".

set AVR_GCC_PRACTICAL_DISABLED_WARNINGS=^
	-Wno-unused-function -Wno-implicit-fallthrough

set AVR_GCC_DEVELOPMENT_DISABLED_WARNINGS=^
	-Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter

set AVR_GCC_FLAGS= ^
	-std=c2x -Os -D DEBUG=1 -fshort-enums -I W:/ ^
	-Werror -Wall -Wextra -fmax-errors=1 !AVR_GCC_PRACTICAL_DISABLED_WARNINGS! !AVR_GCC_DEVELOPMENT_DISABLED_WARNINGS! ^
	--param=min-pagesize=0

REM The "--param=min-pagesize=0" flag is to make GCC shut up about a false-positive warning in the
REM case of writing to certain registers (e.g. "UDCON &= ~(1 << DETACH);"). I believe this is make
REM GCC know that 0x0000 is a valid address according to what was said in (1), but I can't guarantee
REM that's what's really happening here.
REM
REM (1) Bug Thread @ URL(gcc.gnu.org/bugzilla//show_bug.cgi?id=105523) (Accessed: September 26, 2023).

if not exist W:\build\ (
	mkdir W:\build\
)

pushd W:\build\
	*.o *.elf *.hex > nul 2>&1

	set BOOTLOADER_BAUD_SIGNAL=1200
	set DIAGNOSTIC_BAUD_SIGNAL=1201

	set PROGRAM_MCU=ATmega32U4
	set PROGRAM_NAME=Diplomat
	set PROGRAMMER=avr109
	set BOOTLOADER_COM=4
	set DIAGNOSTIC_COM=5

	REM
	REM Compile C source code into ELF (describes memory layout of main program).
	REM

	avr-gcc !AVR_GCC_FLAGS! -mmcu=!PROGRAM_MCU! ^
		-D BOOTLOADER_BAUD_SIGNAL=!BOOTLOADER_BAUD_SIGNAL! ^
		-D DIAGNOSTIC_BAUD_SIGNAL=!DIAGNOSTIC_BAUD_SIGNAL! ^
		-o W:\build\!PROGRAM_NAME!.elf W:\src\!PROGRAM_NAME!.c
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)

	REM
	REM Convert ELF into HEX (raw instructions for the MCU to execute).
	REM

	avr-objcopy -O ihex !PROGRAM_NAME!.elf !PROGRAM_NAME!.hex
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)

	REM
	REM Check if the bootloader's COM is available.
	REM

	mode | findstr "COM!BOOTLOADER_COM!:" > nul
	if !ERRORLEVEL! == 0 (
		goto BOOTLOADER_COM_FOUND
	)

	REM
	REM Send BOOTLOADER_BAUD_SIGNAL to diagnostic COM if available; this should open up bootloader COM.
	REM

	mode | findstr "COM!DIAGNOSTIC_COM!:" > nul
	if !ERRORLEVEL! == 0 (
		mode COM!DIAGNOSTIC_COM!: BAUD=!BOOTLOADER_BAUD_SIGNAL! > nul
	) else (
		goto NO_BOOTLOADER_FOUND
	)

	REM
	REM Wait for booloader.
	REM The "ping" is have a slight delay. Yes. Windows Batch programming sucks.
	REM

	for /L %%n in (1,1,100) do (
		mode | findstr "COM!BOOTlOADER_COM!:" > nul
		if !ERRORLEVEL! == 0 (
			goto BOOTLOADER_COM_FOUND
		) else (
			ping 127.0.0.1 -n 1 -w 500 >nul
		)
	)

	REM
	REM Abort if there's no bootloader COM.
	REM

	:NO_BOOTLOADER_FOUND
	echo No bootloader found.
	goto ABORT

	REM
	REM Flash the MCU.
	REM

	:BOOTLOADER_COM_FOUND
	avrdude -p !PROGRAM_MCU! -c !PROGRAMMER! -V -P COM!BOOTLOADER_COM! -D -Uflash:w:!PROGRAM_NAME!.hex
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)

	REM
	REM Wait for diagnostic COM to appear and open PuTTY.
	REM

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

	start putty.exe -load "Default Settings" -serial COM!DIAGNOSTIC_COM! -sercfg !DIAGNOSTIC_BAUD_SIGNAL!,8,n,1,N

	del *.o > nul 2>&1
	:ABORT
popd W:\build\

REM TODO I can't find much information online at all about what bootloader is used specifically for
REM the Arduino Leonardo or ATmega32U4 variants. The IDE calls AVRDUDE with AVR109 as the programmer
REM but other than that, there's no official documentation I could find for this. Perhaps it's what
REM every ATmega32U4 has for the bootloader (if installed)?
