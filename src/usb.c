#define error error_pin(PinErrorSource_usb)

static void
usb_init(void)
{ // [USB Initialization Process].

	// 1. "Power on USB pads regulator".
	UHWCON = (1 << UVREGE);

	// 2. "Configure PLL interface and enable PLL".
	PLLCSR = (1 << PINDIV) | (1 << PLLE);

	// 3. "Check PLL lock".
	while (!(PLLCSR & (1 << PLOCK)));

	// 4. "Enable USB interface".
	USBCON = (1 << USBE);
	USBCON = (1 << USBE) | (1 << OTGPADE);

	// 5. "Configure USB interface".
	UDIEN = (1 << EORSTE) | (1 << SOFI);
	UDCON = 0; // Clearing the DETACH bit allows the device to be connected to the host. See: Source(1) @ Section(22.18.1) @ Page(281).

	// 6. "Wait for USB VBUS information connection".
	#if DEBUG
	while (!debug_usb_rx_diagnostic_signal);
	#endif
}

ISR(USB_GEN_vect)
{ // [USB Device Interrupt Routine].

	if (UDINT & (1 << EORSTI)) // End-of-Reset.
	{
		UENUM = 0; // 1. "Select the endpoint".
		{
			// 2. "Activate endpoint".
			UECONX = (1 << EPEN);

			// 3. "Configure and allocate".
			UEIENX  = (1 << RXSTPE); // Endpoint 0 will listen to the reception of SETUP-transactions.
			UECFG1X = (USB_ENDPOINT_DFLT_SIZE << EPSIZE0) | (1 << ALLOC);

			// 4. "Test endpoint configuration".
			#if DEBUG
			if (!(UESTA0X & (1 << CFGOK)))
			{
				error; // Hardware determine that this endpoint's configuration is invalid.
			}
			#endif
		}
	}

	if (UDINT & (1 << SOFI)) // Start-of-Frame.
	{
		UENUM = USB_ENDPOINT_CDC_IN;
		if (UEINTX & (1 << TXINI)) // Endpoint's buffer is ready to be filled up with data to send to the host.
		{
			while
			(
				(UEINTX & (1 << RWAL)) &&                                    // Endpoint's buffer still has some space left.
				_usb_cdc_in_reader_masked(0) != _usb_cdc_in_writer_masked(0) // Our ring buffer still has some data left.
			)
			{
				UEDATX              = _usb_cdc_in_buffer[_usb_cdc_in_reader_masked(0)];
				_usb_cdc_in_reader += 1;
			}

			UEINTX &= ~(1 << TXINI);   // Must be cleared first before FIFOCON. See: Source(1) @ Section(22.14) @ Page(276).
			UEINTX &= ~(1 << FIFOCON); // We are done writing to the bank, either because the endpoint's buffer is full or because there's no more deta to provide.
		}

		UENUM = USB_ENDPOINT_CDC_OUT;
		if (UEINTX & (1 << RXOUTI)) // Endpoint's buffer has data from the host to be copied.
		{
			while
			(
				(UEINTX & (1 << RWAL)) &&                                      // Endpoint's buffer still has some data left to be copied.
				_usb_cdc_out_writer_masked(1) != _usb_cdc_out_reader_masked(0) // Our ring buffer still has some space left.
			)
			{
				_usb_cdc_out_buffer[_usb_cdc_out_writer_masked(0)] = UEDATX;
				_usb_cdc_out_writer += 1;
			}

			if (UEINTX & (1 << RWAL)) // Endpoint's buffer still remaining data to be copied.
			{
				// The data that has yet to be copied from will still remain in the endpoint's buffer.
				// Any incoming IN-TOKEN packets by the host will be replied with a NACK-HANDSHAKE packet by the device.
				// See: Timing Diagram @ Source(1) @ Section(22.13.1) @ Page(276).
			}
			else // Endpoint's buffer has been completely copied.
			{
				UEINTX &= ~(1 << RXOUTI);  // Must be cleared first before FIFOCON. See: Source(1) @ Section(22.13.1) @ Page(275).
				UEINTX &= ~(1 << FIFOCON); // We free up the endpoint's buffer so we can accept the next OUT-transaction to this endpoint.
			}
		}
	}

	UDINT = 0; // Clear interrupt flags to prevent this routine from executing again.
}

