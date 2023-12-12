/*
	"HELD"   : Must evaluate to 0 or 1.
	"DEST_X" : Must be within [0, 127].
	"DEST_Y" : Must be within [0, 255].
	See: [Mouse Commands].
*/
#define usb_mouse_command(HELD, DEST_X, DEST_Y) usb_mouse_command_((((u16) HELD) << 15) | (((u16) DEST_X) << 8) | ((u16) DEST_Y))
static void
usb_mouse_command_(u16 command)
{
	#if USB_HID_ENABLE
		while (_usb_mouse_command_writer_masked(1) == _usb_mouse_command_reader_masked(0)); // Our write-cursor is before the interrupt's read-cursor.
		_usb_mouse_command_buffer[_usb_mouse_command_writer_masked(0)] = command;
		_usb_mouse_command_writer += 1;
	#endif
}

static void
usb_init(void)
{ // [USB Initialization Process].

	#ifdef PIN_USB_BUSY
	pin_output(PIN_USB_BUSY);
	#endif

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
}

ISR(USB_GEN_vect) // [USB Device Interrupt Routine].
{
	#ifdef PIN_USB_BUSY
	pin_high(PIN_USB_BUSY);
	#endif

	if (UDINT & (1 << EORSTI)) // End-of-Reset.
	{
		for (u8 endpoint_index = 0; endpoint_index < countof(USB_ENDPOINT_UECFGNX); endpoint_index += 1)
		{
			if (pgm_u8(USB_ENDPOINT_UECFGNX[endpoint_index][1])) // Elements that aren't explicitly assigned in USB_ENDPOINT_UECFGNX will have the ALLOC bit cleared.
			{
				UENUM  = endpoint_index; // "Select the endpoint".
				UECONX = (1 << EPEN);    // "Activate endpoint".

				// "Configure and allocate".
				UECFG0X = pgm_u8(USB_ENDPOINT_UECFGNX[endpoint_index][0]);
				UECFG1X = pgm_u8(USB_ENDPOINT_UECFGNX[endpoint_index][1]);

				// "Test endpoint configuration".
				#if DEBUG
				if (!(UESTA0X & (1 << CFGOK)))
				{
					debug_unhandled(); // Invald configuration.
				}
				#endif
			}
		}

		// Enable the interrupt source for the event that endpoint 0 receives a SETUP-transaction.
		UENUM  = USB_ENDPOINT_DFLT_INDEX;
		UEIENX = (1 << RXSTPE);

		#if USB_MS_ENABLE
		// Enable the interrupt source for the event that mass storage's BULK-OUT endpoint receives (hopefully) a command wrapper.
		UENUM  = USB_ENDPOINT_MS_OUT_INDEX;
		UEIENX = (1 << RXOUTE);
		#endif

		#if DEBUG
		debug_usb_is_on_host_machine = false; // For when switching from PC to phone or vice-versa.
		#endif
	}

	if (UDINT & (1 << SOFI)) // Start-of-Frame.
	{
#if DEBUG && USB_CDC_ENABLE
		UENUM = USB_ENDPOINT_CDC_IN_INDEX;
		if (UEINTX & (1 << TXINI)) // Endpoint's buffer is ready to be filled up with data to send to the host.
		{
			while
			(
				(UEINTX & (1 << RWAL)) &&                                              // Endpoint's buffer still has some space left.
				debug_usb_cdc_in_reader_masked(0) != debug_usb_cdc_in_writer_masked(0) // Our ring buffer still has some data left.
			)
			{
				UEDATX                   = debug_usb_cdc_in_buffer[debug_usb_cdc_in_reader_masked(0)];
				debug_usb_cdc_in_reader += 1;
			}

			UEINTX &= ~(1 << TXINI);   // Must be cleared first before FIFOCON. See: Source(1) @ Section(22.14) @ Page(276).
			UEINTX &= ~(1 << FIFOCON); // Allow the USB controller to send the data for the next IN-transaction. See: Source(1) @ Section(22.14) @ Page(276)
		}
#endif

#if DEBUG && USB_CDC_ENABLE
		UENUM = USB_ENDPOINT_CDC_OUT_INDEX;
		if (UEINTX & (1 << RXOUTI)) // Endpoint's buffer has data from the host to be copied.
		{
			while
			(
				(UEINTX & (1 << RWAL)) &&                                                // Endpoint's buffer still has some data left to be copied.
				debug_usb_cdc_out_writer_masked(1) != debug_usb_cdc_out_reader_masked(0) // Our ring buffer still has some space left.
			)
			{
				debug_usb_cdc_out_buffer[debug_usb_cdc_out_writer_masked(0)] = UEDATX;
				debug_usb_cdc_out_writer += 1;
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
				UEINTX &= ~(1 << FIFOCON); // Free up this endpoint's buffer so the USB controller can copy the data in the next OUT-transaction to it.
			}
		}
#endif

#if USB_HID_ENABLE
		UENUM = USB_ENDPOINT_HID_INDEX;
		if (UEINTX & (1 << TXINI)) // See: [Mouse Commands].
		{
			i8 delta_x = 0;
			i8 delta_y = 0; // Positive makes the mouse move downward.

			#if DEBUG
			if (debug_usb_is_on_host_machine)
			{
				// Ignore commands to make programming on host machine not be a hassle.
				_usb_mouse_command_reader = _usb_mouse_command_writer;
			}
			else
			#endif
			if (_usb_mouse_calibrations < USB_MOUSE_CALIBRATIONS_REQUIRED) // Calibrate the mouse to a known origin.
			{
				if (_usb_mouse_calibrations < USB_MOUSE_CALIBRATIONS_REQUIRED * 3 / 4)
				{
					delta_x = -128;
					delta_y = -128;
				}
				_usb_mouse_calibrations += 1;
			}
			else if (_usb_mouse_command_reader_masked(0) != _usb_mouse_command_writer_masked(0)) // There's an available command to handle.
			{
				u16 command        = _usb_mouse_command_buffer[_usb_mouse_command_reader_masked(0)];
				b8  command_held   = (command >> 15);
				u8  command_dest_x = (command >>  8) & 0b0111'1111;
				u8  command_dest_y =  command        & 0b1111'1111;

				if (_usb_mouse_held != command_held)
				{
					_usb_mouse_held = command_held;
				}
				else if (!command)
				{
					_usb_mouse_curr_x       = 0;
					_usb_mouse_curr_y       = 0;
					_usb_mouse_calibrations = 0;
				}
				else if (_usb_mouse_curr_x != command_dest_x && (_usb_mouse_curr_y == command_dest_y || ((_usb_mouse_curr_x ^ _usb_mouse_curr_y) & 1)))
				{
					if (_usb_mouse_curr_x < command_dest_x)
					{
						delta_x = 1;
					}
					else
					{
						delta_x = -1;
					}
				}
				else if (_usb_mouse_curr_y < command_dest_y)
				{
					delta_y = 1;
				}
				else if (_usb_mouse_curr_y > command_dest_y)
				{
					delta_y = -1;
				}

				_usb_mouse_curr_x += delta_x;
				_usb_mouse_curr_y += delta_y;

				if (_usb_mouse_curr_x == command_dest_x && _usb_mouse_curr_y == command_dest_y) // We are at the destination.
				{
					_usb_mouse_command_reader += 1; // Free up the mouse command.
				}
			}

			// See: USB_DESC_HID_REPORT.
			UEDATX = _usb_mouse_held;
			UEDATX = delta_x;
			UEDATX = delta_y;

			UEINTX &= ~(1 << TXINI);   // Must be cleared first before FIFOCON. See: Source(1) @ Section(22.14) @ Page(276).
			UEINTX &= ~(1 << FIFOCON); // Allow the USB controller to send the data for the next IN-transaction. See: Source(1) @ Section(22.14) @ Page(276)
		}
#endif
	}

	UDINT = 0; // Clear interrupt flags to prevent this routine from executing again.

	#ifdef PIN_USB_BUSY
	pin_low(PIN_USB_BUSY);
	#endif
}

#if USB_MS_ENABLE
	static void
	_usb_ms_receive_sector(void)
	{
		u16 sector_byte_index = 0;
		while (sector_byte_index < countof(sd_sector))
		{
			//
			// Wait for chunk of the sector data from the host.
			//

			while (!(UEINTX & (1 << RXOUTI)));

			//
			// Copy the chunk of the sector data from host.
			//

			if (UEBCX != USB_ENDPOINT_MS_OUT_SIZE)
			{
				error(); // Payloads are varying in lengths!
			}

			while (UEINTX & (1 << RWAL))
			{
				sd_sector[sector_byte_index]  = UEDATX;
				sector_byte_index            += 1;
			}

			//
			// Flush the endpoint buffer.
			//

			UEINTX &= ~(1 << RXOUTI);
			UEINTX &= ~(1 << FIFOCON);
		}
	}
#endif

