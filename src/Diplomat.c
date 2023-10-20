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
#include "sd.c"
#include "timer.c"
#include "Diplomat_usb.c"
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

/*
let data =
	Array
		.from(document.querySelectorAll("p.st03"))
		.map(x => [x.innerText, x.querySelector("span").innerText])
		.map(([x, y]) => [x.slice(x.indexOf('+') + 1), y.slice(y.indexOf("Get ") + 6)])
		.map(([x, y]) => [x.slice(0, x.indexOf(' ') - 1), y.slice(0, y.indexOf(" "))])
		.map(([x, y]) => [parseFloat(x), parseInt(y, 16)])
		.filter(([x, y]) => y % 512 == 0 && y)

console.table(data)
let reduction = data.reduce((acc, [x, y]) => [acc[0] + x, acc[1] + y], [0, 0])
console.log(`${reduction[0] / reduction[1] * 1000.0 * 1000.0}us/byte`)
*/

/* [Addendum: Optimizing Mass Storage].

	1. Before optimizations.
		86.19012262482745us/byte
		86.6420833083653us/byte

	2. By increasing SPI speed to 8MHz.
		17.830957711557726us/byte
		17.83118092951088us/byte

	3. By moving the SD reading into the interrupt routine itself.
		5.8796026155650285us/byte
		5.879522185220327us/byte

	4. Further simplications to the MS state machine.
		TODO

	5. Increasing endpoint size and double buffering.
		TODO

	6. Simplifying the FAT32 layout.
		TODO
```
let data =
	Array
		.from(document.querySelectorAll("p.st03"))
		.map(x => [x.innerText, x.querySelector("span").innerText])
		.map(([x, y]) => [x.slice(x.indexOf('+') + 1), y.slice(y.indexOf("Get ") + 6)])
		.map(([x, y]) => [x.slice(0, x.indexOf(' ') - 1), y.slice(0, y.indexOf(" "))])
		.map(([x, y]) => [parseFloat(x), parseInt(y, 16)])
		.filter(([x, y]) => y % 512 == 0 && y)

console.table(data)
let reduction = data.reduce((acc, [x, y]) => [acc[0] + x, acc[1] + y], [0, 0]);
`${reduction[0] / reduction[1] * 1000.0 * 1000.0}us/byte`
```
*/
