// Source(1) : ATmega32U4 Datasheet ("Atmel-7766J-USB-ATmega16U4/32U4-Datasheet_04/2016").

#define F_CPU 16'000'000
#include <avr/io.h>
#include <util/delay.h>

int
main(void)
{
	// Source(1) @ Section(21.12) @ Page(266).
	//
	// We will be doing the following:
	//
	// > Power On the USB interface"
	// >      [1.] Power-On USB pads regulator.
	// >      [2.] Configure PLL interface.
	// >      [3.] Enable PLL.
	// >      [4.] Check PLL lock.
	// >      [5.] Enable USB interface.
	// >      [6.] Configure USB interface (USB speed, Endpoints configuration...).
	// >      [7.] Wait for USB VBUS information connection.

	// [1.] Power-On USB pads regulator.
	//
	// Source(1) @ Section(21.13.1) @ Page(267).
	//     The datasheet doesn't say what UHWCON means here, but I'm guessing "USB Hardware Control".
	//     We set the UVREGE ("USB Pad Regulator Enable") bit of this register and that's that.
	//
	// TODO(USB Regulator?).
	//     I'm guessing that "USB Regulator" is the same as the "USB Pad Regulator",
	//     and based off of Source(1) @ Figure(21-1) @ Page(256),
	//     the regulator ties the USB capacitor and USB data lines to VCC.
	//     So I suppose that by not activating the regulator,
	//     we can reduce power usage if we wanted.

	UHWCON = (1 << UVREGE);

	// [2.] Configure PLL interface.
	// [3.] Enable PLL.
	//
	// Source(1) @ Section(6.11.5) @ Page(40-41).
	//     Since we are using the default 16MHz clock source as the input to the PLL,
	//     we will need to set PINDIV ("PLL Input Prescaler") in PLLCSR ("PLL Control and Status Register")
	//     to half the frequency to the expected 8MHz.
	//     After that, we then set the PLLE ("PLL Enable") bit within the same register.
	//
	// TODO(Separate Configuration/Enable Operation?).
	//     Since the USB initialization process lists the configuration
	//     and enabling of the PLL separately, it's what I'll be doing here too.
	//     I'd assume that there's no difference here at all,
	//     and that it'd only matter if the configuration was more complicated,
	//     but I certainly can't say with absolute confidence that combining
	//     bit-operations will always yield the same effect as when done separately.
	//
	// Source(1) @ Section(6.11.6) @ Page(41-42).
	//     It should also be noted that the default PLL output frequency is 48MHz,
	//     which is already the desired value for USB.
	//     This is seen in the default values of PDIV3:0 within PLLFRQ register and within the listed table.
	//
	// TODO(Optimal PLL Configuration?).
	//     The text at the bottom of the table of Source(1) @ Page(42) says that
	//     the 96MHz configuration is the most "optimal" one.
	//     Not entirely too sure what that means (is it power consumption, accuracy, stability?).
	//     So it's probably fine to ignore this "optimal" configuration, but who knows!

	PLLCSR  = (1 << PINDIV);
	PLLCSR |= (1 << PLLE);

	// [4.] Check PLL lock.
	//
	// Source(1) @ Section(6.11.5) @ Page(40-41).
	//
	// > PLL Control and Status Register - PLLCSR
	// >     PLOCK: PLL Lock Detector
	// >         When the PLOCK bit is set, the PLL is locked to the reference clock.
	// >         After the PLL is enabled, it takes about several ms for the PLL to lock.
	// >         To clear PLOCK, clear PLLE.
	//
	// So we just wait until the PLL stablizes and is "locked".

	while (!(PLLCSR & (1 << PLOCK)));

	// [5.] Enable USB interface.
	//
	// The "USB interface" refers the entire USB device controller of the ATmega32U4.
	// This is supported by the following sentence:
	//     Source(1) @ Section(22.2) @ Page(270).
	//         > The USB device controller can at any time be reset by clearing USBE (disable USB interface).
	//
	// Source(1) @ Section(21.13.1) @ Page(267).
	//     So we set the USBE ("USB Macro Enable Bit") of USBCON (probably "USB Control Register"?).

	USBCON = (1 << USBE);

	// [6.] Configure USB interface (USB speed, Endpoints configuration...).

	// TODO

	// [7.] Wait for USB VBUS information connection.

	// TODO

	//
	//
	//

	DDRC = (1 << DDC7);
	for (;;)
	{
		PORTC ^= (1 << PORTC7);
		_delay_ms(100.0);
		PORTC ^= (1 << PORTC7);
		_delay_ms(100.0);
	}
}