ISR(USB_COM_vect) // [USB Endpoint Interrupt Routine].
{
	#ifdef PIN_USB_BUSY
	pin_high(PIN_USB_BUSY);
	#endif

	UENUM = USB_ENDPOINT_DFLT_INDEX;
	if (UEINTX & (1 << RXSTPI)) // [Endpoint 0: SETUP-Transactions].
	{
		UECONX |= (1 << STALLRQC); // SETUP-transactions lift STALL conditions. See: Source(2) @ Section(8.5.3.4) @ Page(228) & [Endpoint 0: Request Error].

		struct USBSetupRequest request;
		for (u8 i = 0; i < sizeof(request); i += 1)
		{
			((u8*) &request)[i] = UEDATX;
		}
		UEINTX = ~(1 << RXSTPI);

		switch (request.kind)
		{
			// [Endpoint 0: GetDescriptor].
			case USBSetupRequestKind_get_desc:
			case USBSetupRequestKind_hid_get_desc:
			{
				u8        payload_length = 0; // Any payload we send will not exceed 255 bytes.
				const u8* payload_data   = 0;

				if (request.kind == USBSetupRequestKind_get_desc)
				{
					switch ((enum USBDescType) request.get_desc.desc_type)
					{
						case USBDescType_device: // See: [Endpoint 0: GetDescriptor].
						{
							payload_data   = (const u8*) &USB_DESC_DEVICE;
							payload_length = sizeof(USB_DESC_DEVICE);
							static_assert(sizeof(USB_DESC_DEVICE) < (((u64) 1) << bitsof(payload_length)));
						} break;

						case USBDescType_config: // [Interfaces, Configurations, and Classes].
						{
							if (!request.get_desc.desc_index) // We only have a single configuration.
							{
								payload_data   = (const u8*) &USB_CONFIG;
								payload_length = sizeof(USB_CONFIG);
								static_assert(sizeof(USB_CONFIG) < (((u64) 1) << bitsof(payload_length)));
							}
						} break;

						case USBDescType_device_qualifier: // See: Source(2) @ Section(9.6.2) @ Page(264).
						case USBDescType_string:           // See: [USB Strings] @ File(defs.h).
						{
							// We induce STALL condition later on. See: [Endpoint 0: Request Error].
						} break;

						default:
						{
							// We currently don't handle any other standard requests, but if needed we could.
							#if DEBUG
							debug_unhandled();
							#endif
						} break;
					}
				}
				#if USB_HID_ENABLE
				else if
				(
					request.hid_get_desc.desc_type                  == USBDescType_hid_report &&
					request.hid_get_desc.designated_interface_index == USBConfigInterface_hid
				)
				{
					payload_data   = (const u8*) &USB_DESC_HID_REPORT;
					payload_length = sizeof(USB_DESC_HID_REPORT);
					static_assert(sizeof(USB_DESC_HID_REPORT) < (((u64) 1) << bitsof(payload_length)));
				}
				#endif
				else
				{
					// We currently don't handle any other requests, but if needed we could.
					#if DEBUG
					debug_unhandled();
					#endif
				}

				if (payload_length) // [Endpoint 0: Data-Transfer from Device to Host].
				{
					const u8* payload_reader    = payload_data;
					u8        payload_remaining = payload_length;

					if (payload_length > request.get_desc.requested_amount)
					{
						payload_remaining = request.get_desc.requested_amount;
					}

					while (true)
					{
						if (UEINTX & (1 << RXOUTI)) // Received OUT-transaction?
						{
							UEINTX &= ~(1 << RXOUTI); // The OUT-transaction should be zero-lengthed, so discard immediately.
							break;
						}
						else if (UEINTX & (1 << TXINI)) // Endpoint 0's buffer ready to be filled?
						{
							u8 packet_size = payload_remaining;

							if (packet_size > USB_ENDPOINT_DFLT_SIZE)
							{
								packet_size = USB_ENDPOINT_DFLT_SIZE;
							}

							for (u8 i = 0; i < packet_size; i += 1)
							{
								UEDATX             = pgm_u8(*payload_reader);
								payload_remaining -= 1;
								payload_reader    += 1;
							}

							UEINTX &= ~(1 << TXINI); // Transmit endpoint 0's buffer.
						}
					}
				}
				else
				{
					UECONX |= (1 << STALLRQ); // See: [Endpoint 0: Request Error].
				}
			} break;

			case USBSetupRequestKind_set_address: // [Endpoint 0: SetAddress].
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
					UECONX |= (1 << STALLRQ); // The host somehow sent us an address that's not within 7-bits. See: Source(2) @ Section(8.3.2.1) @ Page(197).
				}
			} break;

			case USBSetupRequestKind_set_config: // [Endpoint 0: SetConfiguration].
			{
				switch (request.set_config.id)
				{
					#if DEBUG
					case 0:
					{
						debug_unhandled(); // In the case that the host, for some reason, wants to set the device back to the "address state", we should handle this.
					} break;
					#endif

					case USB_CONFIG_ID:
					{
						UEINTX &= ~(1 << TXINI); // Send out zero-length data-packet for the host's upcoming IN-transaction to acknowledge this request.
						while (!(UEINTX & (1 << TXINI)));
					} break;

					default:
					{
						UECONX |= (1 << STALLRQ);
					} break;
				}
			} break;

			#if USB_CDC_ENABLE
			case USBSetupRequestKind_cdc_set_line_coding: // [Endpoint 0: CDC-Specific SetLineCoding].
			{
				if (request.cdc_set_line_coding.incoming_line_coding_datapacket_size == sizeof(struct USBCDCLineCoding))
				{
					while (!(UEINTX & (1 << RXOUTI))); // Wait for the completion of an OUT-transaction.

					// Copy data-packet of OUT-transaction.
					struct USBCDCLineCoding desired_line_coding = {0};
					for (u8 i = 0; i < sizeof(struct USBCDCLineCoding); i += 1)
					{
						((u8*) &desired_line_coding)[i] = UEDATX;
					}

					// These two lines can't be combined. I'm guessing because the USB hardware will get confused and think whatever's in
					// endpoint 0's buffer from the OUT-transaction is what we want to send to the host for the IN-transaction.
					UEINTX &= ~(1 << RXOUTI); // Let the hardware know that we finished copying the OUT-transaction's data-packet.
					UEINTX &= ~(1 << TXINI);  // Send a zero-length data-packet for the upcoming IN-transaction to acknowledge the host's SetLineCoding request.

					switch (desired_line_coding.dwDTERate)
					{
						case BOOTLOADER_BAUD_SIGNAL:
						{
							*(volatile u16*) 0x0800 = 0x7777;
							restart();
						} break;

						default:
						{
							// Nothing particularly interesting about this baud rate.
						} break;
					}
				}
				else
				{
					UECONX |= (1 << STALLRQ);
				}
			} break;
			#endif

			case USBSetupRequestKind_endpoint_clear_feature: // See: "Clear Feature" @ Source(2) @ Section(9.4.1) @ Page(252).
			{
				if (request.endpoint_clear_feature.feature_selector == USBFeatureSelector_endpoint_halt)
				{
					UEINTX &= ~(1 << TXINI);
					while (!(UEINTX & (1 << TXINI))); // Acknowledge this request.

					if (request.endpoint_clear_feature.endpoint) // Endpoint 0 does not need any special handling.
					{
						u8 endpoint_index = request.endpoint_clear_feature.endpoint & ~USBEndpointAddressFlag_in;
						if
						(
							endpoint_index < countof(USB_ENDPOINT_UECFGNX) &&                          // Endpoint index within bounds?
							pgm_u8(USB_ENDPOINT_UECFGNX[endpoint_index][1]) &&                 // Endpoint exists?
							!!(pgm_u8(USB_ENDPOINT_UECFGNX[endpoint_index][0]) & (1 << EPDIR)) // Endpoint has the right transfer direction?
								== !!(request.endpoint_clear_feature.endpoint & USBEndpointAddressFlag_in)
						)
						{
							UENUM  = endpoint_index;
							UECONX = (1 << STALLRQC) | (1 << RSTDT) | (1 << EPEN); // Clear stall condition, reset data-toggle to DATA0. See: Source(2) @ Section(9.4.5) @ Page(256).
							UERST  = (1 << endpoint_index);                        // Set to begin resetting FIFO state machine on the endpoint.
							UERST  = 0;                                            // Clear to finish resetting FIFO.
						}
						else
						{
							UECONX |= (1 << STALLRQ); // An invalid endpoint was requested.
						}
					}
				}
				else
				{
					UECONX |= (1 << STALLRQ); // The standard USB only defines one feature for endpoints.
				}
			} break;

			case USBSetupRequestKind_ms_get_max_lun       : // We send back a single zero byte. See: Source(12) @ Section(3.2) @ Page(7).
			case USBSetupRequestKind_interface_get_status : // We send back two zero bytes. See: Standard "Get Status" on Interfaces @ Source(2) @ Section(9.4.5) @ Page(254).
			{
				// USBSetupRequestKind_ms_get_max_lun:
				//     Despite the fact that the mass storage specification specifically states that we may stall if we
				//     don't support multiple logical units, Apple does not seem to like it, so we're going to implement it anyways.

				b8 sent = false;
				while (true) // See: [Endpoint 0: Data-Transfer from Device to Host].
				{
					if (UEINTX & (1 << RXOUTI))
					{
						UEINTX &= ~(1 << RXOUTI);
						break;
					}
					else if (UEINTX & (1 << TXINI))
					{
						if (!sent)
						{
							UEDATX = 0;

							if (request.kind == USBSetupRequestKind_interface_get_status)
							{
								UEDATX = 0;
							}

							sent = true;
						}

						UEINTX &= ~(1 << TXINI);
					}
				}
			} break;

			case USBSetupRequestKind_cdc_get_line_coding:        // [Endpoint 0: Extraneous CDC-Specific Requests].
			case USBSetupRequestKind_cdc_set_control_line_state: // [Endpoint 0: Extraneous CDC-Specific Requests].
			case USBSetupRequestKind_hid_set_idle:               // [Endpoint 0: HID-Specific SetIdle].
			{
				UECONX |= (1 << STALLRQ);

				#if DEBUG
				debug_usb_is_on_host_machine = true;
				#endif
			} break;

			default:
			{
				UECONX |= (1 << STALLRQ);
				#if DEBUG
				debug_unhandled();
				#endif
			} break;
		}
	}

