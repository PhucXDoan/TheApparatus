#define F_CPU               16'000'000
#define PROGRAM_DIPLOMAT    true
#define BOARD_PRO_MICRO     true
#define PIN_DUMP_SS         2
#define PIN_SD_SS           3
#define PIN_USB_SPINLOCKING 4
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

	sei();
	spi_init();
	usb_init();
	u32 sector_count = sd_init();

	u8 zero_sector[FAT32_SECTOR_SIZE] = {0};

	for (u32 sector_address = sector_count - 1; sector_address; sector_address -= 1)
	{
		debug_tx_u64(sector_address);
		debug_tx_cstr("\n");
		sd_write(zero_sector, sector_address);
	}

	pin_output(HALT);
	for (;;)
	{
		pin_high(HALT);
		_delay_ms(1000.0);
		pin_low(HALT);
		_delay_ms(1000.0);
	}
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
