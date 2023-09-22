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
{ // [USB Device Interrupt Routine and Endpoint 0 Configuration].

	// "Select the endpoint".
	UENUM = 0;

	// "Activate endpoint".
	UECONX = (1 << EPEN);

	// "Configure" and "Allocate".
	UECFG1X = (USB_ENDPOINT_0_SIZE_TYPE << EPSIZE0) | (1 << ALLOC);

	// "Test endpoint configuration".
	#if 0
	if (!(UESTA0X & (1 << CFGOK)))
	{
		debug_halt(-1);
	}
	#endif

	UEIENX = (1 << RXSTPE); // Endpoint 0 will listen to the reception of SETUP-transactions.

	UDINT = 0; // Clear End-of-Reset trigger flag to prevent this routine from executing again.
}

static void
_usb_endpoint_0_transmit // [Endpoint 0: Data-Transfer from Device to Host].
(
	u8* packet_data,
	u16 packet_length,
	u16 endpoint_buffer_size
)
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
				data_remaining < endpoint_buffer_size
					? data_remaining
					: endpoint_buffer_size;

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
		u8  packet_length = 0; // Packets will not exceed 255 bytes.
		u8* packet_data   = 0;

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
					packet_data   = (u8*) &USB_DEVICE_DESCRIPTOR;
					packet_length = sizeof(USB_DEVICE_DESCRIPTOR);
					static_assert(sizeof(USB_DEVICE_DESCRIPTOR) < (((u64) 1) << (sizeof(packet_length) * 8)))
				} break;

				case USBDescriptorType_configuration:
				{
					packet_data   = (u8*) &USB_CONFIGURATION_HIERARCHY;
					packet_length = sizeof(USB_CONFIGURATION_HIERARCHY);
					static_assert(sizeof(USB_CONFIGURATION_HIERARCHY) < (((u64) 1) << (sizeof(packet_length) * 8)))
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

		if (packet_length)
		{
			_usb_endpoint_0_transmit
			(
				packet_data,
				setup_packet.get_descriptor.wLength < packet_length
					? setup_packet.get_descriptor.wLength
					: packet_length,
				SIZEOF_ENDPOINT_SIZE_TYPE(USB_ENDPOINT_0_SIZE_TYPE)
			);
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
		switch (setup_packet.set_configuration.configuration_value)
		{
			case 0:
			case 1:
			{
				// We either move to or remain at the "address state" of the device,
				// but since we only have one configuration, it doesn't really matter;
				// we don't have to do anything on our side here.
			} break;

			default:
			{
				UECONX |= (1 << STALLRQ); // See: "Address State" @ Source(2) @ Section(9.4.7) @ Page(257).
			} break;
		}
	}
	else if // [SETUP Communication GetLineCoding].
	(
		setup_packet.unknown.bmRequestType == 0b1010'0001
		&& setup_packet.unknown.bRequest == USBSetupRequest_communication_get_line_coding
	)
	{
		if
		(
			setup_packet.communication_get_line_coding.interface_index // This should for the only CDC interface. See: USB_CONFIGURATION_HIERARCHY.
			&& setup_packet.communication_get_line_coding.structure_size != sizeof(struct USBCommunicationLineCoding) // Probably fine without this line.
		)
		{
			error_halt();
		}

		_usb_endpoint_0_transmit
		(
			(u8*) &USB_COMMUNICATION_LINE_CODING,
			sizeof(USB_COMMUNICATION_LINE_CODING),
			SIZEOF_ENDPOINT_SIZE_TYPE(USB_ENDPOINT_0_SIZE_TYPE)
		);
	}
	else if // [SETUP Communication SetControlLineState].
	(
		setup_packet.unknown.bmRequestType == 0b0010'0001
		&& setup_packet.unknown.bRequest == USBSetupRequest_communication_set_control_line_state
	)
	{
		while (!(UEINTX & (1 << TXINI)));
		UEINTX &= ~(1 << TXINI); // TODO Optimize?
	}
	else if // [SETUP Communication SetLineCoding].
	(
		setup_packet.unknown.bmRequestType == 0b0010'0001
		&& setup_packet.unknown.bRequest == USBSetupRequest_communication_set_line_coding
	)
	{
		// TODO Is it possible for the host to abort sending data?

		static_assert(sizeof(struct USBCommunicationLineCoding) < SIZEOF_ENDPOINT_SIZE_TYPE(USB_ENDPOINT_0_SIZE_TYPE));
		while (!(UEINTX & (1 << RXOUTI)));
		struct USBCommunicationLineCoding line_coding;
		for (u8 i = 0; i < sizeof(line_coding); i += 1)
		{
			((u8*) &line_coding)[i] = UEDATX;
		}
		UEINTX &= ~(1 << RXOUTI);

		while (!(UEINTX & (1 << TXINI)));
		UEINTX &= ~(1 << TXINI);

		if (memcmp(&line_coding, &USB_COMMUNICATION_LINE_CODING, sizeof(line_coding)))
		{
			debug_halt(-1); // TODO Can we ignore the setting of line-coding?
		}
	}
	else if
	(
		setup_packet.unknown.bmRequestType == 0b0000'0001
		&& setup_packet.unknown.bRequest == USBSetupRequest_clear_feature
	)
	{
		debug_halt(5);
	}
	else // Unhandled SETUP command.
	{
		debug_u8(setup_packet.unknown.bRequest);
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

/* [USB Device Interrupt Routine and Endpoint 0 Configuration].
	See: [Endpoints].

	Triggers listed on (1):
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
	We enable the interrupt for this by using RXSTPE.

	Note that this interrupt is only going to trigger for endpoint 0. So if the host
	sends a SETUP-transaction to endpoint 3, it'll be ignored (unless the interrupt for
	that is also enabled) (6).

	(1) USB Device Controller Interrupt System @ Source(1) @ Figure(22-4) @ Page(279).
	(2) Endpoint Memory Management @ Source(1) @ Section(21.9) @ Page(263-264).
	(3) "Endpoint Selection" @ Source(1) @ Section(22.5) @ Page(271).
	(4) "USB Reset" @ Source(1) @ Section(22.4) @ Page(271).
	(5) EPDIR @ Source(1) @ Section(22.18.2) @ Page(287).
	(6) "USB Device Controller Endpoint Interrupt System" @ Source(1) @ Figure(22-5) @ Page(280).
*/

/* [Communication Between Host and Device].
	A packet is the smallest coherent piece of message that is sent to-or-from host and
	device. The layout of a packet is abstractly structured as so:

		............................
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
	: ----------------------     ---------------------     -------------------------- :
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
	packet stage.

	The HANDSHAKE-typed packet is the final packet that is sent either by the host or
	device (or even not at all). The direction of the transfer of this packet is
	opposite of the DATA-typed packet (e.g. if the host sent the data, then the device
	is the one that responds here). The HANDSHAKE-typed packet is often used by the
	recipent of the data to ACK or NACK the data that was sent. If the device (and only
	the device) is in a state where it cannot not handle this transaction, then the
	device can transmit a STALL packet for the handshake.

	Once again, the layout of a transaction varies case-by-case. For example,
	isochronous endpoints will engage in transactions with the host where there will
	never be a HANDSHAKE-typed packet. This is because the entire point of isochronous
	endpoints is to transmit time-sensitive data quickly (such as a live video or
	song), but if a DATA-typed packet was dropped here and there, then its no big deal;
	there's no need to waste USB bandwidth on handshakes.

	The most common transactions are SETUPs, INs, and OUTs.

		- SETUP-transactions are where the host sends the device a DATA-typed packet
		that specifies what the host wants (see: struct USBSetupPacket). This could be
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
	See: [Communication Between Host and Device].

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
	............ EXAMPLE OF ENDPOINT 0 SENDING DATA TO THE HOST ...........

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





























//
// TODO Revise.
//




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

/* [SETUP Communication GetLineCoding].
	TODO
*/

/* [SETUP Communication SetControlLineState].
	TODO
*/

/* [SETUP Communication SetLineCoding].
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