#if USB_MS_ENABLE // [Mass Storage Bulk-Only Transfer Communication].

	//
	// Handle command.
	//

	UENUM = USB_ENDPOINT_MS_OUT_INDEX;
	if (!_usb_ms_send_status && (UEINTX & (1 << RXOUTI)))
	{
		//
		// Fetch command.
		//

		struct USBMSCommandBlockWrapper command = {0};
		static_assert(sizeof(command) <= USB_ENDPOINT_MS_OUT_SIZE);

		if (UEBCX != sizeof(command)) // We supposedly received a CBW packet that's not 31 bytes in length. See: Source(12) @ Section(6.2.1) @ Page(17).
		{
			error();
		}

		for (u8 i = 0; i < sizeof(command); i += 1)
		{
			((u8*) &command)[i] = UEDATX;
		}
		UEINTX &= ~(1 << RXOUTI);
		UEINTX &= ~(1 << FIFOCON);

		if // Is the CBW valid and meaningful? See: Source(12) @ Section(6.2) @ Page(17).
		(
			!(
				command.dCBWSignature == USB_MS_COMMAND_BLOCK_WRAPPER_SIGNATURE &&
				!(command.bmCBWFlags & 0b0111'1111) &&
				command.bCBWLUN == 0 &&
				1 <= command.bCBWCBLength && command.bCBWCBLength <= 16
			)
		)
		{
			error();
		}

		//
		// Execute command.
		//

		_usb_ms_send_status    = true;
		_usb_ms_status.dCSWTag = command.dCBWTag;

		if
		(
			command.CBWCB[0] == USBMSSCSIOpcode_read || // See: Source(14) @ Table(28) @ Page(48).
			command.CBWCB[0] == USBMSSCSIOpcode_write   // See: Source(14) @ Table(62) @ Page(79).
		)
		{
			// Pretty irrelevant fields.
			u8 rd_wr_protect                 = (command.CBWCB[1] >> 5) & 0b111;
			b8 disable_page_out              = (command.CBWCB[1] >> 4) & 0b1;
			b8 force_unit_access             = (command.CBWCB[1] >> 3) & 0b1;
			b8 force_unit_access_nonvolatile = (command.CBWCB[1] >> 1) & 0b1;
			u8 group_number                  = command.CBWCB[6] & 0b11111;
			u8 control                       = command.CBWCB[9];

			u32 transaction_sector_count         = ((command.CBWCB[7] << 8) | command.CBWCB[8]);
			u32 transaction_sector_start_address =
					(((u32) command.CBWCB[2]) << 24) |
					(((u32) command.CBWCB[3]) << 16) |
					(((u32) command.CBWCB[4]) <<  8) |
					(((u32) command.CBWCB[5]) <<  0);

			if
			(
				!(
					command.dCBWDataTransferLength == transaction_sector_count * FAT32_SECTOR_SIZE &&
					(command.CBWCB[0] == USBMSSCSIOpcode_read) == !!command.bmCBWFlags && // Ensures data transaction direction matches.
					command.bCBWCBLength == 10
				)
			)
			{
				error(); // Command wrapper does not match the actual wrapped command.
			}

			if (!(!rd_wr_protect && !disable_page_out && !force_unit_access && !force_unit_access_nonvolatile && !control))
			{
				error(); // The common values for the fields are different for some reason; we should probably handle it if this happens.
			}

			if (command.CBWCB[0] == USBMSSCSIOpcode_read)
			{
				UENUM = USB_ENDPOINT_MS_IN_INDEX;
				for (u32 sector_index = 0; sector_index < transaction_sector_count; sector_index += 1)
				{
					sd_read(transaction_sector_start_address + sector_index);

					u16 sector_byte_index = 0;
					while (sector_byte_index < countof(sd_sector))
					{
						//
						// Wait for endpoint buffer to be free.
						//

						while (!(UEINTX & (1 << TXINI)));

						//
						// Fill endpoint buffer with chunk of the sector data to send to the host.
						//

						static_assert(FAT32_SECTOR_SIZE % USB_ENDPOINT_MS_IN_SIZE == 0); // This allows us to now worry about each chunk payload being different lengths.
						while (UEINTX & (1 << RWAL))
						{
							UEDATX             = sd_sector[sector_byte_index];
							sector_byte_index += 1;
						}

						//
						// Send the chunk of the sector data to the host.
						//

						UEINTX &= ~(1 << TXINI);
						UEINTX &= ~(1 << FIFOCON);
					}
				}
			}
			else if (transaction_sector_count) // [Mass Storage - OCR].
			{
				_usb_ms_receive_sector();

				//
				// Determine whether or not we need to inspect the write transaction to do OCR.
				//

				b8   transaction_is_for_bmp  = false;
				u16  curr_red_channel_offset = 0;
				u8   compressed_slot_stride  = pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].compressed_slot_stride);
				u8_2 board_dim_slots         =
					{
						pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.x),
						pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.y),
					};

				if (transaction_sector_start_address >= FAT32_RESERVED_SECTOR_COUNT + FAT32_TABLE_SECTOR_COUNT) // In data region?
				{
					switch (usb_ms_ocr_state)
					{
						case USBMSOCRState_ready: // The main program is still setting up the OCR state or is reading the parsing results.
						{
						} break;

						case USBMSOCRState_set: // Is this writing transaction the beginning of a BMP?
						{
							struct BMPFileHeader* bmp_file_header = (struct BMPFileHeader*)  sd_sector;
							struct BMPDIBHeader*  bmp_dib_header  = (struct BMPDIBHeader* ) (sd_sector + sizeof(struct BMPFileHeader));
							static_assert(sizeof(struct BMPFileHeader) + sizeof(struct BMPDIBHeader) <= sizeof(sd_sector));

							if
							(
								(transaction_sector_start_address - (FAT32_RESERVED_SECTOR_COUNT + FAT32_TABLE_SECTOR_COUNT)) % FAT32_SECTORS_PER_CLUSTER == 0 // Cluster-aligned?
								&& bmp_file_header->bfType == BMP_FILE_HEADER_SIGNATURE
								&& bmp_file_header->bfOffBits   == sizeof(struct BMPFileHeader) + BMPDIBHEADER_MIN_SIZE_V5
								&& bmp_dib_header->Size         == BMPDIBHEADER_MIN_SIZE_V5
								&& bmp_dib_header->Planes       == 1
								&& bmp_dib_header->BitCount     == 32
								&& bmp_dib_header->Compression  == BMPCompression_BITFIELDS
								&& bmp_dib_header->v2.RedMask   == 0x00'FF'00'00
								&& bmp_dib_header->v2.GreenMask == 0x00'00'FF'00
								&& bmp_dib_header->v2.BlueMask  == 0x00'00'00'FF
								&& bmp_dib_header->v3.AlphaMask == 0xFF'00'00'00
							)
							{
								if
								(
									((u32)  bmp_dib_header->Width ) != ((u32) board_dim_slots.x - 1) * compressed_slot_stride + MASK_DIM ||
									((u32) -bmp_dib_header->Height) != ((u32) board_dim_slots.y - 1) * compressed_slot_stride + MASK_DIM
								)
								{
									error(); // Received BMP is not conforming to the expect dimensions for the wordgame.
								}

								usb_ms_ocr_state        = USBMSOCRState_processing;
								curr_red_channel_offset = sizeof(struct BMPFileHeader) + sizeof(struct BMPDIBHeader) + 2; // Color channels are ordered BGRA in memory; +2 skips blue and green bytes.
								transaction_is_for_bmp  = true;
							}
						} break;

						case USBMSOCRState_processing: // Does this sector look like BMP pixel data?
						{
							u32 pixels_in_bmp_row_processed      = ((u32) _usb_ms_ocr_slot_topdown_board_coords.x) * compressed_slot_stride + _usb_ms_ocr_slot_topdown_pixel_coords.x;
							u32 bmp_width                        = ((u32) board_dim_slots.x - 1) * compressed_slot_stride + MASK_DIM;
							u32 total_amount_of_pixels_remaining = (((u32) board_dim_slots.y - 1 - _usb_ms_ocr_slot_topdown_board_coords.y) * compressed_slot_stride + MASK_DIM - _usb_ms_ocr_slot_topdown_pixel_coords.y) * bmp_width - pixels_in_bmp_row_processed;
							u8  pixels_to_verify                 = FAT32_SECTOR_SIZE / sizeof(struct BMPPixel);
							if (pixels_to_verify > total_amount_of_pixels_remaining)
							{
								pixels_to_verify = total_amount_of_pixels_remaining;
							}

							transaction_is_for_bmp = true;
							for (u8 pixel_index = 0; pixel_index < pixels_to_verify; pixel_index += 1)
							{
								if (sd_sector[pixel_index * sizeof(struct BMPPixel) + 1] != 0xFF) // Alpha should always be set.
								{
									transaction_is_for_bmp = false;
									break;
								}
							}
						} break;
					}
				}

				//
				// Perform write transaction.
				//

				for
				(
					u32 sector_address = transaction_sector_start_address;
					sector_address < transaction_sector_start_address + transaction_sector_count;
					sector_address += 1
				)
				{
					if (sector_address != transaction_sector_start_address) // We already did the first write in order to inspect the sector.
					{
						_usb_ms_receive_sector();
					}

					if (transaction_is_for_bmp && usb_ms_ocr_state == USBMSOCRState_processing)
					{
						while (curr_red_channel_offset < FAT32_SECTOR_SIZE && usb_ms_ocr_state == USBMSOCRState_processing)
						{
							//
							// Determine if we should be OCRing for a letter, or should we just skip the pixels.
							//

							b8 should_process = false;

							if (_usb_ms_ocr_slot_topdown_pixel_coords.x < MASK_DIM && _usb_ms_ocr_slot_topdown_pixel_coords.y < MASK_DIM)
							{
								should_process =
									!is_slot_excluded
									(
										diplomat_packet.wordgame,
										_usb_ms_ocr_slot_topdown_board_coords.x,
										board_dim_slots.y - 1 - _usb_ms_ocr_slot_topdown_board_coords.y
									);

								if (should_process && diplomat_packet.wordgame == WordGame_wordbites && _usb_ms_ocr_slot_topdown_board_coords.y)
								{
									u8_2 coords =
										{
											_usb_ms_ocr_slot_topdown_board_coords.x,
											board_dim_slots.y - 1 - _usb_ms_ocr_slot_topdown_board_coords.y
										};

									should_process &= // Up-left slot must be free.
										implies
										(
											coords.x,
											!diplomat_packet.board[coords.y + 1][coords.x - 1]
										);

									should_process &= // Up-right slot must be free.
										implies
										(
											coords.x < board_dim_slots.x - 1,
											!diplomat_packet.board[coords.y + 1][coords.x + 1]
										);

									should_process &= // Must not be below a vertical duo piece.
										implies
										(
											coords.y + 2 < board_dim_slots.y,
											!(
												diplomat_packet.board[coords.y + 2][coords.x] &&
												diplomat_packet.board[coords.y + 1][coords.x]
											)
										);

									should_process &= // We must have already encountered an activated pixel at some point.
										implies
										(
											!(_usb_ms_ocr_packed_activated_slots & (1 << coords.x)),
											_usb_ms_ocr_slot_topdown_pixel_coords.y < MASK_DIM / 4
										);
									static_assert(MASK_DIM == 64);
								}
							}

							//
							// Process pixels, or just skip.
							//

							if (should_process)
							{
								//
								// Extract activated pixels.
								//

								while (_usb_ms_ocr_slot_topdown_pixel_coords.x < MASK_DIM && curr_red_channel_offset < FAT32_SECTOR_SIZE)
								{
									_usb_ms_ocr_packed_slot_pixels[_usb_ms_ocr_slot_topdown_pixel_coords.x / 8] |=
										(sd_sector[curr_red_channel_offset] <= MASK_ACTIVATION_THRESHOLD) << (_usb_ms_ocr_slot_topdown_pixel_coords.x % 8);

									curr_red_channel_offset                 += sizeof(struct BMPPixel);
									_usb_ms_ocr_slot_topdown_pixel_coords.x += 1;
								}

								//
								// [OCR - Mask Stream].
								//

								if (_usb_ms_ocr_slot_topdown_pixel_coords.x == MASK_DIM)
								{
									//
									// Go through a bit of the mask stream to compare the activated pixels we've found.
									//

									struct USBMSOCRMaskStreamState probing_mask_stream_state = _usb_ms_ocr_curr_mask_stream_state;

									for
									(
										enum Letter letter = {1};
										letter < pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].sentinel_letter);
										letter += 1
									)
									{
										static_assert(MASK_DIM % 8 == 0);
										for (u8 byte_index = 0; byte_index < MASK_DIM / 8; byte_index += 1)
										{
											//
											// Get mask byte.
											//

											u8 mask_byte          = 0;
											u8 mask_bits_unfilled = bitsof(mask_byte);

											while (mask_bits_unfilled)
											{
												//
												// Refill runlength.
												//

												if (!probing_mask_stream_state.runlength_remaining)
												{
													//
													// Read the next byte-sized run-length.
													//

													probing_mask_stream_state.runlength_remaining  = pgm_u8(MASK_U8_STREAM[probing_mask_stream_state.index_u8]);
													probing_mask_stream_state.index_u8            += 1;

													//
													// If we read a zero, then run-length is word-sized.
													//

													if (!probing_mask_stream_state.runlength_remaining)
													{
														probing_mask_stream_state.runlength_remaining  = pgm_u16(MASK_U16_STREAM[probing_mask_stream_state.index_u16]);
														probing_mask_stream_state.index_u16           += 1;
													}
												}

												//
												// Shift in bits of mask.
												//

												u8 shift_amount =
													mask_bits_unfilled < probing_mask_stream_state.runlength_remaining
														? mask_bits_unfilled
														: probing_mask_stream_state.runlength_remaining;

												if (probing_mask_stream_state.index_u8 & 1) // Shift in zeros?
												{
													mask_byte = mask_byte >> shift_amount;
												}
												else // Shift in ones.
												{
													mask_byte = ~(((u8) ~mask_byte) >> shift_amount); // Shift in ones.
												}

												mask_bits_unfilled                            -= shift_amount;
												probing_mask_stream_state.runlength_remaining -= shift_amount;
											}

											//
											// Compare.
											//

											_usb_ms_ocr_accumulated_scores[_usb_ms_ocr_slot_topdown_board_coords.x][letter] +=
												count_cleared_bits(_usb_ms_ocr_packed_slot_pixels[byte_index] ^ mask_byte);

											if (_usb_ms_ocr_packed_slot_pixels[byte_index]) // Make note that we found an activated pixel in this slot.
											{
												_usb_ms_ocr_packed_activated_slots |= 1 << _usb_ms_ocr_slot_topdown_board_coords.x;
											}
										}
									}

									memset(_usb_ms_ocr_packed_slot_pixels, 0, sizeof(_usb_ms_ocr_packed_slot_pixels));

									//
									// Make note of where the next row of mask data will be in the mask stream.
									//

									if (!_usb_ms_ocr_next_mask_stream_state.index_u8)
									{
										u16 remaining_mask_row_pixels = MASK_DIM * (Letter_COUNT - pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].sentinel_letter));
										while (remaining_mask_row_pixels)
										{
											if (!probing_mask_stream_state.runlength_remaining)
											{
												//
												// Read the next byte-sized run-length.
												//

												probing_mask_stream_state.runlength_remaining  = pgm_u8(MASK_U8_STREAM[probing_mask_stream_state.index_u8]);
												probing_mask_stream_state.index_u8            += 1;

												//
												// If we read a zero, then run-length is word-sized.
												//

												if (!probing_mask_stream_state.runlength_remaining)
												{
													probing_mask_stream_state.runlength_remaining  = pgm_u16(MASK_U16_STREAM[probing_mask_stream_state.index_u16]);
													probing_mask_stream_state.index_u16           += 1;
												}
											}

											u16 shift_amount =
												remaining_mask_row_pixels < probing_mask_stream_state.runlength_remaining
													? remaining_mask_row_pixels
													: probing_mask_stream_state.runlength_remaining;

											remaining_mask_row_pixels                     -= shift_amount;
											probing_mask_stream_state.runlength_remaining -= shift_amount;
										}

										_usb_ms_ocr_next_mask_stream_state = probing_mask_stream_state;
									}
								}
							}
							else // Skip pixels.
							{
								u8 skip_amount =
									_usb_ms_ocr_slot_topdown_board_coords.x == board_dim_slots.x - 1 // On last column of slots?
										? MASK_DIM               - _usb_ms_ocr_slot_topdown_pixel_coords.x
										: compressed_slot_stride - _usb_ms_ocr_slot_topdown_pixel_coords.x;

								u8 remaining_unprocessed_pixels_in_sector = (FAT32_SECTOR_SIZE - curr_red_channel_offset) / sizeof(struct BMPPixel); // This math also happens to work for the first sector of the BMP!
								if (skip_amount > remaining_unprocessed_pixels_in_sector)
								{
									skip_amount = remaining_unprocessed_pixels_in_sector;
								}

								curr_red_channel_offset                 += skip_amount * sizeof(struct BMPPixel);
								_usb_ms_ocr_slot_topdown_pixel_coords.x += skip_amount;
							}

							//
							// Update coordinates.
							//

							if (_usb_ms_ocr_slot_topdown_pixel_coords.x == compressed_slot_stride) // Move to the next horizontally adjacent slot.
							{
								_usb_ms_ocr_slot_topdown_board_coords.x += 1;
								_usb_ms_ocr_slot_topdown_pixel_coords.x  = 0;
							}
							else if (_usb_ms_ocr_slot_topdown_pixel_coords.x == MASK_DIM && _usb_ms_ocr_slot_topdown_board_coords.x == board_dim_slots.x - 1) // On right edge of BMP.
							{
								_usb_ms_ocr_slot_topdown_board_coords.x  = 0;
								_usb_ms_ocr_slot_topdown_pixel_coords.x  = 0;
								_usb_ms_ocr_slot_topdown_pixel_coords.y += 1;
								_usb_ms_ocr_curr_mask_stream_state       = _usb_ms_ocr_next_mask_stream_state;
								_usb_ms_ocr_next_mask_stream_state       = (struct USBMSOCRMaskStreamState) {0};

								if (_usb_ms_ocr_slot_topdown_pixel_coords.y == MASK_DIM) // Went through every pixel row of the slots?
								{
									//
									// Pick the best matching letters.
									//

									for (u8 slot_coord_x = 0; slot_coord_x < board_dim_slots.x; slot_coord_x += 1)
									{
										enum Letter best_letter = Letter_null;

										if (_usb_ms_ocr_packed_activated_slots & (1 << slot_coord_x)) // Was this slot actually processed?
										{
											u16 highest_score = 0;
											for
											(
												enum Letter letter = {1};
												letter < pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].sentinel_letter);
												letter += 1
											)
											{
												if (highest_score < _usb_ms_ocr_accumulated_scores[slot_coord_x][letter])
												{
													highest_score = _usb_ms_ocr_accumulated_scores[slot_coord_x][letter];
													best_letter   = letter;
												}
											}
										}

										diplomat_packet.board[board_dim_slots.y - 1 - _usb_ms_ocr_slot_topdown_board_coords.y][slot_coord_x] = best_letter;
									}

									_usb_ms_ocr_packed_activated_slots = 0;
									_usb_ms_ocr_curr_mask_stream_state = (struct USBMSOCRMaskStreamState) {0};
									memset(_usb_ms_ocr_accumulated_scores, 0, sizeof(_usb_ms_ocr_accumulated_scores));

									//
									// Finally, at the end of BMP!
									//

									if (_usb_ms_ocr_slot_topdown_board_coords.y == board_dim_slots.y - 1)
									{
										_usb_ms_ocr_slot_topdown_board_coords.y = 0;
										_usb_ms_ocr_slot_topdown_pixel_coords.y = 0;
										usb_ms_ocr_state                        = USBMSOCRState_ready;

										// Immediately send out packet.
										for (u8 i = 0; i < sizeof(diplomat_packet); i += 1)
										{
											usart_tx(((volatile u8*) &diplomat_packet)[i]);
										}
										usart_tx(0xFF); // We're ready for a command!
									}
								}
								else if (_usb_ms_ocr_slot_topdown_pixel_coords.y == compressed_slot_stride) // Move onto the next row of slots.
								{
									_usb_ms_ocr_slot_topdown_board_coords.y += 1;
									_usb_ms_ocr_slot_topdown_pixel_coords.y  = 0;
								}
							}
						}

						curr_red_channel_offset = 0;
					}
					else // Only actually write to SD card if it's junk we don't care about; this speeds up the OCR process as we aren't wasting time doing data transfers.
					{
						sd_write(sector_address);
					}
				}
			}

			_usb_ms_status.dCSWDataResidue = 0;
			_usb_ms_status.bCSWStatus      = USBMSCommandStatusWrapperStatus_success;
		}
		else // Miscellaneous SCSI commands.
		{
			b8        unsupported_command = false;
			const u8* info_data           = 0;
			u8        info_size           = 0;

			switch (command.CBWCB[0])
			{
				case USBMSSCSIOpcode_test_unit_ready: // See: Source(13) @ Section(7.25) @ Page(163).
				{
					u8 control = command.CBWCB[5];

					if (!(command.dCBWDataTransferLength == 0 && command.bCBWCBLength == 6))
					{
						error(); // Command wrapper does not match the actual wrapped command.
					}

					if (control)
					{
						error(); // The common values for the fields are different for some reason; we should probably handle it if this happens.
					}
				} break;

				case USBMSSCSIOpcode_inquiry: // See: Source(13) @ Section(7.3.1) @ Page(80).
				{
					// Pretty irrelevant fields.
					b8 enable_vital_product_data = (command.CBWCB[1] >> 0) & 1; // "EVPD".
					b8 command_support_data      = (command.CBWCB[1] >> 1) & 1; // "CMDT".
					u8 page_operation_code       =  command.CBWCB[2];
					u8 allocation_length         =  command.CBWCB[4];
					u8 control                   =  command.CBWCB[5];

					// Windows usually the command wrapper with command buffer length of 6 (as it should be),
					// but when formatting the drive, this changes to 12 for some reason!
					if (!(command.dCBWDataTransferLength == allocation_length && command.bmCBWFlags && (command.bCBWCBLength == 6 || command.bCBWCBLength == 12)))
					{
						error(); // Command wrapper does not match the actual wrapped command.
					}

					if
					(
						!enable_vital_product_data && !command_support_data && // Host requests for standard inquiry data.
						!page_operation_code &&                                // Must be zero for standard inquiry data request.
						!control
					)
					{
						info_data = USB_MS_SCSI_INQUIRY_DATA;
						info_size = sizeof(USB_MS_SCSI_INQUIRY_DATA);
						static_assert(sizeof(USB_MS_SCSI_INQUIRY_DATA) <= USB_ENDPOINT_MS_IN_SIZE);
					}
					else
					{
						unsupported_command = true;
					}
				} break;

				case USBMSSCSIOpcode_request_sense: // See: Source(13) @ Section(7.20) @ Page(135).
				{
					u8 allocation_length = command.CBWCB[4];
					u8 control           = command.CBWCB[5];

					// bCBWCBLength is for some reason 12 on Windows, even thought the command is only 6 bytes long.
					// Apple seems to do this correctly though, so maybe a Windows driver bug?
					if (!(command.dCBWDataTransferLength == allocation_length && command.bmCBWFlags && (command.bCBWCBLength == 6 || command.bCBWCBLength == 12)))
					{
						error(); // Command wrapper does not match the actual wrapped command.
					}

					if (control)
					{
						error(); // The common values for the fields are different for some reason; we should probably handle it if this happens.
					}

					info_data = USB_MS_SCSI_UNSUPPORTED_COMMAND_SENSE;
					info_size = sizeof(USB_MS_SCSI_UNSUPPORTED_COMMAND_SENSE);
					static_assert(sizeof(USB_MS_SCSI_UNSUPPORTED_COMMAND_SENSE) <= USB_ENDPOINT_MS_IN_SIZE);
				} break;

				case USBMSSCSIOpcode_mode_sense: // See: Source(13) @ Section(7.8) @ Page(100).
				{
					// Pretty irrelevant fields.
					b8 disable_block_descriptors = (command.CBWCB[1] >> 3) & 1;
					u8 page_control              = (command.CBWCB[2] >> 6) & 0b11;
					u8 page_code                 = (command.CBWCB[2] >> 0) & 0b11'1111;
					u8 allocation_length         =  command.CBWCB[4];
					u8 control                   =  command.CBWCB[5];

					if (!(command.dCBWDataTransferLength == allocation_length && command.bmCBWFlags && command.bCBWCBLength == 6))
					{
						error(); // Command wrapper does not match the actual wrapped command.
					}

					// Apple wants us to at least acknowledge the "return all mode pages" (0x3F) usage; other than that, this command is pretty irrelevant. See: Source(13) @ Table(64) @ Page(101).
					if (!(!disable_block_descriptors && page_control == 0b00 && page_code == 0x3F && !control))
					{
						unsupported_command = true;
					}
				} break;

				case USBMSSCSIOpcode_read_capacity: // See: Source(14) @ Section(5.10) @ Page(54).
				{
					// Pretty irrelevant fields.
					b8 partial_medium_indicator = (command.CBWCB[8] & 1); // "PMI".
					u8 control                  =  command.CBWCB[9];

					u32 sector_start_address =
						(((u32) command.CBWCB[2]) << 24) |
						(((u32) command.CBWCB[3]) << 16) |
						(((u32) command.CBWCB[4]) <<  8) |
						(((u32) command.CBWCB[5]) <<  0);

					if (!(command.dCBWDataTransferLength == 8 && command.bmCBWFlags && command.bCBWCBLength == 10))
					{
						error(); // Command wrapper does not match the actual wrapped command.
					}

					if
					(
						!(
							!sector_start_address &&
							!partial_medium_indicator && // We send information about the last addressable logical block.
							!control
						)
					)
					{
						error(); // The common values for the fields are different for some reason; we should probably handle it if this happens.
					}

					info_data = USB_MS_SCSI_READ_CAPACITY_DATA;
					info_size = sizeof(USB_MS_SCSI_READ_CAPACITY_DATA);
					static_assert(sizeof(USB_MS_SCSI_READ_CAPACITY_DATA) <= USB_ENDPOINT_MS_IN_SIZE);
				} break;

				default:
				{
					unsupported_command = true;
				} break;
			}

			if (unsupported_command) // Denote a failure status.
			{
				if (!command.dCBWDataTransferLength)
				{
					// No stalling necessary.
				}
				else if (command.bmCBWFlags) // Device to host.
				{
					UENUM   = USB_ENDPOINT_MS_IN_INDEX;
					UECONX |= (1 << STALLRQ);
				}
				else // Host to device.
				{
					error(); // Haven't encountered a situation where this case needs to be handled, but should be implemented if needed.
				}

				_usb_ms_status.dCSWDataResidue = command.dCBWDataTransferLength;
				_usb_ms_status.bCSWStatus      = USBMSCommandStatusWrapperStatus_failed;
			}
			else if (command.dCBWDataTransferLength) // Send SCSI device server info to host.
			{
				u8 amount_to_send = info_size;
				if (amount_to_send > command.dCBWDataTransferLength)
				{
					amount_to_send = command.dCBWDataTransferLength;
				}

				UENUM = USB_ENDPOINT_MS_IN_INDEX;
				while (!(UEINTX & (1 << TXINI)));
				for (u8 i = 0; i < amount_to_send; i += 1)
				{
					UEDATX = pgm_u8(info_data[i]);
				}
				UEINTX &= ~(1 << TXINI);
				UEINTX &= ~(1 << FIFOCON);

				_usb_ms_status.dCSWDataResidue = command.dCBWDataTransferLength - amount_to_send;
				_usb_ms_status.bCSWStatus      = USBMSCommandStatusWrapperStatus_success;
			}
			else // No data transferring needs to occur.
			{
				_usb_ms_status.dCSWDataResidue = 0;
				_usb_ms_status.bCSWStatus      = USBMSCommandStatusWrapperStatus_success;
			}
		}
	}

	//
	// Send status.
	//

	UENUM = USB_ENDPOINT_MS_IN_INDEX;
	if (_usb_ms_send_status && !(UECONX & (1 << STALLRQ))) // If a stall condition has been placed, we wait until the default endpoint clears it first.
	{
		_usb_ms_send_status = false;

		while (!(UEINTX & (1 << TXINI)));

		static_assert(sizeof(_usb_ms_status) <= USB_ENDPOINT_MS_IN_SIZE);
		for (u8 i = 0; i < sizeof(_usb_ms_status); i += 1)
		{
			UEDATX = ((u8*) &_usb_ms_status)[i];
		}

		UEINTX &= ~(1 << TXINI);
		UEINTX &= ~(1 << FIFOCON);
	}
