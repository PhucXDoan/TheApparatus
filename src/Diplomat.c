#define F_CPU 16'000'000
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "defs.h"
#include "pins.c"

/* "USB Device Interrupt".
	Can trigger on:
		- VBUS Plug-In.
		- Upstream Resume.
		- End-of-Resume.
		- Wake Up.
		- End-of-Reset.
		- Start-of-Frame.
		- Suspension Detection.
		- Start-of-Frame CRC Error.

	Since only the End-of-Reset event is enabled, that's what this
	interrupt will be only triggered for.

	See: [6] Configure USB interface (USB speed, Endpoints configuration...).
	* "USB Device Interrupt" @ Source(1) @ Figure(22-4) @ Page(279).
	* Interrupt Bits @ Source(1) @ Section(22.18.1) @ Page(283).
*/
ISR(USB_GEN_vect)
{
	/* Endpoint Configuration.
		This is where we'll initialize the endpoints.
		We consult the flowchart in "Endpoint Activation" where we
		select our choice of endpoint to modify (dictated by UENUM),
		enable it via EPEN in UECONX,
		apply the configurations such as size and banks,
		and then allocate.

		It should be noted that UECFG0X describes the endpoint's
		USB transfer type (control, bulk, isochronous, interrupt).
		Since endpoint 0 is for setting up the USB device,
		it makes sense to make it have the control type,
		and it is already so by default.
		UECFG0X also describes the endpoint's direction too (IN/OUT),
		but the description for that configuration bit (EPDIR) seems to imply
		that control endpoints can only have an OUT direction.

		UECFG1X has two more settings: endpoint size and endpoint banks.
		The endpoint sizes can go from 8 to 512 in powers of twos.
		According to the introduction of the "USB Device Operating Modes" section,
		endpoint 0 has a max size of 64 bytes.
		Any larger than this will probably cause an allocation failure.
		The other configuration in the UECFG1X register is the
		ability to pick between one or two banks.
		Since endpoint 0 will be communicating from device to host and vice-versa
		(despite being considered an OUT by UECFG0X), we can't dual-bank.

		TODO Endpoint 0 Size?
			How does the size affect endpoint 0?
			Could we just be cheap and have 8 bytes?

		TODO 512 Byte Endpoint?
			The datasheet claims that an endpoint size can be 512 bytes,
			but makes no other mention to this at all. The introduction
			states that endpoint 1 can have a "FIFO up to 256 bytes in ping-pong mode",
			while the others have it up to 64 bytes. So how could an endpoint ever get to 512 bytes?
			Maybe it's saying that for endpoint 1 that one bank is 256 bytes, and
			in ping-pong mode, it's another 256 bytes, and 512 bytes total,
			but this still doesn't really make sense as to why we can
			configure an endpoint in UECFG1X to be have 512 bytes!
			* UECFG1X @ Source(1) @ Section(22.18.2) @ Page(287).
			* Endpoint Size Statements @ Source(1) @ Section(22.1) @ Page(270).

		* "Endpoint Selection" @ Source(1) @ Section(22.5) @ Page(271).
		* "Endpoint Activation" @ Source(1) @ Section(22.6) @ Page(271).
		* EPEN, UENUM, EPSIZEx, UECONX, UECFG0X, EPDIR, UECFG1X, ALLOC @ Source(1) @ Section(22.18.2) @ Page(285-287).
		* USB Transfer Types @ Source(2) @ Section(5.4) @ Page(36-37).
		* USB Device Operating Modes Introduction @ Source(1) @ Section(22.1) @ Page(270).
		* Dual Bank Behavior for OUT Endpoints @ Source(1) @ Section(22.13.2.1) @ 276.
		* Dual Bank Behavior for IN Endpoints @ Source(1) @ Section(22.14.1) @ 276.
	*/

	UENUM   = 0;
	UECONX  = (1 << EPEN);
	UECFG1X = (USB_ENDPOINT_0_SIZE << EPSIZE0) | (1 << ALLOC);

	/* Endpoint Allocation Check.
		The allocation process can technically fail,
		as seen in the endpoint activation flowchart,
		but if we aren't dynamically changing the endpoints around,
		then we can safely assume the allocation we do here is successful
		(we'd only have to check CFGOK in UESTA0X once and then remove the asserting code after).

		* "Endpoint Activation" @ Source(1) @ Section(22.6) @ Page(271).
		* CFGOK, UESTA0X @ Source(1) @ Section(22.18.2) @ Page(287).
	*/

	/* Enabling SETUP Interrupt for Endpoint 0.
		Once we have enabled endpoint 0, we can enable the interrupt
		that'll trigger when we have received some setup commands from the host.
		This is done with RXSTPE ("Received Setup Enable") bit in the UEIENX register.

		Note that this interrupt is only going to trigger for endpoint 0.
		So if the host sends a setup command to endpoint 3, it'll be ignored
		(unless the interrupt for that is enabled).

		* UEIENX, RXSTPE @ Source(1) @ Section(22.18.2) @ Page(290-291).
		* "USB Device Controller Endpoint Interrupt System" @ Source(1) @ Figure(22-5) @ Page(280).
	*/

	UEIENX = (1 << RXSTPE);

	/* Clear Interrupt Flags.
		The UDINT register has flags that are triggering this interrupt routine,
		so we'll need to clear them to make sure we don't accidentally go in an infinite loop!

		The only flag-bit we'd need to actually clear is the EORSTI bit,
		but since we don't care about the other bits and they don't do anything when cleared,
		we can just go ahead set the entire register to zero.

		* UDINT, EORSTI @ Source(1) @ Section(22.18.1) @ Page(282).
	*/

	UDINT = 0;
}

