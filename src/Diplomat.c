/*
	Source(1) @ ATmega32U4 Datasheet ("Atmel-7766J-USB-ATmega16U4/32U4-Datasheet_04/2016").
*/

#define F_CPU 16'000'000
#include <avr/io.h>
#include <util/delay.h>

int
main(void)
{
	/* USB Initialization Process.

		We will be doing the following:
			[1] Power-On USB pads regulator.
			[2] Configure PLL interface.
			[3] Enable PLL.
			[4] Check PLL lock.
			[5] Enable USB interface.
			[6] Configure USB interface (USB speed, Endpoints configuration...).
			[7] Wait for USB VBUS information connection.

		"Power On the USB interface" @ Source(1) @ Section(21.12) @ Page(266).
	*/
	/* TODO(Tampered Default Register Values).
		- Does bootloader modify initial registers?
		- Is there a way to reset all registers, even if bootloader did mess with them?

		To my great dismay, some registers used in the initialization process are already tampered with.
		I strongly believe this is due to the fact that the Arduino Leonardo's bootloader, which uses USB,
		is not cleaning up after itself. This can be seen in the default bits in USBCON being different after
		the bootloader finishes running, but are the expected values after power-cycling.
		I don't know if there's a source online that could definitively confirm this however.
		The datasheet also shows a crude figure of a state machine going between the RESET and IDLE states,
		but doing USBE=0 doesn't seem to actually completely reset all register states, so this is useless.
		So for now, we are just gonna enforce that the board is power-cycled after each flash.

		- UHWCON, USBCON @ Source(1) @ Section(21.13.1) @ Page(267).
		- Crude USB State Machine @ Source(1) @ Figure (22-1) @ Page(270).
	*/

	if (USBCON != 0b0010'0000)
	{
		goto error;
	}

	/* [1] Power-On USB pads regulator.
		- UHWCON, UVREGE : Source(1) @ Section(21.13.1) @ Page(267).
	*/
	/* TODO(USB Regulator?).
		- Is "USB Pad Regulator" the same as "USB Regulator" in the figure?
		- Is UVREGE bit just for saving power?
		- What does the UVREGE do that couldn't have been done otherwise with USBE?

		- UHWCON, UVREGE @ Source(1) @ Section(21.13.1) @ Page(267).
		- "USB controller Block Diagram Overview" @ Source(1) @ Figure(21-1) @ Page(256).
	*/

	UHWCON = (1 << UVREGE);

	/* [2] Configure PLL interface and [3] Enable PLL.
		Since we are using the default 16MHz clock source as the input to the PLL,
		we will need to set PINDIV in PLLCSR to half the frequency to the expected 8MHz.
		After that, we then set the PLLE bit within the same register.

		It should also be noted that the default PLL output frequency is already 48MHz, the desired value for USB.
		This is seen in the default bits of PDIV3:0 within PLLFRQ register, and with the PLL frequency table.

		- PLLCSR, PINDIV, PLLE @ Source(1) @ Section(6.11.5) @ Page(40-41).
		- PLLFRQ, PDIV3:0, PLL Frequency Table @ Source(1) @ Section(6.11.6) @ Page(41-42).
	*/
	/* TODO(Optimal PLL Configuration?).
		The text at the bottom of the PLL frequency table says that the 96MHz configuration is the most "optimal" one.
		Not entirely too sure what that means (is it power consumption, accuracy, stability?).
		So it's probably fine to ignore this "optimal" configuration, but who knows!
	*/

	PLLCSR = (1 << PINDIV) | (1 << PLLE);

	/* [4] Check PLL lock.
		We just wait until the PLL stablizes and is "locked" onto the expected frequency,
		which may apparently take some milliseconds

		- PLLCSR, PLOCK @ Source(1) @ Section(6.11.5) @ Page(40-41).
	*/

	while (!(PLLCSR & (1 << PLOCK)));

	/* [5] Enable USB interface.
		The "USB interface" refers the entire USB device controller of the ATmega32U4, according to this sentence:
			> The USB device controller can at any time be reset by clearing USBE (disable USB interface).

		So that's what we'll do: set USBE in the USBCON register.

		But... based off of some experiments and another crude figure of the USB state machine,
		the FRZCLK bit can only be cleared **after** the USB interface has been enabled.
		In other words, when we're in the RESET state, FRZCLK will always be set, presumably for power-saving reasons.

		So we just have to set USBCON again afterwards, and while we're at it,
		we can set OTGPADE to be able to detect whether or not we are connected to a USB device/host.
		The connection can be determined by the VBUS bit in the USBSTA register.

		- "USB interface" @ Source(1) @ Section(22.2) @ Page(270).
		- Another Crude USB State Machine @ Source(1) @ Figure (21-7) @ Page(260).
		- USBCON, FRZCLK, OTGPADE @ Source(1) @ Section(21.13.1) @ Page(267).
		- VBUS, USBSTA @ Source(1) @ Section(21.13.1) @ Page(268).
	*/
	/* TODO(FRZCLK?)
		- What is FRZCLK?
		- Why does having FRZCLK make it so that Windows does not see the USB device (no warnings about missing device descriptor or anything)?
		Well, it "disable[s] the clock inputs" to the USB interface...
		Whatever the hell that means!

		- USBCON, FRZCLK @ Source(1) @ Section(21.13.1) @ Page(267).
	*/

	USBCON = (1 << USBE);
	USBCON = (1 << USBE) | (1 << OTGPADE);

	/* [6] Configure USB interface (USB speed, Endpoints configuration...).

		> The D+ or D- pull-up will be activated as soon as the DETACH bit is cleared and VBUS is present.

		The DETACH bit is in the UDCON register, so we'll clear it to connect the USB.
		There are other settings in the register too, but we don't care about them and
		they're all zeroed by default, so we can just set the entire register as zero.

		- D+/D- Activation @ Source(1) @ Section(22.2) @ Page(270).
		- UDCON, DETACH @ Source(1) @ Section(22.18.1) @ Page(281).
	*/

	UDCON = 0;

	/* [7] Wait for USB VBUS information connection.
		// TODO
	*/

	//
	//
	//

	DDRC = (1 << DDC7);
	for (;;)
	{
		if (USBSTA & (1 << VBUS))
		{
			PORTC ^= (1 << PORTC7);
			_delay_ms(1000.0);
		}
		else
		{
			PORTC ^= (1 << PORTC7);
			_delay_ms(250.0);
		}
	}

	error:;
	DDRC = (1 << DDC7);
	for (;;)
	{
		PORTC ^= (1 << PORTC7);
		_delay_ms(50.0);
	}
}