static void
_usb_endpoint_0_in(u8* packet_data, u16 packet_length)
{ // [Endpoint 0: Data-Transfer from Device to Host].

	u8* data_cursor    = packet_data;
	u16 data_remaining = packet_length;
	while (true)
	{
		if (UEINTX & (1 << RXOUTI)) // Received OUT-transaction?
		{
			UEINTX &= ~(1 << RXOUTI); // Clear interrupt flag for this OUT-transaction. See: Pseudocode @ Source(2) @ Section(22.12.2) @ Page(275).
			break;
		}
		else if (UEINTX & (1 << TXINI)) // Endpoint 0's buffer ready to be filled?
		{
			u8 writes_left = data_remaining;
			if (writes_left > USB_ENDPOINT_DFLT_SIZE)
			{
				writes_left = USB_ENDPOINT_DFLT_SIZE;
			}

			data_remaining -= writes_left;
			while (writes_left)
			{
				UEDATX        = data_cursor[0];
				data_cursor  += 1;
				writes_left  -= 1;
			}

			UEINTX &= ~(1 << TXINI); // Endpoint 0's buffer is ready for the next IN-transaction.
		}
	}
}

ISR(USB_COM_vect)
{ // See: [USB Endpoint Interrupt Routine].

	UENUM = 0;
	if (UEINTX & (1 << RXSTPI)) // [Endpoint 0: SETUP-Transactions].
	{
		UECONX |= (1 << STALLRQC); // SETUP-transactions lift STALL conditions. See: Source(2) @ Section(8.5.3.4) @ Page(228) & [Endpoint 0: Request Error].

		struct USBSetupRequest request = {0};
		for (u8 i = 0; i < sizeof(request); i += 1)
		{
			((u8*) &request)[i] = UEDATX;
		}
		UEINTX &= ~(1 << RXSTPI); // Let the hardware know that we finished copying the SETUP-transaction's data-packet. See: Source(1) @ Section(22.12) @ Page(274).

		switch (request.type)
		{
			case USBSetupRequestType_get_desc: // [Endpoint 0: SETUP-Transaction's GetDescriptor].
			{
				u8  payload_length = 0; // Any payload we send will not exceed 255 bytes.
				u8* payload_data   = 0;

				switch ((enum USBDescType) request.get_desc.desc_type)
				{
					case USBDescType_device: // See: [Endpoint 0: SETUP-Transaction's GetDescriptor].
					{
						payload_data   = (u8*) &USB_DEVICE_DESCRIPTOR;
						payload_length = sizeof(USB_DEVICE_DESCRIPTOR);
						static_assert(sizeof(USB_DEVICE_DESCRIPTOR) < (((u64) 1) << bitsof(payload_length)))
					} break;

					case USBDescType_config: // [Interfaces, Configurations, and Classes].
					{
						if (!request.get_desc.desc_index) // We only have a single configuration.
						{
							payload_data   = (u8*) &USB_CONFIGURATION_HIERARCHY;
							payload_length = sizeof(USB_CONFIGURATION_HIERARCHY);
							static_assert(sizeof(USB_CONFIGURATION_HIERARCHY) < (((u64) 1) << bitsof(payload_length)))
						}
					} break;

					case USBDescType_device_qualifier: // See: Source(2) @ Section(9.6.2) @ Page(264).
					case USBDescType_string:           // See: [USB Strings] @ defs.h.
					{
						// We induce STALL condition. See: [Endpoint 0: Request Error].
					} break;

					case USBDescType_interface:
					case USBDescType_endpoint:
					case USBDescType_other_speed_config:
					case USBDescType_interface_power:
					case USBDescType_cdc_interface:
					case USBDescType_cdc_endpoint:
					{
						error; // We can probably address some of these cases, but we won't do it if it doesn't seem to inhibit base functionality of USB.
					} break;
				}

				if (payload_length)
				{
					_usb_endpoint_0_in // TODO Inline?
					(
						payload_data,
						request.get_desc.requested_amount < payload_length
							? request.get_desc.requested_amount
							: payload_length
					);
				}
				else
				{
					UECONX |= (1 << STALLRQ); // See: [Endpoint 0: Request Error].
				}
			} break;

			case USBSetupRequestType_set_address: // [Endpoint 0: SETUP-Transaction's SetAddress].
			{
				if (request.set_address.address < 0b0111'1111)
				{
					UDADDR = request.set_address.address;

					UEINTX &= ~(1 << TXINI);          // Send out zero-length data-packet for the upcoming IN-transaction.
					while (!(UEINTX & (1 << TXINI))); // Wait for the IN-transaction to be completed.

					UDADDR |= (1 << ADDEN);
				}
				else
				{
					error; // The host somehow sent us an address that's not within 7-bits. See: Source(2) @ Section(8.3.2.1) @ Page(197).
				}
			} break;

			case USBSetupRequestType_set_config: // [Endpoint 0: SETUP-Transaction's SetConfiguration]. TODO Revise.
			{
				switch (request.set_config.value)
				{
					case 0:
					{
						error; // In the case that the host, for some reason, wants to set the device back to the "Address State", we should handle this.
					} break;

					case USB_CONFIGURATION_HIERARCHY_CONFIGURATION_VALUE:
					{
						UEINTX &= ~(1 << TXINI);
						while (!(UEINTX & (1 << TXINI)));

						static_assert(USB_ENDPOINT_DFLT < USB_ENDPOINT_CDC_IN);

						UENUM = USB_ENDPOINT_CDC_IN;
						{
							UECONX = (1 << EPEN);

							UECFG0X = (USBEndpointTransferType_bulk << EPTYPE0) | (1 << EPDIR);
							UECFG1X = (USB_ENDPOINT_CDC_IN_SIZE_CODE << EPSIZE0) | (1 << ALLOC);

							#if DEBUG
							if (!(UESTA0X & (1 << CFGOK)))
							{
								error; // Hardware determine that this endpoint's configuration is invalid.
							}
							#endif
						}

						static_assert(USB_ENDPOINT_CDC_IN < USB_ENDPOINT_CDC_OUT);

						UENUM = USB_ENDPOINT_CDC_OUT;
						{
							UECONX = (1 << EPEN);

							UECFG0X = (USBEndpointTransferType_bulk << EPTYPE0);
							UECFG1X = (USB_ENDPOINT_CDC_OUT_SIZE_CODE << EPSIZE0) | (1 << ALLOC);

							#if DEBUG
							if (!(UESTA0X & (1 << CFGOK)))
							{
								error; // Hardware determine that this endpoint's configuration is invalid.
							}
							#endif
						}

						UERST = 0x1E; // TODO ???
						UERST = 0;
					} break;

					default:
					{
						error;
					} break;
				}
			} break;

			case USBSetupRequestType_cdc_get_line_coding: // TODO Understand and Explain.
			case USBSetupRequestType_cdc_set_control_line_state: // TODO Understand and Explain.
			{
				UECONX |= (1 << STALLRQ);
			} break;

			case USBSetupRequestType_cdc_set_line_coding: // TODO Understand and Explain
			{
				if (request.cdc_set_line_coding.incoming_line_coding_datapacket_size == sizeof(struct USBCDCLineCoding))
				{
					// Wait to receive data-packet of OUT-transaction.
					while (!(UEINTX & (1 << RXOUTI)));

					// Copy data-packet of OUT-transaction.
					struct USBCDCLineCoding desired_line_coding = {0};
					for (u8 i = 0; i < sizeof(struct USBCDCLineCoding); i += 1)
					{
						((u8*) &desired_line_coding)[i] = UEDATX;
					}

					UEINTX &= ~(1 << RXOUTI);

					// Acknowledge the upcoming IN-transaction.
					while (!(UEINTX & (1 << TXINI)));
					UEINTX &= ~(1 << TXINI);

					switch (desired_line_coding.dwDTERate)
					{
						case BOOTLOADER_BAUD_SIGNAL:
						{
							*(u16*)0x0800 = 0x7777; // https://github.com/PaxInstruments/ATmega32U4-bootloader/blob/bf5d4d1edff529d5cc8229f15463720250c7bcd3/avr/cores/arduino/CDC.cpp#L99C14-L99C14
							wdt_enable(WDTO_15MS);
							for(;;);
						} break;

						case DIAGNOSTIC_BAUD_SIGNAL:
						{
							debug_usb_rx_diagnostic_signal = true;
						} break;
					}
				}
				else
				{
					UECONX |= (1 << STALLRQ);
				}
			} break;

			default:
			{
				error;
			} break;
		}
	}
}

#if DEBUG
static void
debug_tx_chars(char* value, u16 value_size)
{
	for (u16 i = 0; i < value_size; i += 1)
	{
		while (_usb_cdc_in_writer_masked(1) == _usb_cdc_in_reader_masked(0)); // Our write-cursor is before the interrupt's read-cursor.
		_usb_cdc_in_buffer[_usb_cdc_in_writer_masked(0)] = value[i];
		_usb_cdc_in_writer += 1;
	}
}
#endif

#if DEBUG
static void
debug_tx_cstr(char* value)
{
	debug_tx_chars(value, strlen(value));
}
#endif

#if DEBUG
static u8 // Amount of data copied into dst.
debug_rx(char* dst, u8 dst_max_length) // TODO Does windows/PuTTY really just send a /r on enter?
{
	u8 result = 0;

	while (result < dst_max_length && _usb_cdc_out_reader_masked(0) != _usb_cdc_out_writer_masked(0))
	{
		dst[result]          = _usb_cdc_out_buffer[_usb_cdc_out_reader_masked(0)];
		_usb_cdc_out_reader += 1;
		result              += 1;
	}

	return result;
}
#endif

#undef error

//
// Documentation.
//

/* [Overview].
	TODO
	TODO Mention [About: Endpoints].
*/

/* [USB Initialization Process].
	We carry out the following steps specified by (1):

		1. "Power on USB pads regulator".
			Based off of (2), the USB regulator simply supplies voltage to the USB data lines
			and the capacitor. So this first step is just as simple enabling UVREGE (3).

		2. "Configure PLL interface and enable PLL".
			PLL refers to a phase-locked-loop device. I don't know much about the mechanisms of
			the circuit, but it seems like you give it some input frequency and it'll output a
			higher frequency after it has synced up onto it.

			I've heard that using the default 16MHz clock signal is more reliable as the input to
			the PLL (TODO Citation!), especially when using full-speed USB connection. So that's
			what we'll be using.

			But we can't actually use the 16MHz frequency directly since the actual PLL unit itself
			expects 8MHz, so we'll need to half our 16MHz clock to get that. Luckily, there's a
			PLL clock prescaler as seen in (2) that just does this for us.

			From there, we enable the PLL and it'll output a 48MHz frequency by default, as
			shown in the default bits in (4) and table (5). We want 48MHz since this is what
			the USB interface expects as shown by (2).

		3. "Check PLL lock".
			We just wait until the PLL stablizes and is "locked" onto the expected frequency,
			which apparently can take some milliseconds according to (7).

		4. "Enable USB interface".
			The "USB interface" is synonymous with "USB device controller" or simply
			"USB controller" (8). We can enable the controller with the USBE bit in USBCON.

			But... based off of some experiments and the crude figure of the USB state machine
			at (9), the FRZCLK bit can only be cleared **AFTER** the USB interface has been
			enabled. FRZCLK halts the USB clock in order to minimize power consumption (10),
			but obviously the USB is also disabled, so we want to avoid that. As a result, we
			have to assign to USBCON again afterwards to be able to clear the FRZCLK bit.

			We will also set OTGPADE to be able to detect whether or not we are connected to the
			host (VBUS bit), and let the host know of our presence when we are plugged in (11).

		5. "Configure USB interface".
			We enable the End-of-Reset interrupt, so when the unenumerated USB device connects
			and the hosts sees this, the host will pull the data lines to a state to signal
			that the device should reset (as described by (1)), and when the host releases, the
			USB_GEN_vect is then triggered. It is important that we do some of our initializations
			after the End-of-Reset signal since (12) specifically states that all of our endpoints
			will end up disabled.

			We also enable the Start-of-Frame interrupt which is a signal that's sent by the host
			every frame (~1ms for full-speed USB). This is used to flush out data to be sent to the
			host or data to be received. The alternative would've been to have an interrupt for
			whenever the host wanted to do a transaction, but this happens so frequently that it
			really ends up bogging the main program down too much.

		6. "Wait for USB VBUS information connection".
			At this point, the rest of USB initialization will be done in the USB_GEN_vect interrupt
			routine. If we're in DEBUG mode, we'll wait for PuTTY to open up so we won't miss any
			diagnostic outputs.

	(1) "Power On the USB interface" @ Source(1) @ Section(21.12) @ Page(266).
	(2) "USB Controller Block Diagram Overview" @ Source(1) @ Figure(21-1) @ Page(256).
	(3) UHWCON, UVREGE @ Source(1) @ Section(21.13.1) @ Page(267).
	(4) PDIV @ Source(1) @ Section(6.11.6) @ Page(41).
	(5) PDIV Table @ Source(1) @ Section(6.11.6) @ Page(42).
	(7) PLOCK @ Source(1) @ Section(6.11.5) @ Page(41).
	(8) "USB interface" @ Source(1) @ Section(22.2) @ Page(270).
	(9) Crude USB State Machine @ Source(1) @ Figure (21-7) @ Page(260).
	(10) FRZCLK @ Source(1) @ Section(21.13.1) @ Page(267).
	(11) OTGPADE, VBUS, USBSTA @ Source(1) @ Section(21.13.1) @ Page(267-268).
	(12) "USB Reset" @ Source(1) @ Section(22.4) @ Page(271).
*/

/* [About: Endpoints].
	All USB devices have "endpoints" which is essentially a buffer where it can be
	written with data sent by the host and then read by the device, or written by the
	device to which it is then transmitted to the host.

	Endpoints are assigned a "transfer type" (1) where it is optimized for a specific
	purpose. For example, bulk-typed endpoints are great for transmitting large amounts
	of data (e.g. uploading a movie to a flash-drive) but there's no guarantee on when
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
	customizable, with each having their own limitation on the maximum capacity (3). The
	endpoint buffers are all manually allocated by the MCU in a single 832-byte
	arena (2). The ATmega32U4 also has the ability to "double-buffer" or "double-bank"
	or "ping-pong", meaning that an endpoint can essentially have two buffers in use at
	the same time. One will be written to or read from by the hardware, while the other
	is read from or written to by the firmware. This allows us to do something like
	operate on a data-packet sent from the host while, simultaneously, the bytes of the
	next data-packet is being copied by the hardware. The role of these two buffers
	would flip back and forth, hence ping-pong mode.

	(1) Endpoint Transfer Types @ Source(2) @ Table(9-13) @ Page(270) & Source(1) @ Section(22.18.2) @ Page(286) & Source(2) @ Section(4.7) @ Page(20-21).
	(2) Endpoint Memory @ Source(1) @ Section(21.9) @ Page(263-264).
	(3) Endpoint FIFO Limitations @ Source(1) @ Section(22.1) @ Page(270).
*/

/* [About: Packets and Transactions].
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

/* [USB Device Interrupt Routine].
	This interrupt routine is triggered whenever an event that's related to the USB controller (and
	not to the endpoints) has occured. This would be anything listed on (1):
		- VBUS Plug-In.
		- Upstream Resume.
		- End-of-Resume.
		- Wake Up.
		- End-of-Reset.
		- Start-of-Frame.
		- Suspension Detection.
		- Start-of-Frame CRC Error.

	When an unenumerated device connects to the USB and the host sees this, the host
	will pull the D+/D- lines to a specific state where it signals to the device that it
	should reset (4), and when the host releases the D+/D- lines, USB_GEN_vect is then triggered
	for the "End-of-Reset" event. We care about this End-of-Reset event since (1) specifically
	states that any endpoints we configure will just end up being disabled.

	The first endpoint that we will initialize is endpoint 0 (USB_ENDPOINT_DFLT), which we will do
	by consulting the flowchart in (1):

		1. "Select the endpoint".
			ATmega32U4 uses the system where writing to and reading from
			endpoint-specific registers (e.g. UECFG0X) depends on the currently
			selected endpoint determined by UENUM (3).

		2. "Activate endpoint."
			Self-explainatory. The only thing we have to be aware of is that we
			have to initiate endpoints in sequential order so the memory allocation
			works out (2).

		3. "Configure and allocate".
			We set the endpoint's transfer type, direction, buffer size, interrupts, etc.
			For anything related to memory, the hardware does it all on-the-fly (2).

			For UECFG0X, endpoint 0 will always be a control-typed endpoint as enforced
			by the USB specification, so that part of the configuration is straight-forward.
			For the data-direction of endpoint 0, it seems like the ATmega32U4 datasheet wants
			us to define endpoint 0 to have an "OUT" direction (5), despite the fact that
			control-typed endpoints are bidirectional.

			We also want from endpoint 0 the ability for it to detect when it has received a
			SETUP-transaction as it is where the rest of the enumeration will be communicated
			through, and USB_COM_vect will be the one to carry that out. Note that the interrupt
			USB_COM_vect is only going to trigger for endpoint 0. So if the host sends a
			SETUP-transaction to endpoint 3, it'll be ignored (unless the interrupt for that is
			also enabled) (6).

		4. "Test endpoint configuration".
			The allocation can technically fail for things like using more than
			832 bytes of memory or making a buffer larger than what that endpoint
			can support. This doesn't really need to be addressed, just as long we
			aren't dynamically reallocating, disabling, etc. endpoints at run-time.

	This interrupt routine also handle the Start-of-Frame event that the host sends to the device
	every ~1ms (for full-speed USB devices). This part of the interrupt is pretty much used for
	sending out buffered data or to copy an endpoint's buffer into our own ring buffer. An
	interrupt could be made for each specific endpoint within Start-of-Frame event to be instead
	triggered in USB_COM_vect, but these interrupts would be executed so often that it'll grind the
	MCU down to a halt.

	(1) "USB Device Controller Interrupt System" @ Source(1) @ Figure(22-4) @ Page(279).
	(2) Endpoint Memory Management @ Source(1) @ Section(21.9) @ Page(263-264).
	(3) "Endpoint Selection" @ Source(1) @ Section(22.5) @ Page(271).
	(4) "USB Reset" @ Source(1) @ Section(22.4) @ Page(271).
	(5) EPDIR @ Source(1) @ Section(22.18.2) @ Page(287).
	(6) "USB Device Controller Endpoint Interrupt System" @ Source(1) @ Figure(22-5) @ Page(280).
*/

/* [Endpoint 0: Data-Transfer from Device to Host].
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
	No data will actually be sent to the device since the data-packet in the OUT-transaction is
	always zero-length, i.e. empty.

	The host can "abort" early by sending an OUT-transaction before the device has
	completely sent all of its data. For example, the host might've asked for 1024 bytes
	of the string, but for some reason it needed to end the transferring early, so it
	sends an OUT-transaction (instead of another IN-transaction) even though the device
	has only sent, say, 256 bytes of the requested 1024 bytes.

	The ATmega32U4 actually seems to send a NACK-HANDSHAKE packet to the terminating
	OUT-transaction (2), and it is only the one after that (when the host attempts again to get an
	ACK) that the USB controller actually acknowledges it, but really this is just an insignificant
	implementation detail if anything.

	(1) Control Transfers @ Source(2) @ Section(8.5.3) @ Page(226).
	(2) Control Read Timing Diagram @ Source(2) @ Section(22.12.2) @ Page(275).
*/

/* [USB Endpoint Interrupt Routine].
	This interrupt routine is where the meat of all the enumeration is taken place. It handles
	any events related to endpoints, all listed in (1):
		- An endpoint's buffer is ready to be filled with data to be sent to the host.
		- An endpoint's buffer is filled with data sent by the host.
		- A control-typed endpoint received a SETUP-transaction.
		- An endpoint replied with a STALL-HANDSHAKE packet.
		- An isochronous endpoint detected a CRC mismatch.
		- An isochronous endpoint underwent overflow/underflow of data.
		- An endpoint replied with a NACK-HANDSHAKE packet.

	(1) "USB Device Controller Endpoint Interrupt System" @ Source(1) @ Figure(22-5) @ Page(280).
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
	STALL-HANDSHAKE packet as a response. See: [Endpoint 0: Request Error].
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

/* [CDC - Communication Class Device].
	The USB communication devices class is designed for all sorts of... well... communication
	devices... such as modems, telephones, ethernet hubs, etc. The reason why we implement
	this class for the device is so we can transmit data to the host for diagnostic
	purposes.

	Although not super important, it's nice to begin building our understanding of CDC by rewinding
	far back into the history of a communication systems, starting at the age old thing called
	POTS (Plain Old Telephone Service):
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

	Thus, (from my limited understanding of how this CDC implementation is done) we can essentially
	use CDC to mimick our device to be something like a modem and serially transmit data to the host
	(the fact we aren't really a modem that's working with analog signals doesn't really matter).
	We are going to define a couple interfaces for CDC later on, so we will need to declare
	the entire device as being a communication device in USB_DEVICE_DESCRIPTOR as mentioned in (5).

	As stated earlier, there are many communication devices that the CDC specification
	wants to be compatiable with. The one that will be the most applicable for us is the
	"Abstract Control Model" as seen in (4). Based on the figure, the specification
	is assuming we are some sort of device that has components for data-compression,
	error-correction, etc. just like a modem might have. But a lot of this doesn't actually really
	matter at all; we're still sending pure binary data-packets to the host via USB.

	Within our USB_CONFIGURATION_HIERARCHY, we define the "Communication Class Interface".
	This interface is responsible for "device management" and "call management", and is required
	for all communication devices, according to (1). As for what device and call management
	entails:

		- "Device Management" is any operation the host would want to do to "control
		and configure the operational state of the device" (3). These operations would
		be carried out by the "management element" which is just a fancy name for
		endpoint 0 (2). The host would send CDC-specific requests to endpoint 0 for
		things like "ringing the auxillary phone jack", all of which is detailed in (6).
		Once again, we can ignore a lot of these commands.

		- "Call Management" is "responsible for setting up and tearing down of calls"
		(7). As to what specific things are done in call management, I'm actually not
		too sure. The specification wasn't too clear on this, but I think it's
		more-or-less a subset of the management element requests for things like hanging
		up phones (SET_HOOK_STATE) or dialing numbers (DIAL_DIGITS). This is further
		supported by the fact that an example configuration in (8) states that the
		"management element will transport both call management element and device
		management element commands". But in the end it doesn't matter again; we're
		going to never really encounter any of these commands.

	The CDC interface is also apparently required (9) to have the "notification element" which
	simply lets the host know of certain events like whether or not "the phone is hooked or not"
	(10). Like with the management element, the notification element is just another endpoint,
	but this time it's not endpoint 0, and is instead usually some other interrupt-typed endpoint
	(11), or maybe a bulk-typed endpoint (12). Although the specification is worded where it
	states that the abstract control model is supposed to have a minimum of two pipes (endpoint 0
	for management element and some other endpoint for the notification element), it seems like
	Windows and PuTTY is completely fine without the notification element. So this isn't exactly
	standard compliant, I will admit, but CDC is implemented for this device for diagnostics and
	easy reprogramming, so does it really matter? Besides, even if I did waste an entire endpoint
	just for it to do nothing, how can we be confident that everything else coded in this repo is
	standard compliant? Do you even know what the hell "Data Compression (V.42bis)" even mean in
	(19)? No? Then don't complain. If you do know what it means, then please email me:
	phucxdoan@gmail.com. I'd be happy to learn!

	Within the CDC interface itself is a list of "functional descriptors" (13) which are
	proprietary to the CDC specification. Each functional descriptor just state some small piece of
	information related to the communication device we are using for the host:

		- "Header Functional Descriptor" (14).
			Always the first functional descriptor and it only lists the CDC specification we are
			using. That's all there is to it.

		- "Call Management Functional Descriptor" (15).
			This just tells the host whether or not the device can handle call management itself,
			and if call management commands can be transferred over the endpoints that we'll be
			using for transferring diagnostics. Doesn't seem relevant at all for actually making
			the CDC function, but still nonetheless needs to exist for some reason.

		- "Abstract Control Management Functional Descriptor" (16).
			Like with "Call Management Functional Descriptor", this one just tells the host what
			commands and notifications it can expect from the device. Seems to also not be very
			relevant for making CDC functional, but also needs to be defined for the enumeration to
			be successful.

		- "Union Functional Descriptor" (17).
			This descriptor is probably the most useful one compared to the previous three; it is
			used to group all the interfaces that contribute into creating the communication
			functionality altogether (18). This is probably more apparently useful in communication
			devices that involve things like audio and video. Without this, the host probably can't
			determine which CDC-interfaces would go with which other interfaces that are used to
			transfer the communication itself.

	After all that, we then define the CDC-data class (20). This one seems pretty straight-forward;
	we just kinda specify some endpoints that'll be used for transferring data to and from the
	host. It can be used for sending raw data, or some sort of structured format that the
	specification didn't define. In any case, this is where we define the endpoints that'll be
	transmitting diagnostics and to receive input from the host.

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
	(18) "Communication Device Management" @ Source(6) @ Section(3.1.1) @ AbsPage(19-20).
	(19) "Data Compression (V.42bis)" @ Source(6) @ Figure(3) @ AbsPage(26).
	(20) "Data Class Interface" @ Source(6) @ Section(3.3.2) @ AbsPage(21).
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
	transmitted. See: [About: Packets and Transactions]). Thus, if we set ADDEN to enable the
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
