@echo off
setlocal EnableDelayedExpansion

REM To list AVRDUDE's supported AVR devices: "avrdude -c avrisp".
REM To list AVRDUDE's supported programmers: "avrdude -c asd".

set AVR_GCC_PRACTICAL_DISABLED_WARNINGS=^
	-Wno-unused-function -Wno-implicit-fallthrough -Wno-missing-field-initializers

set AVR_GCC_DEVELOPMENT_DISABLED_WARNINGS=^
	-Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-comment

set AVR_GCC_FLAGS= ^
	-std=c2x -Os -D DEBUG=1 -fshort-enums -I W:/ -fno-strict-aliasing ^
	-Werror -Wall -Wextra -fmax-errors=1 !AVR_GCC_PRACTICAL_DISABLED_WARNINGS! !AVR_GCC_DEVELOPMENT_DISABLED_WARNINGS! ^
	--param=min-pagesize=0

REM The "--param=min-pagesize=0" flag is to make GCC shut up about a false-positive warning in the
REM case of writing to certain registers (e.g. "UDCON &= ~(1 << DETACH);"). I believe this is make
REM GCC know that 0x0000 is a valid address according to what was said in (1), but I can't guarantee
REM that's what's really happening here.
REM
REM (1) Bug Thread @ URL(gcc.gnu.org/bugzilla//show_bug.cgi?id=105523) (Accessed: September 26, 2023).

set DIPLOMAT_BOOTLOADER_BAUD_SIGNAL=1200
set DIPLOMAT_DIAGNOSTIC_BAUD_SIGNAL=1201

REM set DIPLOMAT_BOOTLOADER_COM=12
set DIPLOMAT_BOOTLOADER_COM=4

set DIPLOMAT_DIAGNOSTIC_COM=10
set DIPLOMAT_PROGRAMMER=avr109
set DIPLOMAT_MCU=ATmega32U4
set DIPLOMAT_AVR_GCC_ARGS= ^
	!AVR_GCC_FLAGS! -mmcu=!DIPLOMAT_MCU! ^
	-D BOOTLOADER_BAUD_SIGNAL=!DIPLOMAT_BOOTLOADER_BAUD_SIGNAL! ^
	-D DIAGNOSTIC_BAUD_SIGNAL=!DIPLOMAT_DIAGNOSTIC_BAUD_SIGNAL!

set NERD_DIAGNOSTIC_BAUD=76800
set NERD_BOOTLOADER_COM=12
set NERD_DIAGNOSTIC_COM=12

set NERD_PROGRAMMER=wiring
set NERD_MCU=ATmega2560
set NERD_AVR_GCC_ARGS= ^
	!AVR_GCC_FLAGS! -mmcu=!NERD_MCU!

if not exist W:\build\ (
	mkdir W:\build\
)

pushd W:\build\
	del *.s *.o *.elf *.hex > nul 2>&1

	REM
	REM Compile C source code into assembly and ELF.
	REM

	avr-gcc !DIPLOMAT_AVR_GCC_ARGS! -S -fverbose-asm         W:\src\Diplomat.c
	avr-gcc !DIPLOMAT_AVR_GCC_ARGS! -o W:\build\Diplomat.elf W:\src\Diplomat.c
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)

	avr-gcc !NERD_AVR_GCC_ARGS! -S -fverbose-asm     W:\src\Nerd.c
	avr-gcc !NERD_AVR_GCC_ARGS! -o W:\build\Nerd.elf W:\src\Nerd.c
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)

	REM
	REM Convert ELF into HEX.
	REM

	avr-objcopy -O ihex -j .text -j .data Diplomat.elf Diplomat.hex
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)

	avr-objcopy -O ihex -j .text -j .data Nerd.elf Nerd.hex
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)

	REM
	REM Find bootloader COM, flash, and start PuTTY.
	REM

	mode | findstr "COM!NERD_BOOTLOADER_COM!:" > nul
	if !ERRORLEVEL! == 0 (
		set AVRDUDE_ARGS=-p !NERD_MCU! -c !NERD_PROGRAMMER! -V -P COM!NERD_BOOTLOADER_COM! -D -Uflash:w:Nerd.hex
		set PUTTY_ARGS=-serial COM!NERD_DIAGNOSTIC_COM! -sercfg !NERD_DIAGNOSTIC_BAUD!,8,n,1,N
		goto UPLOAD
	)

	mode | findstr "COM!DIPLOMAT_DIAGNOSTIC_COM!:" > nul
	if !ERRORLEVEL! == 0 (
		mode COM!DIPLOMAT_DIAGNOSTIC_COM!: BAUD=!DIPLOMAT_BOOTLOADER_BAUD_SIGNAL! > nul
	)

	for /L %%n in (1,1,64) do (
		mode | findstr "COM!DIPLOMAT_BOOTLOADER_COM!:" > nul
		if !ERRORLEVEL! == 0 (
			set AVRDUDE_ARGS=-p !DIPLOMAT_MCU! -c !DIPLOMAT_PROGRAMMER! -V -P COM!DIPLOMAT_BOOTLOADER_COM! -D -Uflash:w:Diplomat.hex
			REM set PUTTY_ARGS=-serial COM!DIPLOMAT_DIAGNOSTIC_COM! -sercfg !DIPLOMAT_DIAGNOSTIC_BAUD_SIGNAL!,8,n,1,N
			goto UPLOAD
		) else (
			ping 127.0.0.1 -n 1 -w 500 > nul
		)
	)

	echo No bootloader found.
	goto ABORT

	:UPLOAD
	avrdude !AVRDUDE_ARGS!
	if not !ERRORLEVEL! == 0 (
		goto ABORT
	)
	if not "!PUTTY_ARGS!" == "" (
		start putty.exe -load "Default Settings" !PUTTY_ARGS!
	)

	del *.o > nul 2>&1
	:ABORT
popd W:\build\

REM TODO I can't find much information online at all about what bootloader is used specifically for
REM the Arduino Leonardo or ATmega32U4 variants. The IDE calls AVRDUDE with AVR109 as the programmer
REM but other than that, there's no official documentation I could find for this. Perhaps it's what
REM every ATmega32U4 has for the bootloader (if installed)?
REM Update: What's wiring programmer??!