#endif

	#ifdef PIN_USB_BUSY
	pin_low(PIN_USB_BUSY);
	#endif
}

#if DEBUG && USB_CDC_ENABLE
	static void
	debug_chars(char* value, u16 value_size)
	{
		if (debug_usb_is_on_host_machine)
		{
			for (u16 i = 0; i < value_size; i += 1)
			{
				while (debug_usb_cdc_in_writer_masked(1) == debug_usb_cdc_in_reader_masked(0)); // Our write-cursor is before the interrupt's read-cursor.
				debug_usb_cdc_in_buffer[debug_usb_cdc_in_writer_masked(0)] = value[i];
				debug_usb_cdc_in_writer += 1;
			}
		}
	}

	static u8
	debug_rx(char* dst, u16 dst_size) // Note that PuTTY sends only '\r' (0x13) for keyboard enter.
	{
		u16 result = 0;

		if (debug_usb_is_on_host_machine)
		{
			while (result < dst_size && debug_usb_cdc_out_reader_masked(0) != debug_usb_cdc_out_writer_masked(0))
			{
				dst[result]               = debug_usb_cdc_out_buffer[debug_usb_cdc_out_reader_masked(0)];
				debug_usb_cdc_out_reader += 1;
				result                   += 1;
			}
		}

		return result;
	}