/* "Endpoint Interrupt".
	Can trigger when there's an endpoint that:
		- is ready to accept data.
		- received data.
		- received setup command.
		- has stalled packet.
		- has CRC mismatch on OUT data (isochronous mode).
		- has overflow/underflow (isochronous mode).
		- has NAK on IN/OUT.

	What will actually trigger it:
		When a setup command is sent to endpoint 0 (UENUM = 0, RXSTPI in UEINTX).

	See: ISR(USB_GEN_vect).
	* "Endpoint Interrupt" @ Source(1) @ Figure(22-5) @ Page(280).
	* UENUM, RXSTPI, UEINTX @ Source(1) @ Section(22.18.2) @ Page(285-290).
*/
ISR(USB_COM_vect)
{
	/* Handle Interrupt Types.
		There are multitude of sources that could trigger interrupt routine,
		each applied to each enabled endpoint.
		The source can be found in the bits of UEINTX
		(which is specific to the currently selected endpoint dictated by UENUM).

		At the end, we'll need to clear UEINTX register or
		else the interrupt routine might get triggered again.
		Some of the bits perform a specific task when cleared (like with FIFOCON),
		some require it to be cleared (NAKINI), others don't do anything (KILLBK).

		* Sources of Interrupt Triggers @ Source(1) @ Figure(22-5) @ Page(280).
		* UENUM, UEINTX, FIFOCON, NAKINI, KILLBK @ Source(1) @ Section(22.18.2) @ Page(285-290).
	*/

	UENUM = 0;

	//
	// Read setup packet.
	//
	// * "USB Device Requests" @ Source(2) @ Section(9.3) @ Page(248).
	//

	struct USBSetupPacket setup_packet;
	for (u8 i = 0; i < sizeof(setup_packet); i += 1)
	{
		((u8*) &setup_packet)[i] = UEDATX;
	}

	//
	// Determine setup packet's purpose.
	//
	// * "Standard Device Requests" @ Source(2) @ Table(9-3) @ Page(250).
	//

	if
	(
		setup_packet.unknown.bmRequestType == 0b1000'0000
		&& setup_packet.unknown.bRequest == USBStandardRequestCode_get_descriptor
	)
	{
		switch (setup_packet.get_descriptor.descriptor_type)
		{
			case DescriptorType_device:
			{
				static int i = 0; // TEMP
				pin_output(++i); // TEMP
			} break;

			case DescriptorType_configuration:
			case DescriptorType_string:
			case DescriptorType_interface:
			case DescriptorType_endpoint:
			case DescriptorType_device_qualifier:
			case DescriptorType_other_speed_configuration:
			case DescriptorType_interface_power:
			default:
			{
				goto error;
			} break;
		}
	}
	else
	{
		goto error;
	}

	UEINTX = 0;
	return;

	error:; // TODO have a better looking error here...
	DDRC = (1 << DDC7);
	for (;;)
	{
		pin_set(13, !pin_read(13));
		_delay_ms(50.0);
	}
}

