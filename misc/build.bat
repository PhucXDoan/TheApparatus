@echo off
setlocal EnableDelayedExpansion
REM To list AVRDUDE's supported AVR devices: "avrdude -c avrisp".
REM To list AVRDUDE's supported programmers: "avrdude -c asd".

set COMPILE_MICROSERVICES=0
set COMPILE_DIPLOMAT=1
set COMPILE_NERD=1

set MSVC_PRACTICAL_DISABLED_WARNINGS= ^
	/wd4668 /wd5045 /wd4820 /wd4711 /wd4710 /wd4116

set MSVC_DEVELOPMENT_DISABLED_WARNINGS= ^
	/wd4189 /wd4101 /wd4102 /wd4100 /wd4702

set MSVC_FLAGS= ^
	/nologo /Od /std:c17 /IW:\ /Zi /D DEBUG=1 /D LITTLE_ENDIAN=1 /D PROGRAM_MICROSERVICES=1 ^
	/Wall /WX !MSVC_PRACTICAL_DISABLED_WARNINGS! !MSVC_DEVELOPMENT_DISABLED_WARNINGS! ^
	/link Shlwapi.lib Shell32.lib Dbghelp.lib /incremental:no

set AVR_GCC_PRACTICAL_DISABLED_WARNINGS= ^
	-Wno-unused-function -Wno-implicit-fallthrough -Wno-missing-field-initializers

set AVR_GCC_DEVELOPMENT_DISABLED_WARNINGS= ^
	-Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-comment

set AVR_GCC_FLAGS= ^
	-std=c2x -O2 -D DEBUG=0 -D LITTLE_ENDIAN=1 -fshort-enums -I W:/ -fno-strict-aliasing ^
	-Werror -Wall -Wextra -fmax-errors=1 !AVR_GCC_PRACTICAL_DISABLED_WARNINGS! !AVR_GCC_DEVELOPMENT_DISABLED_WARNINGS! ^
	--param=min-pagesize=0