#endif

//
// Documentation.
//

/* [Overview].
	This file implements all the USB functionality for this device.
	The USB protocol is pretty intense. Hopefully the code base is well documented
	enough that it is easy to follow along to understand why things are being done
	the way they are. If they aren't, then many citations and sources are linked to
	sites, datasheets, specifications, etc. that describes a particular part in more
	detail. (1) contains some articles explaining parts of the USB protocol, and Ben Eater on
	YouTube goes pretty in-depth on the electrical USB transmission sides of things. See the many
	blocks of comments below for more in-depth explainations for the code written above.

	The first USB functionality that was implemented is the CDC which allows communication between
	host and device. This is vital for development since this allows us to debug easily by sending
	diagnostics to the host's terminal, and also for us to potentially receive inputs. It also gives
	us the ability to be able to trigger the bootloader remotely so we don't have to manually reset
	the MCU for reprogramming; a huge quality-of-life improvement. This class is really only for
	development, and doesn't actually contribute to the final firmware of the project. Thus, it
	should be disabled outside of DEBUG mode.

	(1) "NEFASTOR ONLINE" @ URL(nefastor.com/micrcontrollers/stm32/usb/).
*/

/* [USB Initialization Process].
	We carry out the following steps specified by (1):

		1. "Power on USB pads regulator".
			Based off of (2), the USB regulator simply supplies voltage to the USB data lines
			and the capacitor. So this first step is just as simple enabling UVREGE (3).

		2. "Configure PLL interface and enable PLL".
			PLL refers to a phase-locked-loop device. I don't know much about the mechanisms of
			the circuit, but it seems like you give it some input frequency and it'll output a
			higher frequency after it has synced up onto it. For full-speed USB, we must use the
			external crystal oscillator of the CPU (15), so that's what we'll be using.

			But we can't actually use the (16MHz) frequency directly since the actual PLL unit itself
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

			The ATmega32U4 specification is not very clear on what the state of the endpoints are
			after an End-of-Reset event. (12) did state that "all the endpoints are disabled",
			which seems to imply that the EPEN bit is cleared for each endpoint. Clearing the EPEN
			bit, as described in (13), causes an "endpoint reset" but keeps the configuration of
			the endpoints around. (14) describes this "endpoint reset" as where the internal state
			machines, the buffers, and some specific registers are restored to their initial
			values. None of this really, or at least clearly, explain the state of the endpoint's
			buffers in memory, and that if it's okay to just reinitialize each endpoint the same
			way as we did the first time around. Regardless, it seems to be working fine!

			We also enable the Start-of-Frame interrupt which is a signal that's sent by the host
			every frame (~1ms for full-speed USB). This is used to flush out data to be sent to the
			host or data to be received. The alternative would've been to have an interrupt for
			whenever the host wanted to do a transaction, but this happens so frequently that it
			really ends up bogging the main program down too much.

		6. "Wait for USB VBUS information connection".
			No need to really wait.

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
	(13) Clearing EPEN @ Source(1) @ Section(22.6) @ Page(272).
	(14) "Endpoint Reset" @ Source(1) @ Section(22.3) @ Page(270).
	(15) "Crystal-less Operation" @ Source(1) @ Section(21.4) @ Page(259).
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
	structured as so:

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

		- STUFF is any additional data that the packet with a specific PID might have. For
		example, TOKEN-typed packets (except for SOF) define the device address and
		endpoint that the host wants to talk to. For packets belonging to the HANDSHAKE
		category, there is no additional data. STUFF is where DATA-typed packets obviously
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
	states that any endpoints we configure before the reset will just end up being disabled.

	And thus, we now use this opportunity to configure endpoints by consulting the flowchart in (1):

		1. "Select the endpoint".
			ATmega32U4 uses the system where writing to and reading from
			endpoint-specific registers (e.g. UECFG0X) depends on the currently
			selected endpoint determined by UENUM (3).

		2. "Activate endpoint."
			Self-explainatory. The only thing we have to be aware of is that we
			have to initiate endpoints in sequential order so the memory allocation
			works out (2), but this is pretty much automatically handled by the for-loop too.

		3. "Configure and allocate".
			We set the endpoint's transfer type, direction, buffer size, interrupts, etc., which
			are described in the UECFG0X and UECFG1X registers. To make things sleek and automatic,
			the actual values that'll be assigned to these registers are stored compactly within
			USB_ENDPOINT_UECFGNX.

		4. "Test endpoint configuration".
			The memory allocation can technically fail for things like using more than
			832 bytes of memory or making a buffer larger than what that endpoint
			can support. This doesn't really need to be addressed much, just as long we
			aren't dynamically reallocating, disabling, etc. endpoints at run-time.

	We also want from endpoint 0 the ability for it to detect when it has received a
	SETUP-transaction as it is where the rest of the enumeration will be communicated
	through, and USB_COM_vect will be the one to carry that out. Note that the
	interrupt USB_COM_vect is only going to trigger for endpoint 0. So if the host
	sends a SETUP-transaction to endpoint 3, it'll be ignored (unless the interrupt
	for that is also enabled) (5).

	This interrupt routine also handle the Start-of-Frame event that the host sends to the device
	every ~1ms (for full-speed USB devices). This part of the interrupt is pretty much used for
	sending out buffered data or to copy an endpoint's buffer into our own ring buffer. An
	interrupt could be made for each specific endpoint within Start-of-Frame event to be instead
	triggered in USB_COM_vect, but these interrupts would be executed so often that it'll grind the
	MCU down to a halt. The only exception is the mass storage, since we want to prioritize this to
	allow the flashdrive functionality to be as snappy as it can be.

	(1) "USB Device Controller Interrupt System" @ Source(1) @ Figure(22-4) @ Page(279).
	(2) Endpoint Memory Management @ Source(1) @ Section(21.9) @ Page(263-264).
	(3) "Endpoint Selection" @ Source(1) @ Section(22.5) @ Page(271).
	(4) "USB Reset" @ Source(1) @ Section(22.4) @ Page(271).
	(5) "USB Device Controller Endpoint Interrupt System" @ Source(1) @ Figure(22-5) @ Page(280).
*/

/* [Endpoint 0: Data-Transfer from Device to Host].
	We are in a state where endpoint 0 needs to transmit some data to the host in response to
	a SETUP-transaction, and this process here is non-trivial since the data that needs to be sent
	might have to be broken up into multiple transactions and also the fact that the host could
	abort early.

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

	At the beginning, endpoint 0 received a SETUP-transaction where the host, say, asked the
	device for the string of its manufacturer's name. If endpoint 0's buffer is small and/or the
	requested amount of data is large enough, then this transferring of data will be done through
	a series of IN-transactions.

	In that series of IN-transactions, endpoint 0's buffer will be filled as much as possible.
	Only the data-packet in the last IN-transaction may be smaller than endpoint 0's maximum
	capacity in the case that the requested amount of data is not a perfect multiple of the buffer
	size (1). In other words, we should be expecting the host to stop sending IN-transactions when
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
	This interrupt routine is where the meat of all the enumeration takes place. It handles
	any events related to endpoints, all listed in (1):
		- An endpoint's buffer is ready to be filled with data to be sent to the host.
		- An endpoint's buffer is filled with data sent by the host.
		- A control-typed endpoint received a SETUP-transaction.
		- An endpoint replied with a STALL-HANDSHAKE packet.
		- An isochronous endpoint detected a CRC mismatch.
		- An isochronous endpoint underwent overflow/underflow of data.
		- An endpoint replied with a NACK-HANDSHAKE packet.

	The first half is where endpoint 0's SETUP-transactions are handled to get the USB device all
	set up and configured. The other half is for the mass storage functionality. The reason why
	it's here and not in the other interrupt routine with the CDC and HID is so we can handle
	the transactions as fast as possible. In fact, the entire mass storage transaction of receiving
	a command, sending/receiving data, and then sending the status is all usually done in one go
	here. This reduces the amount of cycles wasted in having to do a context switch between an
	interrupt routine and the main program.

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
	being the ".kind" field of USBSetupRequest. See: USBSetupRequestKind.

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

/* [Endpoint 0: GetDescriptor].
	The host sent a SETUP-transaction with a request, as described in the DATA-typed
	packet laid out in USBSetupRequest, telling the device to tell the host more about
	itself, hence "descriptor".

	One of the things the host might ask is what the device itself is. This is detailed
	within USB_DESC_DEVICE. Things such as the device's vendor and the amount of
	ways the device can be configured is transmitted to the host.

	Not all descriptors can be fulfilled. In this case, the device will send the host a
	STALL-HANDSHAKE packet as a response. See: [Endpoint 0: Request Error].

	The HID class can also send a GetDescriptor request where the recipent will be the HID
	interface, as indicated by (1) and (2). It's essentially the same as the standard GetDescriptor
	request, but with slightly different interpretations on the setup packet's fields. Technically,
	other non-HID USB classes can send this 16-bit request code (bmRequestType and bRequest bytes
	as one word), but HID is the only one that actually does this, so we just assume it's from HID.
	If another non-HID class does send this request code, we'd have to look at the recipent interface
	to see if it's at the HID interface or not. Really, this applies to any of the other non-standard
	USB requests I belive, but it seems like it's a pretty disjoint thing, so we don't really have to
	worry about conflicts much.

	(1) "bmRequestType" @ Source(2) @ Table(9-2) @ Page(248).
	(2) HID's GetDescriptor Request @ Source(7) @ Section(7.1.1) @ AbsPage(59).
*/

