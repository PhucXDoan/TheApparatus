static void
usb_init(void)
{ // [USB Initialization Process].

	if (USBCON != 0b0010'0000) // TODO[Tampered Default Register Values].
	{
		wdt_enable(WDTO_15MS);
		for (;;);
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

	// [Configure USB Interface] TODO Update.
	UDIEN = (1 << EORSTE) | (1 << SOFI); // TODO FNCERR ?

	// [Wait For USB VBUS Information Connection].
	UDCON = 0;
}

ISR(USB_GEN_vect)
{ // [USB Device Interrupt Routine, Endpoint 0 Configuration, and Endpoint Interrupts] TODO Update.

	UENUM = 0; // "Select the endpoint".
	{
		// "Activate endpoint".
		UECONX = (1 << EPEN);

		// "Configure" and "Allocate".
		UECFG1X = (USB_ENDPOINT_DFLT_SIZE << EPSIZE0) | (1 << ALLOC);

		UEIENX = (1 << RXSTPE); // Endpoint 0 will listen to the reception of SETUP-transactions.

		// "Test endpoint configuration".
		#if 1 // TODO disable
		if (!(UESTA0X & (1 << CFGOK)))
		{
			debug_halt(3);
		}
		#endif
	}

	static u8 TEMP_ASD = 0;
	static char TEMPY_ASD = ' ';

	if (UDINT & (1 << SOFI))
	{
		UENUM = USB_ENDPOINT_CDC_IN;
		if ((UESTA0X & (1 << CFGOK)) && (UEINTX & (1 << TXINI)))
		{
			if (!TEMP_ASD)
			{
				char message[] = "The work is mysterious and important.\n";
				for (u8 i = 0; i < countof(message) - 1; i += 1)
				{
					UEDATX = message[i];
				}

				UEDATX = TEMPY_ASD;
			}

			TEMP_ASD -= 1;

			UEINTX &= ~(1 << TXINI);
			UEINTX &= ~(1 << FIFOCON);
		}

		UENUM = USB_ENDPOINT_CDC_OUT;
		if ((UESTA0X & (1 << CFGOK)) && (UEINTX & (1 << RXOUTI)))
		{
			TEMP_ASD = 0;
			TEMPY_ASD = UEDATX;
			UEINTX &= ~(1 << RXOUTI);
			UEINTX &= ~(1 << FIFOCON);
		}
	}

	UDINT = 0; // Clear End-of-Reset trigger flag to prevent this routine from executing again. // TODO Update
}

static void
_usb_endpoint_0_transmit(u8* packet_data, u16 packet_length) // [Endpoint 0: Data-Transfer from Device to Host].
{
	u8* data_cursor    = packet_data;
	u16 data_remaining = packet_length;
	while (true)
	{
		if (UEINTX & (1 << RXOUTI)) // Received OUT-transaction?
		{
			UEINTX &= ~(1 << RXOUTI); // Tell host that we're ready for the next command now. See: "Control Transfer > Status Stage > IN" @ Source(4) @ Chapter(4).
			break;
		}
		else if (UEINTX & (1 << TXINI)) // Buffer ready to be filled?
		{
			u8 writes_left =
				data_remaining < USB_ENDPOINT_DFLT_SIZE
					? data_remaining
					: USB_ENDPOINT_DFLT_SIZE;

			data_remaining -= writes_left;

			while (writes_left)
			{
				UEDATX        = data_cursor[0];
				data_cursor  += 1;
				writes_left  -= 1;
			}

			UEINTX &= ~(1 << TXINI); // Mark buffer as ready to be sent for IN-transaction.
		}
	}
}

ISR(USB_COM_vect)
{ // See: [USB Device Interrupt Routine, Endpoint 0 Configuration, and Endpoint Interrupts].

	UENUM = 0;
	if (UEINTX & (1 << RXSTPI)) // [Endpoint 0: SETUP-Transactions].
	{
		UECONX |= (1 << STALLRQC); // SETUP-transactions lift STALL conditions. See: Source(2) @ Section(8.5.3.4) @ Page(228) & [Endpoint 0: Request Error].

		struct USBSetupRequest request = {0};
		for (u8 i = 0; i < sizeof(request); i += 1)
		{
			((u8*) &request)[i] = UEDATX;
		}
		UEINTX &= ~(1 << RXSTPI); // Clear the endpoint bank. See: Source(1) @ Section(22.12) @ Page(274).

		switch (request.type)
		{
			case USBSetupRequestType_get_descriptor: // [Endpoint 0: SETUP-Transaction's GetDescriptor].
			{
				u8  payload_length = 0; // Any payload we send will not exceed 255 bytes.
				u8* payload_data   = 0;

				switch ((enum USBDescType) request.get_descriptor.descriptor_type)
				{
					case USBDescType_device: // See: [Endpoint 0: SETUP-Transaction's GetDescriptor].
					{
						payload_data   = (u8*) &USB_DEVICE_DESCRIPTOR;
						payload_length = sizeof(USB_DEVICE_DESCRIPTOR);
						static_assert(sizeof(USB_DEVICE_DESCRIPTOR) < (((u64) 1) << (sizeof(payload_length) * 8)))
					} break;

					case USBDescType_config: // [Interfaces, Configurations, and Classes].
					{
						if (!request.get_descriptor.descriptor_index) // We only have a single configuration.
						{
							payload_data   = (u8*) &USB_CONFIGURATION_HIERARCHY;
							payload_length = sizeof(USB_CONFIGURATION_HIERARCHY);
							static_assert(sizeof(USB_CONFIGURATION_HIERARCHY) < (((u64) 1) << (sizeof(payload_length) * 8)))
						}
					} break;

					case USBDescType_device_qualifier:   // ATmega32U4 does not support anything beyond full-speed, so this is a request error. See: Source(2) @ Section(9.6.2) @ Page(264) & [Endpoint 0: Request Error].
					case USBDescType_string:             // TODO Check if it's okay to be able to send a STALL here.
					case USBDescType_interface:          // TODO Check if it's okay to be able to send a STALL here.
					case USBDescType_endpoint:           // TODO Check if it's okay to be able to send a STALL here.
					case USBDescType_other_speed_config: // TODO Check if it's okay to be able to send a STALL here.
					case USBDescType_interface_power:    // TODO Check if it's okay to be able to send a STALL here.
					case USBDescType_cdc_interface:      // TODO Check if it's okay to be able to send a STALL here.
					case USBDescType_cdc_endpoint:       // TODO Check if it's okay to be able to send a STALL here.
					{
					} break;
				}

				if (payload_length)
				{
					_usb_endpoint_0_transmit
					(
						payload_data,
						request.get_descriptor.requested_amount < payload_length
							? request.get_descriptor.requested_amount
							: payload_length
					);
				}
				else
				{
					UECONX |= (1 << STALLRQ); // [Endpoint 0: Request Error].
				}
			} break;

			case USBSetupRequestType_set_address: // [Endpoint 0: SETUP-Transaction's SetAddress].
			{
				if (request.set_address.address < 0b0111'1111)
				{
					UDADDR = request.set_address.address;

					UEINTX &= ~(1 << TXINI);
					while (!(UEINTX & (1 << TXINI)));

					UDADDR |= (1 << ADDEN);
				}
				else
				{
					UECONX |= (1 << STALLRQ);
				}
			} break;

			case USBSetupRequestType_set_config: // [Endpoint 0: SETUP-Transaction's SetConfiguration]. TODO Revise.
			{
				if (request.set_config.value == 0)
				{
					debug_halt(4);
				}
				else if (request.set_config.value == USB_CONFIGURATION_HIERARCHY_CONFIGURATION_VALUE)
				{
					UEINTX &= ~(1 << TXINI);
					while (!(UEINTX & (1 << TXINI)));

					UENUM = USB_ENDPOINT_CDC_IN;
					{
						UECONX = (1 << EPEN);

						UECFG0X = (USBEndpointTransferType_bulk << EPTYPE0) | (1 << EPDIR);
						UECFG1X = (USB_ENDPOINT_CDC_IN_SIZE_CODE << EPSIZE0) | (1 << ALLOC);

						#if 1
						if (!(UESTA0X & (1 << CFGOK)))
						{
							debug_halt(5);
						}
						#endif
					}

					UENUM = USB_ENDPOINT_CDC_OUT;
					{
						UECONX = (1 << EPEN);

						UECFG0X = (USBEndpointTransferType_bulk << EPTYPE0);
						UECFG1X = (USB_ENDPOINT_CDC_OUT_SIZE_CODE << EPSIZE0) | (1 << ALLOC);

						#if 1
						if (!(UESTA0X & (1 << CFGOK)))
						{
							debug_halt(6);
						}
						#endif
					}

					UERST = 0x1E; // TODO ???
					UERST = 0;
				}
				else
				{
					UECONX |= (1 << STALLRQ);
				}
			} break;

			case USBSetupRequestType_cdc_get_line_coding: // TODO Understand and Explain.
			case USBSetupRequestType_cdc_set_control_line_state: // TODO Understand and Explain.
			{
				UECONX |= (1 << STALLRQ);
			} break;

			case USBSetupRequestType_cdc_set_line_coding: // TODO Understand and Explain
			{
				if (request.set_line_coding.incoming_line_coding_datapacket_size == sizeof(struct USBCDCLineCoding))
				{
					while (!(UEINTX & (1 << RXOUTI)));
					struct USBCDCLineCoding desired_line_coding = {0};
					for (u8 i = 0; i < sizeof(struct USBCDCLineCoding); i += 1)
					{
						((u8*) &desired_line_coding)[i] = UEDATX;
					}
					UEINTX &= ~(1 << RXOUTI);

					while (!(UEINTX & (1 << TXINI)));
					UEINTX &= ~(1 << TXINI);

					if (desired_line_coding.dwDTERate == 1200)
					{
						*(u16*)0x0800 = 0x7777; // https://github.com/PaxInstruments/ATmega32U4-bootloader/blob/bf5d4d1edff529d5cc8229f15463720250c7bcd3/avr/cores/arduino/CDC.cpp#L99C14-L99C14
						wdt_enable(WDTO_15MS);
						for(;;);
					}
				}
				else
				{
					UECONX |= (1 << STALLRQ);
				}
			} break;

			default:
			{
				for (;;)
				{
					for (u8 i = 0; i < 10; i += 1)
					{
						debug_u8(0xF0);
						_delay_ms(100.0);
						debug_u8(0x0F);
						_delay_ms(100.0);
					}

					debug_u8(request.type);
					_delay_ms(3000.0);
					debug_u8(request.type >> 8);
					_delay_ms(3000.0);
				}
			} break;
		}
	}
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
	lines and capacitor. So this first step is as simple just enabling this (2).

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
	have to assign to USBCON again afterwards to be able to clear the FRZCLK bit.

	We will also set OTGPADE to be able to detect whether or not we are connected to the
	host (VBUS bit), and let the host know of our presence when we are plugged in (4).

	(1) "USB interface" @ Source(1) @ Section(22.2) @ Page(270).
	(2) Crude USB State Machine @ Source(1) @ Figure (21-7) @ Page(260).
	(3) FRZCLK @ Source(1) @ Section(21.13.1) @ Page(267).
	(4) VBUS, USBSTA @ Source(1) @ Section(21.13.1) @ Page(268).
*/

/* [Configure USB Interface].
	We enable the End-of-Reset interrupt, so when the unenumerated USB device connects
	and the hosts sees this, the host will pull the data lines to a state to signal
	that the device should reset (as described by (1)), and when the host releases, the
	[USB Device Interrupt Routine] is then triggered.

	It is important that we do some of our initializations after the End-of-Reset
	signal since (1) specifically states that the endpoints will end up disabled
	(See: [Endpoints]).

	(1) "USB Reset" @ Source(1) @ Section(22.4) @ Page(271).
*/

/* [Wait For USB VBUS Information Connection].
	No need to actually wait for a connection. The control flow will continue on as
	normal; rest of the initialization will be done within the interrupt routines.
*/

/* [Endpoints].
	All USB devices have "endpoints" which is essentially a buffer where it can be
	written with data sent by the host and then read by the device, or written by the
	device to which it is then transmitted to the host.

	Endpoints are assigned a "transfer type" (1) where it is optimized for a specific
	purpose. For example, bulk-typed endpoints are great for transmitting large amounts
	of data (e.g. uploading a movie to a flashdrive) but there's no guarantee on when
	that data may be transmitted, which would actually be important for devices such as
	keyboards and mouses.

	The only transfer type that is bidirectional is control. What this means is that
	control-typed endpoints can transmit and also receive data-packets to and from the
	host using the same buffer. Isochronous, bulk, and interrupts are only
	unidirectional. These endpoints can only have their buffers be written to or be
	copied from by the firmware, so data-transfer is more-or-less one-way.

	Every USB device has endpoint 0 as a control-typed endpoint. This endpoint is where
	all the "enumeration" (initialization of USB device) happens through, and as so,
	endpoint 0 is also called the "default endpoint".

	The ATmega32U4 has a total of 7 endpoints, 6 if you don't count endpoint 0 since it
	is not really programmable as the other ones. The size of the buffers are
	customizable, with each having their own limitation on the maximum capacity. The
	endpoint buffers are all manually allocated by the MCU in a single 832-byte
	arena (2). The ATmega32U4 also has the ability to "double-buffer" or "double-bank"
	or "ping-pong", meaning that an endpoint can essentially have two buffers in use at
	the same time. One will be written to or read from by the hardware, while the other
	is written to or read from by the firmware. This allows us to do something like
	operate on a data-packet sent from the host while, simultaneously, the bytes of the
	next data-packet is being copied by the hardware. The role of these two buffers
	would flip back and forth, hence ping-pong mode.

	(1) Endpoint Transfer Types @ Source(2) @ Table(9-13) @ Page(270) & Source(1) @ Section(22.18.2) @ Page(286) & Source(2) @ Section(4.7) @ Page(20-21).
	(2) Endpoint Memory @ Source(1) @ Section(21.9) @ Page(263-264).
*/

/* [USB Device Interrupt Routine, Endpoint 0 Configuration, and Endpoint Interrupts].
	See: [Endpoints].

	Triggers for USB_GEN_vect are listed on (1):
		[ ] VBUS Plug-In.
		[ ] Upstream Resume.
		[ ] End-of-Resume.
		[ ] Wake Up.
		[X] End-of-Reset.
		[ ] Start-of-Frame.
		[ ] Suspension Detection.
		[ ] Start-of-Frame CRC Error.

	When an unenumerated device connects to the USB and the host sees this, the host
	will pull the D+/D- lines to a specific state where it signals to the device that it
	should reset (4), and when the host releases the D+/D- lines, USB_GEN_vect is then
	triggered. This is called the "End-of-Reset" event.

	Since (1) specifically states that the endpoints will end up disabled, it is
	important that we do the rest of our USB initialization after the End-of-Reset.

	The first endpoint that we will initialize is endpoint 0, which we will do by
	consulting the flowchart in (1):
		1. Select the endpoint.
				ATmega32U4 uses the system where writing to and reading from
				endpoint-specific registers (e.g. UECFG0X) depends on the currently
				selected endpoint determined by UPNUM (3).
		2. Activate endpoint.
				Self-explainatory. The only thing we have to be aware of is that we
				have to initiate endpoints in sequential order so the memory allocation
				works out (2).
		3. Configure.
				We set the endpoint's transfer type, direction, buffer size, etc.
		4. Allocate.
				The hardware does it all on-the-fly (2).
		5. Test endpoint configuration.
				Technically the allocation can fail for things like using more than
				832 bytes of memory or making a buffer larger than what that endpoint
				can support. This doesn't really need to be addressed, just as long we
				aren't dynamically reallocating, disabling, etc. endpoints at run-time.

	Endpoint 0 will always be a control-typed endpoint as enforced by the USB
	specification, so that part of the configuration is straight-forward. For the
	data-direction of endpoint 0, it seems like the ATmega32U4 datasheet wants us to
	define endpoint 0 to have an "OUT" direction (5), despite the fact that control-typed
	endpoints are bidirectional.

	One last thing we want from endpoint 0 is the ability for it to detect when it has
	received a SETUP-transaction. SETUP-transactions will be described more later on,
	but it's essentially where the rest of the enumeration will be communicated through.
	We enable the interrupt vector USB_COM_vect for this by using RXSTPE (7). The
	USB_COM_vect is the interrupt routine for any event related to endpoints themselves.

	Note that this interrupt is only going to trigger for endpoint 0. So if the host
	sends a SETUP-transaction to endpoint 3, it'll be ignored (unless the interrupt for
	that is also enabled) (6).

	(1) USB Device Controller Interrupt System @ Source(1) @ Figure(22-4) @ Page(279).
	(2) Endpoint Memory Management @ Source(1) @ Section(21.9) @ Page(263-264).
	(3) "Endpoint Selection" @ Source(1) @ Section(22.5) @ Page(271).
	(4) "USB Reset" @ Source(1) @ Section(22.4) @ Page(271).
	(5) EPDIR @ Source(1) @ Section(22.18.2) @ Page(287).
	(6) "USB Device Controller Endpoint Interrupt System" @ Source(1) @ Figure(22-5) @ Page(280).
	(7) "Endpoint Interrupt" @ Source(1) @ Figure(22-5) @ Page(280).
*/

/* [Packets and Transactions].
	A packet is the smallest coherent piece of message that the host can send to the
	device, or the device send to the host. The layout of a packet is abstractly
	structured as so: // TODO SOF packet

		....... HOST / DEVICE ......
		:                          :
		: <SYNC PID STUFF CRC EOP> :
		:                          :
		.......... PACKET ..........

		- SYNC announces on the USB that there's an incoming packet. SYNC consists
		of oscillating D- and D+ signals that calibrates clocks in order to ensure the
		parsing of the upcoming signal is done correctly.

		- PID is the "packet ID", and this determines the purpose of the packet. PIDs
		are grouped into four categories: TOKEN, DATA, HANDSHAKE, and SPECIAL (1). For
		example, ACK and NACK packets are considered HANDSHAKE-typed packets.

		- STUFF is any additional data that is defined by the PID's category. For
		example, TOKEN-typed packets define the device address and endpoint that the
		host wants to talk to. For packets belonging to the HANDSHAKE category,
		there is no additional data. STUFF is where DATA-typed packets obviously
		transmit the raw bytes of the data.

		- CRC is used for error checking to ensure that the packet was not read in a
		malformed manner.

		- EOP signals the USB hardware that it has reached the end of the packet.

	Packets are thoroughly explained by (1), and are mostly already handled by the
	hardware, so the firmware doesn't have to worry about things like calculating the
	CRC for instance.

	Packets together form "transactions", which always begins with the host sending a
	TOKEN-typed packet. This will always be the case since USB is host-centric; thus
	the USB device can only ever reply the host and never be able to initiate a
	transaction of its own.

	The exact layout of a transaction varies a lot on factors like endpoint transfer
	types and error conditions, but I'll describe it in the common case where it's
	carried out in three stages:

	...................................................................................
	:-------- HOST ---------     --- HOST / DEVICE ---     ----- HOST / DEVICE ------ :
	: | TOKEN-TYPED PACKET | ... | DATA-TYPED PACKET | ... | HANDSHAKE-TYPED PACKET | :
	: ----------------------     ---------------------     -------------------------- :
	.................................. TRANSACTION ....................................

	The TOKEN-typed packet simply lets the host announce what device it wants to talk
	to and at which endpoint of that device, along with the purpose of the entire
	transaction. If the device's USB hardware detects a TOKEN-typed packet, but it is
	addressed to a different device, then the hardware will ignore it, and thus the
	firmware will not have to worry about it all.

	Next is the DATA-typed packet which could be sent by the host, by the device, or not
	at all depending on the PID of the earlier TOKEN-typed packet. There may not be a
	DATA-typed packet because the transaction would simply not need it, or because there
	was an error, to which the transaction may go immediately to the HANDSHAKE-typed
	packet stage (see: [Endpoint 0: Request Error]).

	The HANDSHAKE-typed packet is the final packet that is sent either by the host or
	device (or even not at all). The direction of the transfer of this packet is
	opposite of the DATA-typed packet (e.g. if the host sent the data, then the device
	is the one that responds here). The HANDSHAKE-typed packet is often used by the
	recipent of the data to ACK or NACK the data that was sent. If the device (and only
	the device) is in a state where it cannot not handle this transaction, then the
	device can transmit a STALL-HANDSHAKE packet for the handshake (see:
	[Endpoint 0: Request Error]).

	Once again, the layout of a transaction varies case-by-case. For example,
	isochronous endpoints will engage in transactions with the host where there will
	never be a HANDSHAKE-typed packet. This is because the entire point of isochronous
	endpoints is to transmit time-sensitive data quickly (such as a live video or
	song), but if a DATA-typed packet was dropped here and there, then its no big deal;
	there's no need to waste USB bandwidth on handshakes.

	The most common transactions are SETUPs, INs, and OUTs.

		- SETUP-transactions are where the host sends the device a DATA-typed packet
		that specifies what the host wants (see: struct USBSetupRequest). This could be
		things ranging from requesting a string from the device or setting the device
		into a specific configuration. This transaction is reserved only for the
		control-typed endpoints and the device will be the one to send the host a
		HANDSHAKE-typed packet.

		- IN-transactions are where the device is the one sending data to the host (the
		naming is always from the host's perspective); that is, the DATA-typed packet
		is transmitted by the device for the host. In this transaction, the host will
		be the one to send the HANDSHAKE-typed packet.

		- OUT-transactions are just like the IN-transactions, but the host is the one
		sending the DATA-typed packet and the device responding with a HANDSHAKE-typed
		packet.

	(1) Packet IDs @ Source(2) @ Table(8-1) @ page(196).
	(2) Packets @ Source(4) @ Chapter(3).
*/

/* [Endpoint 0: Data-Transfer from Device to Host].
	See: [Endpoints].
	See: [Packets and Transactions].

	This procedure is called when we are in a state where endpoint 0 needs to transmit
	some data to the host in response to a SETUP-transaction. This process is
	non-trivial since the data that needs to be sent might have to be broken up into
	multiple transactions and also the fact that the host could abort early.

	Consider the following diagram:

	.......................................................................
	: =====================     ==================     ================== :
	: ( SETUP-TRANSACTION ) --> ( IN-TRANSACTION ) --> ( IN-TRANSACTION ) :
	: =====================     ==================     ================== :
	:                                                           |         :
	:                                                           v         :
	:   ===================     ==================     ================== :
	:   ( OUT-TRANSACTION ) <-- ( IN-TRANSACTION ) <-- ( IN-TRANSACTION ) :
	:   ===================     ==================     ================== :
	........... EXAMPLE OF ENDPOINT 0 SENDING DATA TO THE HOST ............

	Before this procedure was called, endpoint 0 received a SETUP-transaction where the
	host, say, asked the device for the string of its manufacturer's name. If
	endpoint 0's buffer is small and/or the requested amount of data is large enough,
	then this transferring of data will be done through a series of IN-transactions.

	The procedure will be called to begin handling the series of IN-transactions that
	will be sent by the host. In each IN-transaction, the procedure will fill up
	endpoint 0's buffer as much as possible. Only the data-packet in the last
	IN-transaction may be smaller than endpoint 0's maximum capacity in the case that
	the requested amount of data is not a perfect multiple of the buffer size (1). In
	other words, we should be expecting the host to stop sending IN-transactions when
	the requested amount of data has been sent or if the data-packet it received is
	shorter than endpoint 0's maximum capacity.

	The entire conversation will conclude once the device receives the OUT-transaction.
	No data will actually be sent to the device since the data-packet here is
	always zero-length; that is, the DATA-typed packet in the OUT-transaction will be
	empty.

	The host can "abort" early by sending an OUT-transaction before the device has
	completely sent all of its data. For example, the host might've asked for 1024 bytes
	of the string, but for some reason it needed to end the transferring early, so it
	sends an OUT-transaction (instead of another IN-transaction) even though the device
	has only sent, say, 256 bytes of the requested 1024 bytes.

	(1) Control Transfers @ Source(2) @ Section(8.5.3) @ Page(226).
*/

/* [Endpoint 0: SETUP-Transactions].
	SETUP-transactions are exclusive to control-typed endpoints (which endpoint 0 is
	always defined as). It is the first of many transactions sent by the host to load
	appropriate drivers and perform the enumeration process to get the USB device up
	and running.

	A timeline inside of a SETUP-transaction:

		..................................................
		:            -------- HOST --------              :
		:            | SETUP-TOKEN PACKET |              :
		:            ----------------------              :
		:                                                :
		............ SETUP-TRANSACTION (1/3) .............

		First, the host sends a packet addressed to the device at endpoint 0 with the
		PID of "SETUP" (1).

		..................................................
		:            -------- HOST --------              :
		:            | SETUP-TOKEN PACKET |              :
		:            ----------------------              :
		:                      :                         :
		:                      :                         :
		:                                                :
		: ------------------- HOST --------------------- :
		: | DATA-TYPED PACKET (struct USBSetupRequest) | :
		: ---------------------------------------------- :
		:                                                :
		............ SETUP-TRANSACTION (2/3) .............

		After that, the host sends another packet, specifically a DATA-typed packet.
		The data that is transmitted here is struct USBSetupRequest (4).

		..................................................
		:            -------- HOST --------              :
		:            | SETUP-TOKEN PACKET |              :
		:            ----------------------              :
		:                      :                         :
		:                      :                         :
		:                                                :
		: ------------------- HOST --------------------- :
		: | DATA-TYPED PACKET (struct USBSetupRequest) | :
		: ---------------------------------------------- :
		:                      :                         :
		:                      :                         :
		:                                                :
		:           -------- DEVICE --------             :
		:           | ACK-HANDSHAKE PACKET |             :
		:           ------------------------             :
		:                                                :
		............ SETUP-TRANSACTION (3/3) .............

		Up to this point, the host had no idea whether or not the device is receiving
		any of this information. This is where the last stage of the transaction comes
		in. According to (2), the ATmega32U4 will always send an ACK-HANDSHAKE packet (1)
		as a response for any control-typed endpoint that is involvegd in this
		SETUP-transaction. This is also when the USB_COM_vect interrupt would be
		triggered.

	Within USB_COM_vect, we can copy the the host's DATA-typed packet of the
	SETUP-transaction via UEDATX, byte-by-byte.

	As seen in (3), the first byte of the setup data is "bmRequestType" which is a
	bitmap detailing the characteristics of the host's request. The byte after that is
	the specific request that the host is asking for. In the majority of cases, we can
	just simply represent these two bytes as a single integer word, and this ends up
	being the ".type" field of USBSetupRequest. See: USBSetupRequestType.

	(1) PIDs @ Source(2) @ Table(8-1) @ Page(196).
	(2) "CONTROL Endpoint Management" @ Source(1) @ Section(22.12) @ Page(274).
	(3) "Format of Setup Data" @ Source(2) @ Table(9-2) @ Page(248).
	(4) "Every Setup packet has eight bytes." @ Source(2) @ Section(9.3) @ Page(248).
*/

/* [Endpoint 0: Request Error].
	If the host requested some piece of information from the device, then a successful
	data-transferring process would look something like this:
	.......................................................................
	: =====================     ==================     ================== :
	: ( SETUP-TRANSACTION ) --> ( IN-TRANSACTION ) --> ( IN-TRANSACTION ) :
	: =====================     ==================     ================== :
	:                                                           |         :
	:                                                           v         :
	:   ===================     ==================     ================== :
	:   ( OUT-TRANSACTION ) <-- ( IN-TRANSACTION ) <-- ( IN-TRANSACTION ) :
	:   ===================     ==================     ================== :
	........... EXAMPLE OF ENDPOINT 0 SENDING DATA TO THE HOST ............

	If the host requested some piece of information that the device does not have, or
	some other request that the device cannot carry out, then a STALL-HANDSHAKE packet
	must be sent as soon as possible. This is also synonomously referred to as a
	"Request Error" (3).

	A STALL-HANDSHAKE packet cannot be given to the host during the SETUP-transaction,
	since (1) specifically states that control-typed endpoints handling
	SETUP-transactions will always ACK it. Furthermore, the firmware at this point
	haven't copied and parsed the data-packet sent by the host to determine that it
	wouldn't be able to fulfill it. Thus, the earliest that the STALL packet can be
	sent is whenever the next transaction arrives. This is expected, as stated in (3).

	So this is what it'd look like inside of a IN-transaction that is successful:
	..............................................................................
	: ------ HOST -------     ------ DEVICE -------     --------- HOST --------- :
	: | IN-TOKEN PACKET | ... | DATA-TYPED PACKET | ... | ACK-HANDSHAKE PACKET | :
	: -------------------     ---------------------     ------------------------ :
	........................ SUCCESSFUL IN-TRANSACTION ...........................

	But if we want to abort the request, then we skip the DATA-typed packet stage
	entirely and have ourselves send a HANDSHAKE-typed packet with a PID of "STALL"
	instead.
	......................................................
	: ------ HOST -------     --------- DEVICE --------- :
	: | IN-TOKEN PACKET | ... | STALL-HANDSHAKE PACKET | :
	: -------------------     -------------------------- :
	............. STALLED IN-TRANSACTION .................

	And from here onwards, endpoint 0 will continiously send a STALL-HANDSHAKE packet
	as a response to the host. We exit the "STALL condition" once we receive the next
	SETUP-transaction (2) and can begin ACK'ing once more.

	(1) "CONTROL Endpoint Management" @ Source(1) @ Section(22.12) @ Page(274).
	(2) "STALL Handshakes Returned by Control Pipes" @ Source(2) @ Section(8.5.3.4) @ Page(228) & [Endpoint 0: Request Error].
	(3) "Request Error" @ Source(2) @ Section(9.2.7) @ Page(247).
*/

/* [Endpoint 0: SETUP-Transaction's GetDescriptor].
	The host sent a SETUP-transaction with a request, as described in the DATA-typed
	packet laid out in USBSetupRequest, telling the device to tell the host more about
	itself, hence "descriptor".

	One of the things the host might ask is what the device itself is. This is detailed
	within USB_DEVICE_DESCRIPTOR. Things such as the device's vendor and the amount of
	ways the device can be configured is transmitted to the host.

	Not all descriptors can be fulfilled. In this case, the device will send the host a
	STALL-HANDSHAKE packet as a response. See [Endpoint 0: Request Error].
*/

/* [Interfaces, Configurations, and Classes].
	An "interface" of a USB device is just a group of endpoints that are used for a
	specific functionality. A simple mouse, for instance, may only just have a single
	interface with one endpoint that simply reports the delta-movement to the host.
	A fancier mouse with a numberpad on the side may have two interfaces: one for the
	mouse functionality and the other for the keyboard of the numberpad. A flashdrive
	can have two endpoints: transmitting and receiving files to and from the host. The
	two endpoints here would be grouped together into one interface.

	The functionality of an interface is determined by the "class" (specifically
	bInterfaceClass). An example would be HID (human-interface device) which include,
	mouses, keyboards, data-gloves, etc. The USB specification does not detail much at
	all about these classes; that is, the corresponding class specification must be
	consulted. The entire USB device can optionally be labeled as a single class (via
	bDeviceClass). This seems to be useful for the host in loading the necessary
	drivers, according to (1), but it is not required for the device to be classified as
	anything specific.

	A set of interfaces is called a "configuration", and the host must choose the most
	appropriate configuration for the device. The ability of having multiple
	configurations allows greater flexibility of when and where the device can be used.
	For example, if the host is a laptop that is currently on battery-power, the host
	may choose a device configuration that is more power-efficient. When the laptop is
	plugged in, it may reconfigure the device to now use as much power as it wants.

	We define a single configuration (consult (2) for how configuration descriptor is
	laid out), and within it, we implement interfaces for each of the following classes:
		- [CDC - Communication Class Device].
		- [HID - Human Interface Device Class].
		- [Mass Storage Device Class].

	(1) Device and Configuration Descriptors @ Source(4) @ Chapter(5).
	(2) "USB Descriptors" @ Source(4) @ Chapter(5).
*/

/* [CDC - Communication Class Device]. TODO Relook
	The USB communication devices class is designed for all sorts of communication
	devices, such as modems, telephones, ethernet hubs, etc. The reason why we implement
	this class for the device is so we can transmit data to the host for diagnostic
	purposes.

	To even begin to understand our implementation of CDC, we must rewind far back into
	the history of a communication system called POTS (Plain Old Telephone Service):
	.......................................................................
	.                                                                     :
	:                    ||                         ||                    :
	:                  |=||=|~~~~~~~~~~~~~~~~~~~~~|=||=|                  :
	:                    ||            ^            ||                    :
	:  /=========\       ||   (PURE ANALOG SIGNAL)  ||       /=========\  :
	: (O) |===| (O)      ||                         ||      (O) |===| (O) :
	:    / (#) \         ||                         ||         / (#) \    :
	:    \_____/~~~~~~~~~||                         ||~~~~~~~~~\_____/    :
	:                    ||                         ||                    :
	:.....................................................................:
	With many simplications, we can see that two plain old telephones would communicate
	to each other via analog signals travelling down some copper wires. This is great
	for humans to exchange pleasantries, but computers, however, cannot use these analog
	signals; it must be in the form of digital signals. That is, there needs to exist
	a device that "modulates" digital signals into analog signals, and a device to
	"demodulate" analog signals back into the original digital signals.
	This modulator-demodulator is the "modem".
	.......................................................................
	.                                                                     :
	:                    ||                         ||                    :
	:                  |=||=|~~~~~~~~~~~~~~~~~~~~~|=||=|                  :
	:                    ||            ^            ||                    :
	:  /=========\       ||   (PURE ANALOG SIGNAL)  ||       /=========\  :
	: (O) |===| (O)      ||                         ||      (O) |===| (O) :
	:    / (#) \         ||                         ||         / (#) \    :
	:    \_____/~~~~~~~~~||                         ||~~~~~~~~~\_____/    :
	:       ~            ||                         ||            ~       :
	:       ~                                                     ~       :
	:       ~             _______             _______             ~       :
	:    =======         ||.....||           ||.....||         =======    :
	:    |MODEM| 1010101 ||.....||           ||.....|| 1010101 |MODEM|    :
	:    =======    ^    ||_____||           ||_____||    ^    =======    :
	:               |    [  -=.  ]           [  -=.  ]    |               :
	:               |    =========           =========    |               :
	:               |                                     |               :
	:               -------- (PURE DIGITAL SIGNAL) --------               :
	:.....................................................................:

	This is also where the name "terminal" come from. The computers that'd display the
	received information were quite literally the terminal endings of communication
	systems such as this.

	Thus, we can essentially use CDC to mimick our device to be something like a modem
	and serially transmit data to the host (the fact we aren't really a modem that's
	working with analog signals doesn't really matter). Since we are going to define
	a couple interfaces for CDC later on, so we will need to declare the device's class
	as being CDC in the device descriptor (see: USB_DEVICE_DESCRIPTOR) as mentioned in
	(5).

	As stated earlier, there are many communication devices that the CDC specification
	wants to be compatiable with. The one that will be the most applicable for us is the
	"Abstract Control Model" as seen in (4). Based on the figure, the specification
	is assuming we are some sort of device that has components for data-compression,
	error-correction, etc. just like a modem might have. But once again, this doesn't
	really matter; we're just sending pure binary data-packets to the host via USB.

	Within our device configuration (see: USB_CONFIGURATION_HIERARCHY), we define the
	"Communication Class Interface". This interface is responsible for
	"device management" and "call management", and is required for all communication
	devices, according to (1). As for what device and call management entails:

		- "Device Management" is any operation the host would want to do to "control
		and configure the operational state of the device" (3). These operations would
		be carried out by the "management element" which is just a fancy name for
		endpoint 0 (2). The host would send CDC-specific requests to endpoint 0 for
		things like "ringing the auxillary phone jack", all of which is detailed in (6).
		For the most part, we really don't care about these requests.

		- "Call Management" is "responsible for setting up and tearing down of calls"
		(7). As to what specific things are done in call management, I'm actually not
		too sure. The specification wasn't too clear on this, but I think it's
		more-or-less a subset of the management element requests of things like hanging
		up phones (SET_HOOK_STATE) or dialing numbers (DIAL_DIGITS). This is further
		supported by the fact that an example configuration in (8) states that the
		"management element will transport both call management element and device
		management element commands".

	The CDC interface is also required to have the "notification element" (9) which // TODO WE DONT USE NOTIFICATION ELEMENT??
	simply lets the host know of certain events like whether or not the phone is hooked
	or not (10). Like with the management element, the notification element is just
	another endpoint, but this time it's not endpoint 0. We are to give up an endpoint
	and it seems like it's usually an interrupt-typed endpoint as stated in (11),
	but (12) states that it could also be a bulk-typed endpoint.

	After we define the CDC interface descriptor, we then list a series of "functional
	descriptors" (13) which are proprietary to the CDC specification. Each functional
	descriptor just state some small piece of information related to the communication
	device we are using for the host.

		- "Header Functional Descriptor" is always the first functional descriptor and
		it only lists the CDC specification we are using. That's all there is to it.

		- "Call Management Functional Descriptor" TODO (15).

		- "Abstract Control Management Functional Descriptor" TODO (16).

		- "Union Functional Descriptor" (17) is used to group all the interfaces that
		support the communication functionality together. It is a required descriptor
		(18) that is is probably more apparently useful in communication devices that
		involve things like audio and video, but given that we only have two interfaces
		(this CDC interface and the following CDC-data interface), we can just say that
		this CDC interface is the "master" and the other CDC-data interface is the
		"slave". Thus, any events relating to this group will be made aware of to the
		master interface.

	So in short, the CDC interface is used by the host to determine what kind
	communication device it's talking to and its properties.

	(1) "Interface Definitions" @ Source(6) @ Section(3.3) @ AbsPage(20).
	(2) "Communication Class Interface" @ Source(6) @ Section(3.3.1) @ AbsPage(20).
	(3) Device Management Definition @ Source(6) @ Section(1.4) @ AbsPage(15).
	(4) "Abstract Control Model" @ Source(6) @ Section(3.6.2) @ AbsPage(26).
	(5) Communication Device Class Code @ Source(6) @ Section(3.2) @ AbsPage(20).
	(6) "Management Element Request" @ Source(6) @ Section(6.2) @ AbsPage(62).
	(7) Call Management Definition @ Source(6) @ Section(1.4) @ AbsPage(15).
	(8) Example Configurations @ Source(6) @ Section(3.3.1) @ AbsPagee(21).
	(9) Endpoints on Abstract Control Models @ Source(6) @ Section(3.6.2) @ AbsPage(26).
	(10) "Notification Element Notifications" @ Source(6) @ Section(6.3) @ AbsPage(84).
	(11) Notification Element Definition @ Source(6) @ Section(1.4) @ AbsPage(16).
	(12) Notification Element Endpoint @ Source(6) @ Section(3.3.1) @ AbsPage(20).
	(13) Functional Descriptors @ Source(6) @ Section(5.2.3) @ AbsPage(43).
	(14) "Header Functional Descriptor" @ Source(6) @ Section(5.2.3.1) @ AbsPage(45).
	(15) "Call Management Functional Descriptor" @ Source(6) @ Section(5.2.3.2) @ AbsPage(45-46).
	(16) "Abstract Control Management Functional Descriptor" @ Source(6) @ Section(5.2.3.3) @ AbsPage(46-47).
	(17) "Union Functional Descriptor" @ Source(6) @ Section(5.2.3.8) @ AbsPage(51).
	(18) "Communication Device Management" @ Source(6) @ Section(3.1.1.) @ AbsPage(19).
*/

/* [HID - Human Interface Device Class].
	TODO
*/

/* [Mass Storage Device Class].
	TODO
*/

/* [Endpoint 0: SETUP-Transaction's SetAddress].
	The host would like to set the device's address, which at this point is zero by
	default (as with any other USB device when it first connects). The desired address
	is stated within the SETUP-transaction's data-packet's 16-bit wIndex field (3).

	According to (1), any address is a 7-bit unsigned integer, so if we received a
	desired address from the host that can't fit into that, then something has gone
	quite wrong.

	Once we have the desired address, we set it as so in the UDADDR register, but don't
	actually enable it until later, as stated in (2).

	So after the host sent the SETUP-transaction asking to set the device's address,
	a IN-transaction (still to address 0 (4)) is sent by the host to test whether or not
	the device understood the request. We don't actually have to send any meaningful
	content to the host, just a zero-lengthed data-packet. This is done by not writing
	anything to UEDATX and then clearing TXINI.

	Just because we clear TXINI doesn't mean that we the device has sent the DATA-typed
	packet for the IN-transaction however. The hardware is possibily still waiting for
	that IN-TOKEN packet to initiate the transaction, or possibly is currently in the
	middle of transmitting the DATA-typed packet (just because it's zero-lengthed
	doesn't mean no signal is being sent; the PID for instance still has to be
	transmitted. See: [Packets and Transactions]). Thus, if we set ADDEN to enable the
	new USB device address too soon, then we might fail to actually perform the
	IN-transaction with the host.

	Therefore we have to spinlock on TXINI until it is set, which is how we can find out
	when the endpoint buffer has been transmitted entirely. It is only after this that
	we then can enable the address.

	(1) "Address Field" @ Source(2) @ Section(8.3.2.1) @ Page(197).
	(2) "Address Setup" @ Source(1) @ Section(22.7) @ Page(272).
	(3) SetAddress Data-Packet Layout @ Source(2) @ Section(9.4.6) @ Page(256).
	(4) "Set Address Processing" @ Source(2) @ Section(9.2.6.3) @ Page(246).
*/

/* [Endpoint 0: SETUP-Transaction's SetConfiguration].
	This is where the host obviously set the configuration of the device. We only have
	a single configuration defined, so we already pretty much set for that. The host
	could send a 0 to indicate that we should be in the "address state" (1), which is
	the state a USB device is in after it has been assigned an address. But once again,
	since we only have configuration and we're already prepped for that, there really
	isn't anything we have to do here.

	When the host wants to set the device to our only configuration
	(USB_CONFIGURATION_HIERARCHY), it must provide the value
	USB_CONFIGURATION_HIERARCHY_CONFIGURATION_VALUE, the very same value within
	USB_CONFIGURATION_HIERARCHY.

	If the host sends us some other configuration value, then we will report this as
	a request error.

	(1) "Address State" @ Source(2) @ Section(9.4.7) @ Page(257).
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