REM The "--param=min-pagesize=0" flag is to make GCC shut up about a false-positive warning in the
REM case of writing to certain registers (e.g. "UDCON &= ~(1 << DETACH);"). I believe this is make
REM GCC know that 0x0000 is a valid address according to what was said in (1), but I can't guarantee
REM that's what's really happening here.
REM
REM (1) Bug Thread @ URL(gcc.gnu.org/bugzilla//show_bug.cgi?id=105523) (Accessed: September 26, 2023).

set DIPLOMAT_BOOTLOADER_BAUD_SIGNAL=1200

set DIPLOMAT_BOOTLOADER_COM=10
set DIPLOMAT_DIAGNOSTIC_COM=9
set DIPLOMAT_PROGRAMMER=avr109
set DIPLOMAT_MCU=ATmega32U4
set DIPLOMAT_AVR_GCC_ARGS= ^
	!AVR_GCC_FLAGS! -mmcu=!DIPLOMAT_MCU! ^
	-D BOOTLOADER_BAUD_SIGNAL=!DIPLOMAT_BOOTLOADER_BAUD_SIGNAL!

set NERD_DIAGNOSTIC_BAUD=250000
set NERD_BOOTLOADER_COM=11
set NERD_DIAGNOSTIC_COM=11

set NERD_PROGRAMMER=wiring
set NERD_MCU=ATmega2560
set NERD_AVR_GCC_ARGS= ^
	!AVR_GCC_FLAGS! -mmcu=!NERD_MCU!

if not exist W:\build\ (
	mkdir W:\build\
)

pushd W:\build\
	del *.s *.o *.hex > nul 2>&1

	REM
	REM Compile Microservices.c.
	REM

	if %COMPILE_MICROSERVICES% equ 1 (
		cl W:\src\Microservices.c /Fe:Microservices.exe !MSVC_FLAGS!
		if not !ERRORLEVEL! == 0 (
			goto ABORT
		)
	)

	REM
	REM Compile C source code into assembly and ELF.
	REM

	if %COMPILE_DIPLOMAT% equ 1 (
		avr-gcc !DIPLOMAT_AVR_GCC_ARGS! -S -fverbose-asm         W:\src\Diplomat.c
		avr-gcc !DIPLOMAT_AVR_GCC_ARGS! -o W:\build\Diplomat.elf W:\src\Diplomat.c
		if not !ERRORLEVEL! == 0 (
			goto ABORT
		)
	)

	if %COMPILE_NERD% equ 1 (
		avr-gcc !NERD_AVR_GCC_ARGS! -S -fverbose-asm     W:\src\Nerd.c
		avr-gcc !NERD_AVR_GCC_ARGS! -o W:\build\Nerd.elf W:\src\Nerd.c
		if not !ERRORLEVEL! == 0 (
			goto ABORT
		)
	)

	avr-size W:\build\Diplomat.elf W:\build\Nerd.elf

	REM
	REM Convert ELF into HEX.
	REM

	if %COMPILE_DIPLOMAT% equ 1 (
		avr-objcopy -O ihex -j .text -j .data Diplomat.elf Diplomat.hex
		if not !ERRORLEVEL! == 0 (
			goto ABORT
		)
	)

	if %COMPILE_NERD% equ 1 (
		avr-objcopy -O ihex -j .text -j .data Nerd.elf Nerd.hex
		if not !ERRORLEVEL! == 0 (
			goto ABORT
		)
	)

	REM
	REM Find Nerd's bootloader.
	REM

	if %COMPILE_NERD% equ 1 (
		mode | findstr "COM!NERD_BOOTLOADER_COM!:" > nul
		if !ERRORLEVEL! == 0 (
			set PUTTY_ARGS=-serial COM!NERD_DIAGNOSTIC_COM! -sercfg !NERD_DIAGNOSTIC_BAUD!,8,n,1,N

			avrdude -p !NERD_MCU! -c !NERD_PROGRAMMER! -V -P COM!NERD_BOOTLOADER_COM! -D -Uflash:w:Nerd.hex
			if not !ERRORLEVEL! == 0 (
				goto ABORT
			)

			goto OPEN_PUTTY
		)
	)

	REM
	REM Find Diplomat's bootloader.
	REM

	if %COMPILE_DIPLOMAT% equ 1 (
		mode | findstr "COM!DIPLOMAT_DIAGNOSTIC_COM!:" > nul
		if !ERRORLEVEL! == 0 (
			mode COM!DIPLOMAT_DIAGNOSTIC_COM!: BAUD=!DIPLOMAT_BOOTLOADER_BAUD_SIGNAL! > nul
		) else (
			mode | findstr "COM!DIPLOMAT_BOOTLOADER_COM!:" > nul
			if not !ERRORLEVEL! == 0 (
				echo No bootloader found.
				goto ABORT
			)
		)

		for /L %%n in (1,1,64) do (
			mode | findstr "COM!DIPLOMAT_BOOTLOADER_COM!:" > nul
			if !ERRORLEVEL! == 0 (
				set PUTTY_ARGS=-serial COM!DIPLOMAT_DIAGNOSTIC_COM!

				avrdude -p !DIPLOMAT_MCU! -c !DIPLOMAT_PROGRAMMER! -V -P COM!DIPLOMAT_BOOTLOADER_COM! -D -Uflash:w:Diplomat.hex
				if not !ERRORLEVEL! == 0 (
					goto ABORT
				)

				for /L %%n in (1,1,64) do (
					mode | findstr "COM!DIPLOMAT_DIAGNOSTIC_COM!:" > nul
					if !ERRORLEVEL! == 0 (
						goto OPEN_PUTTY
					) else (
						ping 127.0.0.1 -n 1 -w 500 > nul
					)
				)

				echo No diagnostic port found.
				goto ABORT
			) else (
				ping 127.0.0.1 -n 1 -w 500 > nul
			)
		)
	)

	echo No bootloader found.
	goto ABORT

	REM
	REM Open PuTTY!
	REM

	:OPEN_PUTTY
	if not "!PUTTY_ARGS!" == "" (
		start putty.exe -load "Default Settings" !PUTTY_ARGS!
	)

	del *.o > nul 2>&1
	:ABORT
popd W:\build\