/* [Interfaces, Configurations, and Classes].
	An "interface" of a USB device is just a group of endpoints that are used for a
	specific functionality (but not really... see later). A simple mouse, for instance, may only
	just have a single interface with one endpoint that simply reports the delta-movement to the
	host.  A fancier mouse with a numberpad on the side may have two interfaces: one for the
	mouse functionality and the other for the keyboard of the numberpad. A flashdrive
	can have two endpoints: transmitting and receiving files to and from the host. The
	two endpoints here would be grouped together into one interface.

	A set of interfaces is called a "configuration" (2), and the host must choose the most
	appropriate configuration for the device. The ability of having multiple
	configurations allows greater flexibility of when and where the device can be used.
	For example, if the host is a laptop that is currently on battery-power, the host
	may choose a device configuration that is more power-efficient. When the laptop is
	plugged in, it may reconfigure the device to now use as much power as it wants.

	The functionality of an interface is determined by the "class" (such as bInterfaceClass).
	An example would be HID (human-interface device) which include, mouses, keyboards, data-gloves,
	etc. The USB specification does not detail much at all about these classes; that is, the
	corresponding class specifications must be consulted. The entire USB device can optionally be
	labeled as a single class (via bDeviceClass). This seems to be useful for the host in loading
	the necessary drivers, according to (1), but it is not required for the device to be classified
	as anything specific.

	There's nothing stopping from a USB device from having multi-function capabilities (say a mouse
	with a flashdrive function, for some reason). Ideally, each function that the device provided to
	the host would be in its own little neat box that is the interface, but the USB 2.0
	specification apparently never explicitly stated that this had to be the case, according to
	(3). An example of this issue is the communication functionality of our device being split
	into two separate interfaces (CDC and CDC-Data). Once you add audio and video on top of that
	communication functionality, I'm only going to guess that it gets even messier.

	This is resolved by noting in our device's class, subclass, and protocol to be particular
	values to indicate to the host that we have an "interface association descriptor" (IAD). This
	IAD is visually described in (4), but it essentially just groups multiple interfaces together
	into one proper function as it should've been.

	Our device only has a single configuration, and within it, we implement interfaces for each of
	the following classes (with IAD to group as necessary):
		- [CDC - Communication Class Device].
		- [HID - Human Interface Device Class].
		- [Mass Storage Device Class].

	(1) Device and Configuration Descriptors @ Source(4) @ Chapter(5).
	(2) "USB Descriptors" @ Source(4) @ Chapter(5).
	(3) IAD Purpose @ Source(9) @ Section(1) @ AbsPage(4).
	(4) IAD Graphic @ Source(9) @ Section(2) @ AbsPage(5).
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
	:                                                                     :
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
	:                                                                     :
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
	We are going to define a couple interfaces (CDC and CDC-Data) later on, so we would need to
	declare the entire device as being a communication device in USB_DESC_DEVICE as mentioned in
	(5), but since we also have other device functionalities we want to bring to the host, this is
	done in the corresponding IAD instead.

	As stated earlier, there are many communication devices that the CDC specification
	wants to be compatiable with. The one that will be the most applicable for us is the
	"Abstract Control Model" as seen in (4). Based on the figure, the specification
	is assuming we are some sort of device that has components for data-compression,
	error-correction, etc. just like a modem might have. But a lot of this doesn't actually really
	matter at all; we're still sending pure binary data-packets to the host via USB.

	Within our USB_CONFIG, we define the "Communication Class Interface".
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

	After all that, we then define the CDC-Data class (20). This one seems pretty straight-forward;
	we just kinda specify some endpoints that'll be used for transferring data to and from the
	host. It can be used for sending raw data, or some sort of structured format that the
	specification didn't define. In any case, this is where we define the endpoints that'll be
	transmitting diagnostics and to receive input from the host.

	Since we did define two interfaces to bring a single feature to the host, the CDC and CDC-Data
	interfaces are grouped together under the previous IAD that was defined before (see:
	USB_CONFIG).

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
	The HID specifications is a lot more digestable than the CDC one. Overall, it's pretty
	straightforward. We define an interface with the descriptor denoting that it belongs to the
	HID class. After the interface descriptor is the HID descriptor (confusing namings around
	honestly) that acts as a root for the host to access all the other HID-specific descriptors.
	In fact, a nice little diagram at (1) illustrates this, something that CDC should learn from.

	We give the HID class a single IN-typed interrupt endpoint that'll periodically and consistently
	sends data to the host. The format of this data is described throughly by the HID report
	descriptor (see: USB_DESC_HID_REPORT). I don't fully understand how this format exactly works,
	primarily due to the fact that testing it is a nightmare to do, but it's designed to be
	extremely flexible with any device that will ever be interacted by a human. The report consists
	of "items" that build up properties of how the data sent to the host should be interpreted
	(what units, whether or not it's relative or absolute, etc.). Not much has to be dived into
	here, since the HID specification was just so kind enough to literally provide multitude of
	example report descriptors (2)!

	With this class, we can now use the device as a mouse to move a cursor and do some clicks and
	drags.

	(1) Device Descriptor Structure @ Source(7) @ Section(5.1) @ AbsPage(22).
	(2) "Example USB Descriptors for HID Class Devices" @ Source(7) @ Appendix(E) @ absPage(76).
*/

/* [Mass Storage Device Class].
	The mass storage device class is a hot mess. I mean, it's pretty simple I suppose. Simple as
	minimal as a specification could get, but it's just the fact that you have to use a command set
	with SCSI being the de-facto standard. And oh boy what a mess that is. Anyways, we are
	specifically using the bulk-only transport (BOT) protocol to be able to communicate stuff to
	the host. After exchanging pleasantries, the host can command us to send it sectors of our
	flash drive data. Sectors are just the smallest unit of addressable part of a storage device,
	and is commonly 512 bytes. Thus writing a single byte or reading a single byte will consequently
	result in a 512-byte read/write command. Overall, this is pretty negligible for us.

	Now, originally, I was planning to just fool the host by sending the right sectors to convince
	that it was looking at some FAT32 file system (see: [Mass Storage - FAT32]). This would allow
	us to not have to require an actual external storage component to contain the entire file
	system, because all we needed was to make the host be able to send image data to us. When the
	image data comes in, we would parse it in the moment and not actually store it long-term.

	... turns out things are never that simple. For one, Windows makes some system files and
	directories when you plug in a fresh mass storage device. Things like
	"System Volume Information" and "Index Volume GUID", a whole bunch of junk. I believe these are
	for things like system restore points and optimizing the file indexing in File Explorer, but
	overall useless and unnecessary for us. I eventuall tweaked some random values in the register
	editor and disabled some arbitrary service so that Windows would not do this, but that wouldn't
	really fix anything, since the entire endgoal of this was to have it function on my iPhone, and
	last time I checked, I don't think I'd be able to do something like that. And maybe I wouldn't
	have worried about it at all if Apple didn't have their own equivalent of system files for
	mass storage devices, but they unfortunately do. This time they give the system folder the
	beautiful name of ".Spotlight-V100", and didn't bother to actually set the bit for the FAT32
	entry to make this directory hidden, so it ends up junking stuff up more. Ugh.

	So that strategy of "fooling" the host wasn't going to work. Even if I did send everything to
	the host one-to-one what my geniune flash drive would have, the host would need to write to
	these system files or change things around, and so long-term storage was just a necessity.

	Since we are now relying on an external SD card, this part of the system will be our major
	bottleneck in terms of performance. Luckily, the USB driver allows us to take a while to
	process data, but nonetheless we should not be trying to spend a significant amount of our
	time just reading or writing stuff to the SD card. To profile, I used Device Monitoring Studio
	to record the packets being sent between and forth the host and device when the device is
	initially plugged in. When it is plugged in, the host will read a bunch of sectors from the
	device (although I suppose it varies between platforms; Windows seems to copy the entire FAT
	before the device becomes usable, which takes a bit of time) and by averaging the delta time
	for the BULK-OUT responses in relation to the amount of bytes sent. Device Monitoring Studio
	allows for the entire session be saved as an HTM file, so the following little Javascript can
	be used to calculate the throughput of the BULK-typed enpoints for mass storage.

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

	As for the profiling results:

		1. Before optimizations.
			86.19012262482745 us/byte
			86.6420833083653 us/byte

		2. By increasing SPI speed to 8MHz.
			17.830957711557726 us/byte.
			17.83118092951088 us/byte.

		3. Using interrupt to respond to command upon reception instead of the start-of-frame event.
			5.8796026155650285 us/byte.
			5.879522185220327 us/byte.

		4. Further simplications to the MS state machine/control flow.
			4.62184988857523 us/byte.
			4.644121174125516 us/byte.

		5. Double buffering (buffer size beyond 64 not possible. See: [Endpoint Sizes]).
			3.90665591674541 us/byte
			4.402410773992042 us/byte
			3.9088519507718384 us/byte
			4.183835597591506 us/byte
			3.940001925166678 us/byte

	When uploading large files to the device on my machine, Windows reports upload speeds around
	355 KB/s. If we assume our device is taking 4 us/byte, then that's around 250 KB/s. Not too
	far off honestly.

	For actually using the mass storage class itself for doing OCR, see [Mass Storage - OCR].
*/

/* [Mass Storage - FAT32].
	The mass storage specification obviously does not specify what file system is actually used on
	the mass storage device itself. The OS will just have to determine that based on the data it
	reads from the sectors that we send. I decided to pick the FAT32 file system since that is what
	I'm most familiar with. I'm not sure if FAT12/FAT16 would work with Apple, it just might, but I
	doubt there would be much of any performance benefits gained to implementing it over FAT32; it
	definitely won't be worth the hassle either.

	Overall the FAT32 file system is pretty simple. The first sector is the boot sector and it has
	a lot of junk from the old DOS times, but the most important pieces of information are things
	like sectors per cluster, reserved sector count, bytes per sectors, etc. The reserved sectors
	are the first N sectors (including the boot sector itself) within the storage device. After
	the reserved sectors is the FAT ("File Allocation Table") itself. The boot sector also
	specifies how many sectors are dedicated to this FAT, and there even may be multiple FATs for
	redundancy. The table describes whether or not a cluster of sectors is free to be used, or if
	it is already allocated, and if so, indicates the next cluster in the chain. This essentially
	creates a linked list of clusters that can adapt to dynamically changing file and directory
	sizes. Finally after the FAT section is the data region where sectors themselves actually
	contain data pertaining to things such as file contents or directories. The sectors themselves
	are grouped into clusters with the numbering scheme beginning at #2 since "cluster #0" and
	"cluster #1" are reserved for something specific in the FAT (great job Microsoft!).

	Pictorally, the layout is something like this:
	.................................................................................
	:                                                                               :
	:                                                      - ---------------------  :
	:                                                        | ----------------- |  :
	:                                                  0     | |  BOOT SECTOR  | |  :
	:                                                        | ----------------- |  :
	:                                                      - | . . . . . . . . . |  :
	:                                                        |  . . . . . . . .  |  :
	:                                                  1     | . . . . . . . . . |  :
	:                                                        |  . . . . . . . .  |  :
	:                                                      - | . . . . . . . . . |  :
	:                                                        |  . . . . . . . .  |  :
	:                                                 ...    | . . . . . . . . . |  :
	:                                                        |  . . . . . . . .  |  :
	:                                    (BPB_RsvdSecCnt)  - |--------------------  :
	:                                                        |                   |  :
	:                                                  +0    |                   |  :
	:                                                        |                   |  :
	:                                                      - |                   |  :
	:                                                        |                   |  :
	:                                                  +1    |       FILE        |  :
	:                                                        |    ALLOCATION     |  :
	:                                                      - |      TABLE        |  :
	:                                                        |                   |  :
	:                                                  +2    |                   |  :
	:                                                        |                   |  :
	:                                                      - |                   |  :
	:                                                        |  . . . . . . . .  |  :
	:                                                 ...    | . . . . . . . . . |  :
	:                                                        |  . . . . . . . .  |  :
	:                        BPB_RsvdSecCnt + BPB_FATSz32  - |-------------------|  :
	:                                                        |                   |  :
	:                                                  +0    |                   |  :
	:                                                        |                   |  :
	:                                                      - |                   |  :
	:                                                        |                   |  :
	:                                                  +1    |                   |  :
	:                                                        |                   |  :
	:                                                      - |    CLUSTER #2     |  :
	:                                                        |                   |  :
	:                                                  +2    |                   |  :
	:                                                        |                   |  :
	:                                                      - |                   |  :
	:                                                        |  . . . . . . . .  |  :
	:                                                 ...    | . . . . . . . . . |  :
	:                                                        |  . . . . . . . .  |  :
	:     (BPB_RsvdSecCnt + BPB_FATSz32 + BPB_SecPerClus)  - |-------------------|  :
	:                                                        |                   |  :
	:                                                  +0    |                   |  :
	:                                                        |                   |  :
	:                                                      - |                   |  :
	:                                                        |                   |  :
	:                                                  +1    |                   |  :
	:                                                        |                   |  :
	:                                                      - |    CLUSTER #3     |  :
	:                                                        |                   |  :
	:                                                  +2    |                   |  :
	:                                                        |                   |  :
	:                                                      - |                   |  :
	:                                                        |  . . . . . . . .  |  :
	:                                                 ...    | . . . . . . . . . |  :
	:                                                        |  . . . . . . . .  |  :
	: (BPB_RsvdSecCnt + BPB_FATSz32 + BPB_SecPerClus * 2)  - ---------------------  :
	:                                                        |                   |  :
	:                                                  +0    |                   |  :
	:                                                        |                   |  :
	:                                                      - |                   |  :
	:                                                        |                   |  :
	:                                                  +1    |                   |  :
	:                                                        |                   |  :
	:                                                      - |    CLUSTER #4     |  :
	:                                                        |                   |  :
	:                                                  +2    |                   |  :
	:                                                        |                   |  :
	:                                                      - |                   |  :
	:                                                        |  . . . . . . . .  |  :
	:                                                 ...    | . . . . . . . . . |  :
	:                                                        |  . . . . . . . .  |  :
	: (BPB_RsvdSecCnt + BPB_FATSz32 + BPB_SecPerClus * 3)  - ---------------------  :
	:                                                        |  . . . . . . . .  |  :
	:                                                 ...    | . . . . . . . . . |  :
	:                                                        |  . . . . . . . .  |  :
	:                                                      - | . . . . . . . . . |  :
	:                                                        |  . . . . . . . .  |  :
	:                                                 ...    | . . . . . . . . . |  :
	:                                                        |  . . . . . . . .  |  :
	:                                                      - | . . . . . . . . . |  :
	:                                                        |  . . . . . . . .  |  :
	:                                                 ...    | . . . . . . . . . |  :
	:                                                        |  . . . . . . . .  |  :
	:                                                      - | . . . . . . . . . |  :
	:                                                        |  . . . . . . . .  |  :
	:                                                 ...    | . . . . . . . . . |  :
	:                                                        |  . . . . . . . .  |  :
	:                                        BPB_TotSec32  - ---------------------  :
	:                                                                               :
	:...............................................................................:

	It's important to have the FAT32 file system to have the right properties to be
	as unintrusive as possible in the OCR process. For example, having a cluster size
	(or "allocation unit size" as Windows' drive formatter calls it) as large as possible
	and overall sector count as smallest as it can be will make the FAT itself smaller.
	Windows seem to copy the entire table when the device is first plugged in, so having
	to transfer less sectors for the FAT reduces the initialization time dramatically.
	Apple doesn't seem to do it, but having a large cluster size will also theoretically
	make the FAT have to be updated less often.

	To keep things sane when needed, it is also possible for the Diplomat to perform a
	full wipe of the file system that'll completely clear the FAT and root cluster.
*/

