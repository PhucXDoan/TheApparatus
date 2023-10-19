#define F_CPU               16'000'000
#define PROGRAM_DIPLOMAT    true
#define BOARD_LEONARDO      true
#define PIN_DUMP_SS         0
#define PIN_USB_SPINLOCKING 0
#define PIN_U16_CLK         4
#define PIN_U16_DATA        5
#define PIN_SD_SS           7
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "misc.c"
#include "pin.c"
#include "spi.c"
#include "Diplomat_usb.c"
#include "sd.c"
#include "timer.c"
#undef  PIN_HALT_SOURCE
#define PIN_HALT_SOURCE HaltSource_diplomat

int
main(void)
{
	if (USBCON != 0b0010'0000) // [Bootloader-Tampered State].
	{
		wdt_enable(WDTO_15MS);
		for(;;);
	}

	debug_u16(0);

	sei();
	spi_init();
	sd_init();
	usb_init();
	timer_init();

	#define PROFILER_MAX_SECTORS 8

	u32 iterations = 0;
	u32 mismatches = 0;
	for (;;)
	{
		debug_tx_cstr("Iteration (");
		debug_tx_u64(iterations);
		debug_tx_cstr(") | Mismatches (");
		debug_tx_u64(mismatches);
		debug_tx_cstr(")\n");
		u32 beginning_ms = timer_ms();

		u8 sector_data[FAT32_SECTOR_SIZE] = {0};
		for (u32 abs_sector_address = 0; abs_sector_address < PROFILER_MAX_SECTORS; abs_sector_address += 1)
		{
			sd_write(sector_data, abs_sector_address);
		}

		u64 checksum = 0;
		u64 temp     = 134;
		for (u32 abs_sector_address = 0; abs_sector_address < PROFILER_MAX_SECTORS; abs_sector_address += 1)
		{
			sd_read(sector_data, abs_sector_address);
			for (u16 i = 0; i < countof(sector_data); i += 1)
			{
				temp           *= sector_data[(i - 1) & (countof(sector_data) - 1)];
				temp           += abs_sector_address + i;
				sector_data[i]  = temp;
				temp            = (temp << 8) | ((temp >> 8) & 0xFF);
			}
			sd_write(sector_data, abs_sector_address);
		}

		for (u32 abs_sector_address = 0; abs_sector_address < PROFILER_MAX_SECTORS; abs_sector_address += 1)
		{
			sd_read(sector_data, abs_sector_address);
			for (u16 i = 0; i < countof(sector_data); i += 1)
			{
				checksum += u64(sector_data[i]) << (i % 64);
			}
		}

		u64 ending_ms = timer_ms();
		debug_tx_cstr("\t0x");
		debug_tx_64H(checksum);
		debug_tx_cstr("\n\t");
		debug_tx_u64(ending_ms - beginning_ms);
		debug_tx_cstr("ms\n");

		if (checksum != 0xA7FE'F9FF'1630'19BE)
		{
			mismatches += 1;
			debug_tx_cstr("CHECKSUM MISMATCH.\n");
		}
		iterations += 1;
	}

	for(;;);
}

//
// Documentation.
//

/* [Bootloader-Tampered State].
	For the ATmega32U4 that I have, the bootloader is executed whenever the MCU reinitializes from
	an external reset (RST was pulled low); other sources of resets do not. When the bootloader
	hands execution to the main program, it seems to leaves the USB-related registers (e.g. USBCON)
	not with their default values. This is pretty obviously inconvenient for us to set up USB
	correctly, and who knows what other registers are also tampered with by the bootloader!

	To resolve this, we just simply enable the watchdog timer and wait for it to trigger. This
	induces a hardware reset that won't have the MCU start with the bootloader messing around with
	our registers. AVR-GCC pretty much handles all for us, so don't have to worry about the minute
	details like the timed-writing sequences.

	See: "Watchdog Reset" @ Source(1) @ Section(8.6) @ Page(53-54).
	See: "Watchdog Timer" @ Source(1) @ Section(8.9) @ Page(55-56).
*/