int
main(void)
{
	sei(); // Activates global interrupts. @ Source(1) @ Section(4.4) @ Page(11).

	/* USB Initialization Process.
		We will be doing the following:
			[1] Power-On USB pads regulator.
			[2] Configure PLL interface.
			[3] Enable PLL.
			[4] Check PLL lock.
			[5] Enable USB interface.
			[6] Configure USB interface (USB speed, Endpoints configuration...).
			[7] Wait for USB VBUS information connection.

		TODO Tampered Default Register Values.
			- Does bootloader modify initial registers?
			- Is there a way to reset all registers, even if bootloader did mess with them?
			- Perhaps USB End Of Reset event is sufficient to reset the USB state?
			To my great dismay, some registers used in the initialization process are already tampered with.
			I strongly believe this is due to the fact that the Arduino Leonardo's bootloader, which uses USB,
			is not cleaning up after itself. This can be seen in the default bits in USBCON being different after
			the bootloader finishes running, but are the expected values after power-cycling.
			I don't know if there's a source online that could definitively confirm this however.
			The datasheet also shows a crude figure of a state machine going between the RESET and IDLE states,
			but doing USBE=0 doesn't seem to actually completely reset all register states, so this is useless.
			So for now, we are just gonna enforce that the board is power-cycled after each flash.
			* UHWCON, USBCON @ Source(1) @ Section(21.13.1) @ Page(267).
			* Crude USB State Machine @ Source(1) @ Figure (22-1) @ Page(270).

		* "Power On the USB interface" @ Source(1) @ Section(21.12) @ Page(266).
	*/

	if (USBCON != 0b0010'0000)
	{
		goto error;
	}

	/* [1] Power-On USB pads regulator.
		We turn it on!

		TODO USB Regulator?
			- Is "USB Pad Regulator" the same as "USB Regulator" in the figure?
			- Is UVREGE bit just for saving power?
			- What does the UVREGE do that couldn't have been done otherwise with USBE?
			* UHWCON, UVREGE @ Source(1) @ Section(21.13.1) @ Page(267).
			* "USB controller Block Diagram Overview" @ Source(1) @ Figure(21-1) @ Page(256).

		* UHWCON, UVREGE : Source(1) @ Section(21.13.1) @ Page(267).
	*/

	UHWCON = (1 << UVREGE);

	/* [2] Configure PLL interface and [3] Enable PLL.
		Since we are using the default 16MHz clock source as the input to the PLL,
		we will need to set PINDIV in PLLCSR to half the frequency to the expected 8MHz.
		After that, we then set the PLLE bit within the same register.

		It should also be noted that the default PLL output frequency is already 48MHz, the desired value for USB.
		This is seen in the default bits of PDIV3:0 within PLLFRQ register, and with the PLL frequency table.

		TODO Optimal PLL Configuration?
			The text at the bottom of the PLL frequency table says that the 96MHz configuration is the most "optimal" one.
			Not entirely too sure what that means (is it power consumption, accuracy, stability?).
			So it's probably fine to ignore this "optimal" configuration, but who knows!

		* PLLCSR, PINDIV, PLLE @ Source(1) @ Section(6.11.5) @ Page(40-41).
		* PLLFRQ, PDIV3:0, PLL Frequency Table @ Source(1) @ Section(6.11.6) @ Page(41-42).
	*/

	PLLCSR = (1 << PINDIV) | (1 << PLLE);

	/* [4] Check PLL lock.
		We just wait until the PLL stablizes and is "locked" onto the expected frequency,
		which may apparently take some milliseconds

		* PLLCSR, PLOCK @ Source(1) @ Section(6.11.5) @ Page(40-41).
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

		TODO USB Pad?
			- Is USB pad a part of the USB interface?

		TODO FRZCLK?
			- What is FRZCLK?
			- Why does having FRZCLK make it so that Windows does not see the USB device (no warnings about missing device descriptor or anything)?
			Well, it "disable[s] the clock inputs" to the USB interface...
			Whatever the hell that means!
			* USBCON, FRZCLK @ Source(1) @ Section(21.13.1) @ Page(267).

		* "USB interface" @ Source(1) @ Section(22.2) @ Page(270).
		* Another Crude USB State Machine @ Source(1) @ Figure (21-7) @ Page(260).
		* USBCON, FRZCLK, OTGPADE @ Source(1) @ Section(21.13.1) @ Page(267).
		* VBUS, USBSTA @ Source(1) @ Section(21.13.1) @ Page(268).
	*/

	USBCON = (1 << USBE);
	USBCON = (1 << USBE) | (1 << OTGPADE);

	/* [6] Configure USB interface (USB speed, Endpoints configuration...).
		The LSM ("Low Speed Mode") bit in UDCON is by default set to where the
		USB controller is in full-speed mode, which is probably what we want anyways.

		The endpoint configurations seems to be only configurable after a USB End-of-Reset event has occured,
		according to the following text:
			> When [a] USB reset is detected on the USB line (SE0 state with a minimum duration of 2.5us),
			> the next operations are performed by the controller:
			>     - all the endpoints are disabled
			>     - the default control endpoint remains configured (see "Endpoint Reset" on page 270 for more details)

		So what we can do here instead is enable an interrupt that'll trigger on the End-of-Reset event
		and initialize within that interrupt routine;
		this is be done with the UDIEN (I'm guessing "USB Device Interrupt Enables") register
		holding the EORSTE ("End-of-Reset Enable") bit.

		See: ISR(USB_GEN_vect).
		* LSM, UDCON @ Source(1) @ Section(22.18.1) @ Page(281).
		* "USB Reset" @ Source(1) @ Section(22.4) @ Page(271).
		* UDIEN, EORSTE @ Source(1) @ Section(22.18.1) @ Page(283).
	*/

	UDIEN = (1 << EORSTE);

	/* [7] Wait for USB VBUS information connection.
		We can just try to connect immediately, regardless of the VBUS state.
		This can be done by clearing the DETACH bit in the UDCON register.

		* D+/D- Activation @ Source(1) @ Section(22.2) @ Page(270).
		* UDCON, DETACH @ Source(1) @ Section(22.18.1) @ Page(281).
	*/

	UDCON = 0;

	//
	//
	//

	DDRC   = (1 << DDC7);
	PORTC &= ~(1 << PORTC7);
	_delay_ms(1000.0);

	for (;;)
	{
		PORTC ^= (1 << PORTC7);
		_delay_ms(1000.0);
	}

	error:;
	DDRC = (1 << DDC7);
	for (;;)
	{
		pin_set(13, !pin_read(13));
		_delay_ms(50.0);
	}
}