/* [Endpoint 0: SetAddress].
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

/* [Endpoint 0: SetConfiguration].
	This is where the host obviously set the configuration of the device. The host could send a 0
	to indicate that we should be in the "address state" (1), which is the state a USB device is in
	after it has been just assigned an address; more likely though the host will send a
	configuration value of USB_CONFIG_ID (which it got from GetDescriptor) to set to our
	only configuration that is USB_CONFIG. This request would only really matter when we
	have multiple configurations that the host may choose from.

	(1) "Address State" @ Source(2) @ Section(9.4.7) @ Page(257).
*/

/* [Endpoint 0: CDC-Specific SetLineCoding].
	This request is supposed to be used by the host to configure aspects of the device's
	serial communication (e.g. baud-rate). The way we are using it, however, is for the host to
	send signals to the device without relying on something else like an IO pin or using the
	diagnostics channels.

	The host sends us a struct USBCDCLineCoding detailing the settings the device should be in now,
	and we assigned specific baud-rates beforehand to do certain things. For example, receiving a
	baud-rate of DIAGNOSTIC_BAUD_SIGNAL will mean that PuTTY (or whatever terminal is used) has
	opened the diagnostic pipes. This prevents from any diagnostic messages early on from being
	sent to the void because the terminal couldn't open up the COM port quick enough.

	The more important (as in: convenient) signal is BOOTLOADER_BAUD_SIGNAL. This signal is
	when we should hand the execution back to the booloader so we can reprogram the MCU without
	having to manually put the RST pin low. You'd think it'd be a trivial task to do right? Just
	jump to some random address in program memory of where the bootloader would be and that'll be
	it. Nope. There's very little resources online that detail on how the bootloader is implemented
	on this ATmega32U4 I have. Are there multiple bootloaders, or are there only one? I don't know.
	Is there an implementation of an ATmega32U4 bootloader, the very same one that's flashed onto
	mine? I don't know. All I know is that it's not as simple as jumping to the bootloader's
	address (wherever that might be!) since it might first check whether or not the MCU has
	restarted from an external reset (RST pin pulled low), and if not, it just hands the execution
	back to the main program. So how does Arduino manage to do it then? I can't even say for
	certain either, but from a public repo in (1), there's this single line of code (which I will
	mention lacks ANY sort of explaination!) that writes to some seemingly arbitrary address with
	some seemingly arbritary value. The crazy thing about this is that if we then perform a
	watchdog reset afterwards, the bootloader actually runs. What does the line even do? I have no
	goddamn idea. I'm guessing that this is writing some special magic value in a specific spot of
	memory (probably where the bootloader resides). How does this write persist through a watchdog
	reset? I'm not even sure. I hate writing code that I just copy-paste and leave not knowing how
	it works at all, but this is probably better as a TODO to figured out later...
	TODO Figured it out: caterina bootloader.

	(1) Magic Bootloader Signal @ URL(github.com/PaxInstruments/ATmega32U4-bootloader/blob/bf5d4d1edff529d5cc8229f15463720250c7bcd3/avr/cores/arduino/CDC.cpp#L99C14-L99C14).
*/

/* [Mouse Commands].
	Commands to move and drag/click with the mouse are done through 16-bits: HXXX'XXXX'YYYY'YYYY.
	H is whether or not the mouse button will be held during the movement and X and Y are
	the coordinates of where the mouse should end up. Note that the X coordinate is only given 7
	bits while Y has 8 bits, so the maximum value is for X is 127 and for Y is 255. This is to
	compensate for the aspect ratio of the iPhone screen.

	Of course, we don't actually know the true coordinates of the mouse, we can only infer based
	on its "known" starting position and the accumulation of all the movements we send to the host.

	The actual mouse delta that is sent to the host is only 1, and strictly in either the x-axis or
	the y-axis. This is to prevent mouse acceleration from becoming an issue (an issue that
	wouldn't exist if Apple had happen to provide a way to disable it!). We could change this
	delta to be something greater, but it's not a very fine setting; it's more optimal to go
	through iOS settings and tweak the pointer sensitivity there.

	Because we only use small deltas to move the mouse from one pair of coordinates to another pair
	of coordinates, we have to send tiny deltas to the host to nudge the mouse to the desired
	position. The nudge is done in a specific way so that it flips between the x-axis and the
	y-axis to achieve diagonal movement. Too bad we can't actually just go diagonally, because once
	again, mouse acceleration!

	The mouse input packet that is sent to the host describes whether or not the primary button
	is pressed or not, and what the mouse movement is. Now, if the packet reports that the button
	is pressed, does the host interpret that as being pressed before the movement, or after the
	movement? Well, it really shouldn't matter; after all, we are only moving in tiny increments
	of 1 in the x-axis or the y-axis. Except... it does matter for some reason! The mouse movement
	over time becomes slightly miscalibrated if we send a change in mouse button state and also a
	mouse delta. But if we send the change in mouse button state (if any) in one packet, and then
	in the next packet we send the mouse delta, the movement ends up much more reliable (although
	still not all the time, unfortunately). My only guess is that the change in mouse button state
	affects the internal state machine for the mouse acceleration in some particular way, perhaps
	the time-delta or some shenanigans. I cannot for certain at all, so it's pretty annoying
	honestly.

	There's no guarantee on the reliability of the mouse movement, so we will need to recalibrate
	the mouse once in a while by shoving it into the corner. Recalibration can be done by using
	usb_mouse_command(false, 0, 0), which essentially buffers up a 0 command. Since the mouse
	would be going into the corner anyways, we can take advantage of this by blasting the host
	with huge mouse deltas.
*/

/* [Endpoint 0: HID-Specific SetIdle].
	The host wants to reduce the amount of data being transferred in the case where HID report
	payloads ends up being the same as the previous. We seem to be alright in ignoring the (1)
	request though.

	(1) "Set Idle" @ Source(7) @ Section(7.2.4) @ AbsPage(62-63).
*/

/* [Endpoint 0: Extraneous CDC-Specific Requests].
	The (1) and (2) need to be handled in order for the CDC enumeration to be successful, but
	other than that, they seem pretty irrelevant for functionality.

	(1) "GetLineCoding" @ Source(6) @ Section(6.2.13) @ AbsPage(69)
	(2) "SetLineControlLineState" @ Source(6) @ Section(6.2.14) @ AbsPage(69-70).
*/

/* [Mass Storage Bulk-Only Transfer Communication].
	As if it couldn't get any more absurd, the mass storage specification (1) only lightly
	describes what the class needs to do, and instead there are multiple different
	transportation methods that this class can use, such as Control/Bulk/Interrupt,
	Bulk-Only, UFI, etc. I chose the bulk-only transport (BOT) since that's what most flash drives
	seem to implement... but once again, the specification (2) for that just describes the
	"communication" method with (3) pretty much encapsulating the main idea. This transportation
	protocol doesn't describe at all how to make the device into something that the host will use
	as a flash drive. For that, a command set must be selected, with SCSI being seemingly also the
	de-facto standard. It's crazy. There's like five layers of protocol and specifications going on
	here, but whatever. That's just life, I guess.

	Anyways, if we want to send data to the host, then we do the following series of transactions:
	...................................................................
	: =========================      =============     =============  :
	: ( COMMAND-BLOCK WRAPPER )  --> (  DATA 1/4 ) --> (  DATA 2/4 )  :
	: ======== BULK-OUT =======      == BULK-IN ==     == BULK-IN ==  :
	:                                                          |      :
	:                                                          v      :
	: ==========================     =============      ============= :
	: ( COMMAND-STATUS WRAPPER ) <-- (  DATA 4/4 )  <-- (  DATA 3/4 ) :
	: ======== BULK-IN =========     == BULK-IN ==      == BULK-IN == :
	:                                                                 :
	............. EXAMPLE OF DATA FLOWING FROM DEVICE TO HOST  ........

	This is extremely similar to how the default control endpoint carries out SETUP-transaction,
	but since control-typed endpoints are the only ones that are bidirectional, two endpoints must
	be used for this mass storage transaction. The first transaction transports the
	struct USBMSCommandBlockWrapper data from host to device, which describe things like the
	amount of data the host is expecting in the later BULK-IN-transactions. Assuming the device has
	enough data to actually send to the host, the first couple of BULK-IN-transactions will be part
	of the "data". Once the amount of expected data has been sent, the last BULK-IN-transaction will
	be for the command status. The command status just helps make the host sure that nothing went
	wrong or whatever.

	... but what if there isn't enough data to send? Or if we can't fulfill a command? Well, we
	have to do something pretty weird:
	....................................................................
	: =========================             ===============            :
	: ( COMMAND-BLOCK WRAPPER )   -->       (   !STALL!   )            :
	: ======== BULK-OUT =======             === BULK-IN ===            :
	:                                              |                   :
	:                                              v                   :
	:  ==========================     ================================ :
	:  ( COMMAND-STATUS WRAPPER ) <-- ( CLEAR BULK-IN ENDPOINT STALL ) :
	:  ======== BULK-IN =========     ============ DEFAULT =========== :
	:                                                                  :
	........... EXAMPLE OF REPONSE TO INVALID COMMAND-BLOCK  ...........

	When we receive a command-block wrapper that we can't handle or is invalid or for whatever
	reason, then we stall the BULK-IN endpoint (if the host is sending data to us, then the
	BULK-OUT endpoint is the one that is stalled). When the host attempts to perform a
	transaction on the corresponding bulk-typed endpoint, it'll receive a STALL-HANDSHAKE and the
	host driver will recognize that the command failed. A SETUP-transaction is then sent to the
	default endpoint requesting to clear the stall condition on the bulk-typed endpoint and then
	the host requests for the command status. From there on, we continue onto the next
	command/data/status transfer.

	The transfer for data flowing from host to device is pretty much identical, except the BULK-OUT
	endpoint is now being the one used in the data stage.
	.................................................................
	: =========================      ============     ============  :
	: ( COMMAND-BLOCK WRAPPER )  --> ( DATA 1/4 ) --> ( DATA 2/4 )  :
	: ======== BULK-OUT =======      = BULK-OUT =     = BULK-OUT =  :
	:                                                       |       :
	:                                                       v       :
	: ==========================     ============     ============  :
	: ( COMMAND-STATUS WRAPPER ) <-- ( DATA 4/4 ) <-- ( DATA 3/4 )  :
	: ======== BULK-IN =========     = BULK-OUT =     = BULK-OUT =  :
	:                                                               :
	......... EXAMPLE OF DATA FLOWING FROM HOST TO DEVICE  ..........

	In the case that no data is to be transferred, then there won't be a data stage. The host will
	send a command-block wrapper immediately followed by a request for the command-status wrapper.
	If we can't carry out the command, then stalling the bulk-typed endpoints isn't necessary
	either; we can also just immediately send the command-status wrapper with the status code
	indicating failure.

	Alright, so all of this is just to be able to send commands back and forth using SCSI, an
	ancient command set from the 90s that interfaced with hard drives and printers and all sorts of
	random stuff, much like USB. The SCSI specification in its own right is a mess, but I am
	honestly too tired to really get into that, but essentially we just send some info to the host
	beforehand about what kind of SCSI device we are and what not. After that, the host can begin
	to query and write to specific sectors of our device, mimicking a flashdrive.

	(1) Base Mass Storage Specification @ Source(11).
	(2) Bulk-Only Transport Specification @ Source(12).
	(3) "Command/Data/Status Flow" @ Source(12) @ Figure(1) @ AbsPage(12).
*/

