static void
usb_init(void)
{ // [USB Initialization Process].

	if (USBCON != 0b0010'0000) // TODO[Tampered Default Register Values].
	{
		debug_halt(1);
	}

	// [Power-On USB Pads Regulator].
	UHWCON = (1 << UVREGE);

	// [Configure PLL Interface].
	PLLCSR = (1 << PINDIV) | (1 << PLLE);

	// [Check PLL Lock].
	while (!(PLLCSR & (1 << PLOCK)));

	// [Enable USB Interface].
	USBCON = (1 << USBE);
	USBCON = (1 << USBE) | (1 << OTGPADE);

	// [Configure USB Interface].
	UDIEN = (1 << EORSTE);

	// [Wait For USB VBUS Information Connection].
	UDCON = 0;
}

ISR(USB_GEN_vect)
{ // [USB Device Interrupt Routine].

	// [Endpoint Configuration].
	UENUM   = 0;
	UECONX  = (1 << EPEN);
	UECFG1X = (USB_ENDPOINT_0_SIZE_TYPE << EPSIZE0) | (1 << ALLOC);

	#if 0 // [Endpoint Allocation Check].
	if (!(UESTA0X & (1 << CFGOK)))
	{
		debug_halt(-1);
	}
	#endif

	// [Endpoint 0 Interrupts].
	UEIENX = (1 << RXSTPE);

	// [Clear UDINT].
	UDINT = 0;
}

ISR(USB_COM_vect)
{ // [Endpoint Interrupt Routine].

	UENUM   = 0;
	UECONX |= (1 << STALLRQC); // See: [Request Error Stall].

	// [Read SETUP Packet].
	struct USBSetupPacket setup_packet;
	for (u8 i = 0; i < sizeof(setup_packet); i += 1)
	{
		((u8*) &setup_packet)[i] = UEDATX;
	}

	// Let the interrupt routine know that we have copied the SETUP data. See: Source(1) @ Section(22.12) @ Page(274).
	UEINTX &= ~(1 << RXSTPI);

	if // [SETUP GetDescriptor].
	(
		setup_packet.unknown.bmRequestType == 0b1000'0000
		&& setup_packet.unknown.bRequest == USBSetupRequest_get_descriptor
	)
	{
		u8  data_remaining = 0;
		u8* data_cursor    = 0;

		if (!setup_packet.get_descriptor.language_id) // We're not going to handle different languages.
		{
			if (setup_packet.get_descriptor.descriptor_index) // TODO Does this ever realisitically get executed?
			{
				debug_halt(-1);
			}

			switch ((enum USBDescriptorType) setup_packet.get_descriptor.descriptor_type)
			{
				case USBDescriptorType_device:
				{
					data_cursor    = (u8*) &USB_DEVICE_DESCRIPTOR;
					data_remaining = sizeof(USB_DEVICE_DESCRIPTOR);
				} break;

				case USBDescriptorType_configuration:
				{
					data_cursor    = (u8*) &USB_CONFIGURATION_HIERARCHY;
					data_remaining = sizeof(USB_CONFIGURATION_HIERARCHY);
				} break;

				case USBDescriptorType_device_qualifier:
				{
					// ATmega32U4 does not support anything beyond full-speed, so we have to reply with a request error.
					// See: Source(2) @ Section(9.6.2) @ Page(264) & [Request Error Stall].
				} break;

				case USBDescriptorType_string:
				case USBDescriptorType_interface:
				case USBDescriptorType_endpoint:
				case USBDescriptorType_other_speed_configuration:
				case USBDescriptorType_interface_power:
				case USBDescriptorType_communication_interface:
				case USBDescriptorType_communication_endpoint:
				{
					debug_halt(3);
				} break;
			}
		}

		if (data_remaining) // [Transmit IN Data To Host].
		{
			if (data_remaining > setup_packet.get_descriptor.wLength)
			{
				data_remaining = setup_packet.get_descriptor.wLength;
			}

			while (true) // TODO[IN/OUT Interrupts].
			{
				if (UEINTX & (1 << TXINI))
				{
					u8 writes_left = SIZEOF_ENDPOINT_SIZE_TYPE(USB_ENDPOINT_0_SIZE_TYPE);
					if (writes_left > data_remaining)
					{
						writes_left = data_remaining;
					}

					data_remaining -= writes_left;
					while (writes_left)
					{
						UEDATX        = data_cursor[0];
						data_cursor  += 1;
						writes_left  -= 1;
					}

					UEINTX &= ~(1 << TXINI);
				}

				if (UEINTX & (1 << RXOUTI))
				{
					UEINTX &= ~(1 << RXOUTI);
					break;
				}
			}
		}
		else // [Request Error Stall].
		{
			UECONX |= (1 << STALLRQ);
		}
	}
	else if // [SETUP SetAddress].
	(
		setup_packet.unknown.bmRequestType == 0b0000'0000
		&& setup_packet.unknown.bRequest == USBSetupRequest_set_address
	)
	{
		if (setup_packet.set_address.address >= 0b0111'1111)
		{
			error_halt();
		}

		UDADDR = setup_packet.set_address.address;

		UEINTX &= ~(1 << TXINI);
		while (!(UEINTX & (1 << TXINI)));

		UDADDR |= (1 << ADDEN);
	}
	else if // [SETUP SetConfiguration].
	(
		setup_packet.unknown.bmRequestType == 0b0000'0000
		&& setup_packet.unknown.bRequest == USBSetupRequest_set_configuration
	)
	{
		debug_halt(4);
	}
	else // Unhandled or unknown SETUP command.
	{
		debug_u8(setup_packet.unknown.bmRequestType);
		debug_halt(2);
	}

	return;
}

//
// Internal Documentation.
//

/* [USB Initialization Process].
	We carry out the following steps specified by (1):
		1. [Power-On USB Pads Regulator].
		2. [Configure PLL Interface].
		3. [Enable PLL].
		4. [Check PLL Lock].
		5. [Enable USB Interface].
		6. [Configure USB Interface].
		7. [Wait For USB VBUS Information Connection].

	(1) "Power On the USB interface" @ Source(1) @ Section(21.12) @ Page(266).
*/

/* [Power-On USB Pads Regulator].
	Based off of (1), the USB (pad) regulator simply supplies voltage to the USB data
	lines and capacitor.

	So this first step is as simple as turning the UVREGE bit in UHWCON to enable this
	hardware (2).

	(1) "USB Controller Block Diagram Overview" @ Source(1) @ Figure(21-1) @ Page(256).
	(2) UHWCON, UVREGE @ Source(1) @ Section(21.13.1) @ Page(267).
*/

/* [Configure PLL Interface].
	PLL refers to a phase-locked-loop device. I don't know much about the mechanisms of
	it, but it seems like you give it some input frequency and it'll output a higher
	frequency after it has synced up onto it.

	I've heard that using the default 16MHz clock signal is more reliable (TODO Prove),
	especially when using full-speed USB connection. So that's what we'll be using.

	We don't actually use the 16MHz frequency directly, but put it through a PLL clock
	prescaler first, as seen in (1). The actual PLL unit itself expects 8MHz, so we'll
	need to half our 16MHz clock to get that.

	From there, we enable the PLL and it will output a 48MHz frequency by default, as
	shown in the default bits in (2) and table (3). We want 48MHz since this is what
	the USB interface expects as shown by (1).

	(1) "USB Controller Block Diagram Overview" @ Source(1) @ Figure(21-1) @ Page(256).
	(2) PDIV @ Source(1) @ Section(6.11.6) @ Page(41).
	(3) PDIV Table @ Source(1) @ Section(6.11.6) @ Page(42).
*/

/* [Check PLL Lock].
	We just wait until the PLL stablizes and is "locked" onto the expected frequency,
	which apparently can take some milliseconds according to (1).

	(1) PLOCK @ Source(1) @ Section(6.11.5) @ Page(41).
*/

/* [Enable USB Interface].
	The "USB interface" is synonymous with "USB device controller" or simply
	"USB controller" (1). We can enable the controller with the USBE bit in USBCON.

	But... based off of some experiments and the crude figure of the USB state machine
	at (2), the FRZCLK bit can only be cleared **AFTER** the USB interface has been
	enabled. FRZCLK halts the USB clock in order to minimize power consumption (3),
	but obviously the USB is also disabled, so we want to avoid that. As a result, we
	just have to assign to USBCON again afterwards to be able to clear the FRZCLK bit.

	We will also set OTGPADE to be able to detect whether or not we are connected to a
	USB device/host. This connection can be determined by the VBUS bit in the USBSTA
	register (4).

	(1) "USB interface" @ Source(1) @ Section(22.2) @ Page(270).
	(2) Crude USB State Machine @ Source(1) @ Figure (21-7) @ Page(260).
	(3) FRZCLK @ Source(1) @ Section(21.13.1) @ Page(267).
	(4) VBUS, USBSTA @ Source(1) @ Section(21.13.1) @ Page(268).
*/

/* [Configure USB Interface].
	We enable the End-of-Reset interrupt, so when the unenumerated USB device connects
	and the hosts sees this, the host will pull the data lines to a state to signal
	that the device should reset, and when the host releases, the
	[USB Device Interrupt Routine] is then triggered.

	It is important that we do some of our initializations after the End-of-Reset
	signal since (1) specifically states that the endpoints will end up disable at the
	end.

	(1) "USB Reset" @ Source(1) @ Section(22.4) @ Page(271).
*/

/* [Wait For USB VBUS Information Connection].
	We just allow the device to be able to attach to a host and we're done initializing
	sthe USB interface. No need to actually wait for a connection.
*/

/* [USB Device Interrupt Routine].
	Triggers listed on (1):
		[ ] VBUS Plug-In.
		[ ] Upstream Resume.
		[ ] End-of-Resume.
		[ ] Wake Up.
		[X] End-of-Reset.
		[ ] Start-of-Frame.
		[ ] Suspension Detection.
		[ ] Start-of-Frame CRC Error.

	(1) USB Device Controller Interrupt System @ Source(1) @ Figure(22-4) @ Page(279).
*/

/* [Endpoint Configuration].
	We consult the flowchart in (1) where we select our choice of endpoint to modify
	(dictated by UENUM (3)), enable it via EPEN in UECONX, apply the configurations
	such as size and banks, and then allocate.

	It should be noted that UECFG0X (2) describes the endpoint's
	USB transfer type (CONTROL, BULK, ISOCHRONOUS, INTERRUPT) (4).
	Since endpoint 0 is for setting up the USB device,
	it can only be the CONTROL type, and it is already so by default.

	UECFG0X also describes the endpoint's direction too (IN/OUT).
	The description for that configuration bit (EPDIR) seems to imply
	that CONTROL endpoints can only have an OUT direction.

	UECFG1X (2) has two more settings: endpoint size and endpoint banks.
	The endpoint sizes can go from 8 to 512 in powers of twos.
	According to the (5), endpoint 0 has a max size of 64 bytes.
	Any larger than this will probably cause an allocation failure.

	The other configuration in the UECFG1X register is the ability to pick between one
	or two banks. This is also called "ping-pong mode".
	Since endpoint 0 will be communicating from device to host and vice-versa
	(despite being considered an OUT by UECFG0X), we can't dual-bank (6) (7).

	(1) "Endpoint Activation" @ Source(1) @ Section(22.6) @ Page(271).
	(2) UECFG0X, UECFG1X @ Source(1) @ Section(22.18.2) @ Page(287-289).
	(3) "Endpoint Selection" @ Source(1) @ Section(22.5) @ Page(271).
	(4) USB Transfer Types @ Source(2) @ Section(5.4) @ Page(36-37).
	(5) USB Device Operating Modes Introduction @ Source(1) @ Section(22.1) @ Page(270).
	(6) Dual Bank Behavior for OUT Endpoints @ Source(1) @ Section(22.13.2.1) @ 276.
	(7) Dual Bank Behavior for IN Endpoints @ Source(1) @ Section(22.14.1) @ 276.
*/

/* [Endpoint Allocation Check].
	The allocation process can technically fail, as seen in (1),
	but if we aren't dynamically changing the endpoints around like in (2),
	then we can safely assume the allocation we do here is successful
	(we'd only have to check CFGOK in UESTA0X once and then recompile without
	the asserting code after (3)).

	(1) "Endpoint Activation" @ Source(1) @ Section(22.6) @ Page(271).
	(2) Allocation and Reorganization USB Memory Flow @ Source(1) @ Table(21-1) @ Page(264).
	(3) CFGOK, UESTA0X @ Source(1) @ Section(22.18.2) @ Page(287).
*/

/* [Endpoint 0 Interrupts].
	Once we have configured and enabled endpoint 0, we can activate the interrupt
	that'll trigger when we have received some setup commands from the host.
	This is done with RXSTPE ("Received Setup Enable") bit in the UEIENX register (1).

	Note that this interrupt is only going to trigger for endpoint 0 (2).
	So if the host sends a setup command to endpoint 3, it'll be ignored
	(unless the interrupt for that is also enabled).

	[Endpoint Interrupt Routine] handles the interrupts of endpoints.

	(1) UEIENX, RXSTPE @ Source(1) @ Section(22.18.2) @ Page(290-291).
	(2) "USB Device Controller Endpoint Interrupt System" @ Source(1) @ Figure(22-5) @ Page(280).
*/

/* [Clear UDINT].
	The UDINT register has flags that are triggering this interrupt routine,
	so we'll need to clear them to make sure we don't accidentally go in an infinite
	loop!

	The only flag-bit we'd need to actually clear is the EORSTI bit,
	but since we don't care about the other bits and they don't do anything when cleared,
	we can just go ahead set the entire register to zero (1).

	(1) UDINT, EORSTI @ Source(1) @ Section(22.18.1) @ Page(282).
*/

/* [Endpoint Interrupt Routine].
	Triggers listed on (1) of when a specific endpoint:
		[ ] is ready to supply data for IN packets to send to the host.
		[ ] received OUT data from the host.
		[X] received a SETUP command.
		[ ] has a stalled packet.
		[ ] has a CRC mismatch on OUT data (isochronous mode).
		[ ] has an overflow/underflow (isochronous mode).
		[ ] has a NAK on IN/OUT.

	(1) "Endpoint Interrupt" @ Source(1) @ Figure(22-5) @ Page(280).
*/

/* [Read SETUP Packet].
	The host first sends the device a TOKEN that signifies a SETUP command.
	After that token is a DATA packet consisting of 8 bytes,
	the layout of which is detailed by {struct USBSetupPacket}.
	After this data packet is a handshake (also called the status stage)
	given to the host that the device acknowledged the transaction,
	which according to (1), is always done automatically for CONTROL typed endpoints
	to SETUP commands.

	We can read the data sent by the host byte-by-byte from UEDATX.
	The amount of bytes within the buffer is UEBCX (2), but since SETUP data packets
	are already 8 bytes always, we can assume that it is so. If it isn't,
	then there's a very unlikely chance that there'll be a successful enumeration;
	no need to waste program memory here.

	(1) CONTROL Endpoint Management @ Source(1) @ Section(22.12) @ Page(274).
	(2) UEDATX, UEBCX @ Source(1) @ Section(22.18.2) @ Page(291).

	* "Standard Device Requests" @ Source(2) @ Table(9-3) @ Page(250).
*/

/* [SETUP GetDescriptor].
	This is where the host wants to learn more about the device.
	The host asks about various things such as information about device itself,
	configurations, strings, etc, all of which are noted in {enum USBDescriptorType}.
*/

/* [Transmit IN Data To Host].
	The USB transaction of data from device-to-host begins after the host has already
	sent the SETUP command (first a TOKEN, then the {struct USBSetupPacket} as the
	8-byte data packet, finally handshake of ACK). The host now begins to send a series
	of IN commands to get data from the device.

	The transaction, like with any other, follows the same format: TOKEN, data packet,
	and finally handshake. The TOKEN is sent by host (since USB is host-centric) with a
	bit-pattern that signifies an IN command, to which the data-packet that follows it
	is now being transmitted by the device (rather than from the host like last time
	with the SETUP data-packet). After the data-packet has been sent, the host is now
	the one to send the ACK handshake.

	Let's say for endpoint 0 that it has a buffer size of 8 bytes.
	This will mean that data requested by the host longer than 8 bytes cannot be sent
	entirely through endpoint 0. This is resolved by having the host perform multiple
	of these transactions involving the IN commands until the transfer is complete.
	Data-packets should always be as long as the endpoint's buffer can make it to be,
	until there is not enough remaining data to send, to which the data-packet can be
	truncated to a shorter length (e.g. 3 bytes instead of max 8 bytes). In other words,
	no padding bytes are to be sent.

	After the series of IN commands, the host sends the device an OUT command.
	This transaction is simply a longer version of the ACK handshake though,
	since the OUT data-packet that is sent from host to device is zero-lengthed.
	This is just to let the device know that the host has received all the data.

	An OUT command could also be sent prematurely by the host, even if the device still
	has remaining data to send. Perhaps the host had enough of the device!
*/

/* [SETUP SetAddress]
	The host would like to set the device's address, which at this point is zero by
	default, as with any other USB device when it first connect. The desired address
	is stated within the SETUP data-packet that the host sent to endpoint 0,
	specifically in a 16-bit field.

	According to (1), any address is a 7-bit unsigned integer, so if we received a
	desired address from the host that can't fit into that, then something has gone
	quite wrong.

	Once we have the desired address, we set it as so in the UDADDR register, but don't
	actually enable it until later, as stated in (2).

	So the host sent a SETUP command asking to set the device's address, which we ACK'd
	and processed. After that SETUP command is an IN command (still to address 0) to
	make sure the device has processed it. We can signal to the host that we have indeed
	so by sending a zero-lengthed data-packet in response.

	The ATmega32U4 datasheet makes no mention of this at all, but it seems like we can
	only enable the address **AFTER** we have completely sent the zero-lengthed
	data-packet. So we just simply spinlock until endpoint 0's buffer is free.

	(1) "Address Field" @ Source(2) @ Section(8.3.2.1) @ Page(197).
	(2) "Address Setup" @ Source(1) @ Section(22.7) @ Page(272).
*/

/* [Request Error Stall].
	The host might request things from the device such as a descriptor, but the device
	might not have this information. In this case, we are to reply to the host with a
	STALL packet (1).

	All requests are made to control endpoints in the form of SETUP data-packets,
	often just endpoint 0 (3). The ATmega32U4 will always ACK the first stage of the
	transfer where the host sends this SETUP data-packet. After that, the host will
	send a series of IN commands to grab the device's response data. If the data
	requested by the host specified in the SETUP data-packet cannot be retrieved by the
	device, we send a STALL response to the IN command (or whatever the next command
	the host sends us after the SETUP packet) (1).

	The control endpoint will continue to respond with STALL (unless it is a SETUP
	command, to which ATmega32U4 will ACK like always) until we receive another SETUP
	command, to which we then reset to a non-stall state.

	(1) "Request Error" @ Source(2) @ Section(9.2.7) @ Page(247).
	(2) "STALL Request" @ Source(1) @ Section(22.11) @ Page(273-274).
	(3) "USB Device Requests" @ Source(2) @ Section(9.3) @ Page(248).
	(4) STALL Handshakes @ Source(2) @ Section(8.5.3.4) @ Page(228).
*/

/* [SETUP SetConfiguration].
	TODO
*/

/* TODO[Tampered Default Register Values].
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
	See: UHWCON, USBCON @ Source(1) @ Section(21.13.1) @ Page(267).
	See: Crude USB State Machine @ Source(1) @ Figure (22-1) @ Page(270).
*/

/* TODO[USB Regulator vs Interface]
	There's a different bit for enabling the USB pad regulator and USB interface,
	but why are these separate? Perhaps for saving state?
*/

/* TODO Endpoint 0 Size?
	How does the size affect endpoint 0?
	Could we just be cheap and have 8 bytes?
*/

/* TODO 512 Byte Endpoint?
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
*/

/* TODO[IN/OUT Interrupts].
	In theory, using interrupts to trigger on IN/OUT commands to handle should be
	identical to spinlocking for those commands, but it turns out not. I'm not entirely
	too sure why. Perhaps I am not understanding the exact semantic of the endpoint
	interrupts for these cases, or that the CPU can't handle the interrupt in time
	because USB is just so fast. It could also very well be neither of those cases and
	it's something else entirely!
*/