/* [Mass Storage - OCR].
	When the phone send the screenshot BMP to us, we must process it as it is being received as
	there is no way on god's green earth will it be possible to have the entire image in memory.
	But this is not exactly trivial, since all we see on our side is just what sectors the host
	wants to write to and what the bytes are, and these sectors will not necessarily be related to
	the BMP (it could just be some file system junk). Luckily, we have ways to circumvent this
	issue, first with some reasonable assumptions:

		1. BMP data will be written linearly.

			All BMPs will begin with the magic characters 'B' and 'M'. Checking this along with
			some other fields, we will be able to determine the exact starting sector of the BMP
			data, and all following BMP data will be described in future sectors.

			However, there's nothing really stopping the host from just sending the entire BMP
			in reverse order! That is, write the last sector, then second to last, and all the way
			to the very first sector. This is stupid of course, but this is exactly what happens,
			well not exactly, but Windows File Explorer seems to do the funky thing of writing BMP
			data out-of-order. It'd initially begin at the start, but after the first couple
			clusters, it'll begin at the second half of the BMP or so, finish up, and then jump
			back. This makes doing OCR pretty much impractical on Windows when uploading the BMP
			via File Explorer, but TeraCopy, and more importantly Apple, seem to actually write the
			BMP linearly, which is fortunate.

		2. Initial BMP data is cluster-aligned.

			To actually determine when the BMP data has begun as decribed in (1.), we could check
			each and every sector that will be written, but this is pretty slow and
			actually overcompensating, since the BMP is just a file in the FAT32 file system,
			it'll have to start at some particular cluster. This fact also plays into helping us
			determine the starting BMP sector, as it won't be somewhere like in the middle of a
			cluster.

			Interestingly though, this assumption does not seem to hold for the rest of the BMP
			data. I don't have proof of it, but I believe Apple will do a partial cluster write,
			and not one in its entirety in one command; works fine on Windows though. In other
			words, we can assume the the start of the BMP data is cluster-aligned, but not much
			else.

		3. Transactions are exclusive.

			We assume entire transactions are exclusively reserved for the BMP or not, and nothing
			in between. In other words, we will not be expecting for the host to send a single
			write command that updates the FAT while somehow simultaneously send BMP data in that
			same series of sectors that are being sent.

		3. BMP pixel data is uniquely determined by the alpha channel.

			For some strange reason, it's pretty frequent for the host to send a small portion
			of BMP data, but then send some file system junk, and then some more BMP data. For the
			most part, this won't be an issue, as we can check the alpha channel of the pixels
			(or where they'd at least be) in the sector. If we find that they're not all 0xFF, then
			the sector is likely junk that's unrelated to BMP pixel data. If they are all 0xFF,
			then high chances it's pixel data. This only works at all because the screenshot BMP
			doesn't make any sort of transparency, and these 0xFF bytes are pretty much for
			padding.

			Of course there's a chance of a false positive, if the host happens to send us a sector
			consisting of entirely 0xFF, but something like this doesn't seem to ever happen, so
			good for us! The only thing that'd get close to this is the FAT region, but because of
			(2.), this is not in the data region of the file system, so we wouldn't intercept this
			writing transaction for BMP data anyways.

			Also because of (3.), we will only check the first sector of the transaction to see if
			it's likely pixel data, and assume the rest of the sectors in the transaction will
			follow suit. This stems from the idea that if we receive a sector that appears like
			pixel data, but it is only until sector #300 of that transaction that it turns out it's
			not, then we would have to roll back all of our processing work to the very start. This
			case can possibly happen by all means, but in practice does not.

	With these assumptions, we can decide whether or not we should intercept the write transaction
	to do OCR. When we do intercept the transaction, we can process the sectors one at a time and
	eventually figure out each letter of the slots within the image.

	Of course, when we do have the sectors that we're confident contains BMP pixels, process them
	to get slot pixels overall is tricky. Firstly, we receive the entire image top-down from Apple.
	It's not a huge deal honestly, but makes things needlessly more not so simple when I have
	everything else be bottom-up. Second, the pixel data themselves are not exactly aligned, that
	is, each sector of BMP data (except for the first which contains the headers) begins with the
	latter half of a pixel that the previous BMP data sector truncated at the end. This is only a
	thing because of the BMP header from the very first sector offsets the whole pixel data by
	some bytes.

	In other words, instead of the pixel data looking like:

		BGRA - BGRA - BGRA - BGRA - ... -  BGRA - BGRA - BGRA - BGRA,

	It's instead:

		RA - BGRA - BGRA - BGRA - ... -  BGRA - BGRA - BGRA - BG.

	Ugly! But whatever. At least the positions of the pixel channels doesn't shift around
	sector-to-sector!

	The next thign to note is that the BMP data, because of assumption (1.) and the fact that
	Apple makes BMP top-down, we essentially receive a stream of pixels going left to right from
	the top to the bottom. Thus, we can't singly OCR a slot on its own; we have to do it for an
	entire row of slots in the wordgame! If we don't, then we are discarding pixels of another
	slot, and there's no way to have those be resent, unless the entire image is resent again, but
	that's absolutely stupid to do, so must make use of all the data that we receive efficiently
	and effectively.

	As for how the OCR process actually goes, we collect all the pixels that we receive from the
	sectors the host sends us until we have an entire pixel row of a single slot. We can then
	compare this row of pixels to each mask (see: [OCR - Mask Stream]) and add points to each
	respective letter. If the row of pixels of the slot from the screenshot matches perfectly with
	a row of pixels of a mask, the letter of that mask will receive the most points. Any
	differences will reduce the score added.

	But as stated earlier, we can't conclude what that slot's letter is until we have completely
	compared all of the pixel rows, but the next series of pixels will usually be the next
	horizontally adjacent slot! So we repeat again and again, until we reach the bottom of all of
	those slots. It is only then can we determine the most likely letter of each of those slots.

	There's also padding pixels between each slot that needs to be accounted for, but it's not too
	big of a deal.

	Overall the OCR is pretty damn fast. The wordgame that takes the longest is WordBites, since
	it's a 8x9 grid of slots. Unoptimized, it took about 23 seconds to determine the letters and
	their positions on the board. But with some observations, every WordBites piece are never in
	direct adjacency with another piece, thus we can avoid having to extract activated pixels and
	perform comparisons of a lot of slots since we know for a fact they'll just be empty space.
	Simple exits like these cuts the processing time of WordBites around half, to 13 seconds total.

	Later, I made another optimization where we skip writing to the SD card entirely when we're
	doing OCR. The storage medium is still important to keep around and have up to date since it
	contains a bunch of junk file system stuff that the OS needs, but for the BMP data itself,
	we really don't actually have to bother with saving that. Thus, we can completely remove
	that from the OCR entirely, making processing WordBites take around 8 seconds! This does mean
	that we can't use the cropped screenshots from the SD card whenever we need to do some
	debugging since those files would be pretty much be pointing to junk, but this is mostly fine.
*/

/* [OCR - Mask Stream].
	If there are 44 letters and MASK_DIM = 64, then having a packed monochrome MASK_DIM x MASK_DIM
	BMP mask of each of those letters will take 44*64*64/8 = 22,528 bytes of flash. This is
	obviously way too much, so what we have to do is perform some sort of compression on it to get
	it down to acceptable levels.

	My first attempt at compression was through "row-reduction" where I mark the amount of empty
	rows from the top and bottom of a letter's mask. Thus by having a single byte where the nibbles
	represents amount of empty rows, quite a bit of space could be saved, but was still
	insufficient. Can't remember exact numbers, but it probably dropped the original 22KB to ~17KB,
	which didn't leave enough space for the rest of the program.

	Then I thought about maybe reducing the mask size, but we have to then consider the tradeoffs
	between that and accuracy. Obviously if we reduce MASK_DIM to something smaller like 32, then
	the mask image quality will also be reduced. To a human, the letters are still quite
	distinguishable, but the key issue with this is the fact that it's not very error-tolerant.
	The screenshot BMP that contains the letters that we need to process will not perfectly line up
	with our masks. For the most part, this is fine, we just pick whatever mask fits the letter
	the most; there'll just be an "edge" of error surrounding the letter is all. But if we half
	MASK_DIM from 64 to 32, then the ratio of the error around the perimeter with the area of
	matching pixels will be 4, that is, the error will quadruple if we cut MASK_DIM by half, per
	inverse square law. This effect is quite apparent when a slot's letter and its corresponding
	mask have a small rectangle of mismatch that is just big enough to fool the OCR into thinking
	it's the incorrect letter.

	So in short, it's important that we have a large MASK_DIM, bigger the better, as it'll reduce
	the amount of relative error. The only downside to this is it'll take up more flash memory and
	more time to process. Because of this, reducing mask size is not really an option. It'll make
	the OCR too unreliable.

	Alright then, so I thought about maybe representing each row of the individual masks with some
	sort of run-length encoding. For letters like 'W', a row may need up to 5/6 bytes to indicate
	the amount of white/black pixels there are in a row. This was never implemented, mainly cause
	it was pretty sucky.

	But the crucial insight then came to me when I realized how the OCR was actually accessing the
	mask data. As described in [Mass Storage - OCR], a single slot is never processed at a time,
	but an entire row, specifically from the top to the bottom. So rather than store each letter's
	mask BMP individually, we instead store it by sorting all the top rows of the masks in
	sequence, followed by the second row from the top, and on and on until the very bottom rows.
	From there, we can then perform a straight-forward run-length compression on it. Each
	run-length is assumed to be byte-sized, but if it's zero, then we pop a word-sized run-length
	from a separate place. This is to handle long stretches of empty rows, but have it not be
	incredibly inefficient since most run-lengths are only a dozen pixels or so. Since the masks
	are monochrome, it's obvious that the run-lengths will alternate in color/activation, where the
	first run-length represents black/inactive pixels.

	Thus we have a stream of run-lengths that the OCR can decode easily to get bytes of masks to
	compare slots against, going from 22,528 bytes to ~7,299 bytes, ~33% of original size!
*/

/* [Endpoint Sizes].
	The datasheet for some reason lists 512 bytes as a possible endpoint buffer size (1), but really
	the largest any endpoint buffer could be is 256 bytes (2). The AT90USB64/128 also have
	USB capabilities (furthermore USB host support), and its datasheet doesn't even show that
	512 bytes is possible (3). So very likely that this was some weird mistake that crept through,
	ugh...

	On top of that, setting an endpoint size to be bigger than 64 seems to act weird on my PC.
	The USB analyzer says that the SetConfiguration request is returned with a status of
	"0xC0001000" which apparently means "insufficient resources"? Doesn't many any sense at all
	since it works on my laptop and Apple is fine with it. Likely some weird obscure driver bug...?

	(1) "512 bytes" @ Source(1) @ Section(22.18.2) @ Page(287).
	(2) "Introduction" @ Source(1) @ Section(22.1) @ Page(270).
	(3) Endpoint Sizes @ Source(21) @ Section(23.18.2) @ Page(279).
*/

/* TODO[USB Regulator vs Interface]
	There's a different bit for enabling the USB pad regulator and USB interface,
	but why are these separate? Perhaps for saving state?
*/
