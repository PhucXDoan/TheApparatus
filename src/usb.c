#undef  PIN_HALT_SOURCE
#define PIN_HALT_SOURCE PinHaltSource_usb

// HELD must evaluate to 0 or 1, DEST_X must be within [0, 127], DEST_Y must be within [0, 255]. See: [Mouse Commands].
#define usb_mouse_command(HELD, DEST_X, DEST_Y) usb_mouse_command_(((HELD) << 15) | ((DEST_X) << 8) | (DEST_Y))
static void
usb_mouse_command_(u16 command)
{
	pin_high(PIN_USB_SPINLOCKING);
	while (_usb_mouse_command_writer_masked(1) == _usb_mouse_command_reader_masked(0)); // Our write-cursor is before the interrupt's read-cursor.
	_usb_mouse_command_buffer[_usb_mouse_command_writer_masked(0)] = command;
	_usb_mouse_command_writer += 1;
	pin_low(PIN_USB_SPINLOCKING);
}

static void
usb_init(void)
{ // [USB Initialization Process].

	pin_output(PIN_USB_SPINLOCKING);

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
	pin_high(PIN_USB_SPINLOCKING);
	for (u8 i = 0; i != 255; i += 1)
	{
		if (debug_usb_diagnostic_signal_received)
		{
			break;
		}
		else
		{
			_delay_ms(8.0);
		}
	}
	pin_low(PIN_USB_SPINLOCKING);
	#endif
}

ISR(USB_GEN_vect)
{ // [USB Device Interrupt Routine].

	if (UDINT & (1 << EORSTI)) // End-of-Reset.
	{
		for (u8 i = 0; i < countof(USB_ENDPOINT_UECFGNX); i += 1)
		{
			if (pgm_read_byte(&USB_ENDPOINT_UECFGNX[i][1])) // Elements that aren't explicitly assigned in USB_ENDPOINT_UECFGNX will have the ALLOC bit cleared.
			{
				UENUM   = i;           // "Select the endpoint".
				UECONX  = (1 << EPEN); // "Activate endpoint".

				// "Configure and allocate".
				UECFG0X = pgm_read_byte(&USB_ENDPOINT_UECFGNX[i][0]);
				UECFG1X = pgm_read_byte(&USB_ENDPOINT_UECFGNX[i][1]);

				// "Test endpoint configuration".
				#if DEBUG
				if (!(UESTA0X & (1 << CFGOK)))
				{
					error; // Hardware determine that this endpoint's configuration is invalid.
				}
				#endif
			}
		}

		// Enable the interrupt source of the event that endpoint 0 receives a SETUP-transaction.
		UENUM  = USB_ENDPOINT_DFLT;
		UEIENX = (1 << RXSTPE);
	}

	if (UDINT & (1 << SOFI)) // Start-of-Frame.
	{
		#if DEBUG
		UENUM = USB_ENDPOINT_CDC_IN;
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

		#if DEBUG
		UENUM = USB_ENDPOINT_CDC_OUT;
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

		UENUM = USB_ENDPOINT_HID;
		if (UEINTX & (1 << TXINI)) // See: [Mouse Commands].
		{
			i8 delta_x = 0;
			i8 delta_y = 0; // Positive makes the mouse move downward.

			if (_usb_mouse_calibrations < USB_MOUSE_CALIBRATIONS_REQUIRED)
			{
				delta_x                  = -128;
				delta_y                  = -128;
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

		switch (_usb_ms_state)
		{
			case USBMSState_ready_for_command:
			{
				UENUM = USB_ENDPOINT_MS_OUT;
				if (UEINTX & (1 << RXOUTI))
				{
					struct USBMSCommandBlockWrapper command = {0};
					static_assert(sizeof(command) <= USB_ENDPOINT_MS_OUT_SIZE);

					if (UEBCX == sizeof(command))
					{
						for (u8 i = 0; i < sizeof(command); i += 1)
						{
							((u8*) &command)[i] = UEDATX;
						}

						if // Is command valid and meaningful?
						(
							command.dCBWSignature == USB_MS_COMMAND_BLOCK_WRAPPER_SIGNATURE &&
							!(command.bmCBWFlags & 0b0111'1111) &&
							command.bCBWLUN == 0 &&
							1 <= command.bCBWCBLength && command.bCBWCBLength <= 16
						)
						{
							switch (command.CBWCB[0])
							{
								case USBMSSCSIOpcode_test_unit_ready:
								{
									if
									(
										!command.bmCBWFlags &&
										command.bCBWCBLength == 6 &&
										command.dCBWDataTransferLength == 0 &&
										command.CBWCB[5] == 0 // "CONTROL", just in case it's not zero.
									)
									{
										// TODO Apparently we just don't send anything back?
									}
									else
									{
										debug_pin_u8(1);
										debug_unhandled;
									}
								} break;

								case USBMSSCSIOpcode_inquiry: // See: Source(13) @ Section(3.6.1) @ Page(92).
								{
									if
									(
										command.bmCBWFlags &&
										command.bCBWCBLength == 6 &&
										command.dCBWDataTransferLength == (u16) ((command.CBWCB[3] << 8) | command.CBWCB[4]) &&
										command.CBWCB[5] == 0 && // "CONTROL", just in case it's not zero.
										(
											(
												command.CBWCB[1] == 0 && // The host requests for the standard inquiry data.
												command.CBWCB[2] == 0    // In this case, the page code should always be zero.
											) ||
											(
												command.CBWCB[1] == 0x01 && // The host requests for "vital product data".
												command.CBWCB[2] == 0x80    // Page code for "Unit Serial Number". See: Source(13) @ Section(5.4.19) @ Page(510).
											)
										)
									)
									{
										// Send the standard inquiry data, even if the host asked for the unit serial number,
										// since that data happens to also look identical to the standard inquiry data.

										_usb_ms_scsi_info_data = USB_MS_SCSI_INQUIRY_DATA;
										_usb_ms_scsi_info_size = sizeof(USB_MS_SCSI_INQUIRY_DATA);
										static_assert(sizeof(USB_MS_SCSI_INQUIRY_DATA) <= USB_ENDPOINT_MS_IN_SIZE);
									}
									else
									{
										debug_pin_u8(2);
										debug_unhandled;
									}
								} break;

								case USBMSSCSIOpcode_mode_sense: // TODO Absolute fkin' mystery. See: Source(13) @ Section(3.11) @ Page(111).
								{
									if
									(
										command.bmCBWFlags &&
										command.bCBWCBLength == 6 &&
										command.dCBWDataTransferLength == command.CBWCB[4] &&
										command.CBWCB[5] == 0 && // "CONTROL", just in case it's not zero.
										command.CBWCB[1] == 0 &&
										(command.CBWCB[2] == 0x1C || command.CBWCB[2] == 0x08 || command.CBWCB[2] == 0x3F) && // "PC" | "PAGE CODE"
										command.CBWCB[3] == 0       // "SUBPAGE CODE"
									)
									{
										_usb_ms_scsi_info_data = USB_MS_SCSI_MODE_SENSE;
										_usb_ms_scsi_info_size = sizeof(USB_MS_SCSI_MODE_SENSE);
										static_assert(sizeof(USB_MS_SCSI_MODE_SENSE) <= USB_ENDPOINT_MS_IN_SIZE);
									}
								} break;

								case USBMSSCSIOpcode_read_format_capacities: // See: Source(14) @ Table(701) @ AbsPage(1).
								{
									if
									(
										command.bmCBWFlags &&
										command.bCBWCBLength == 10 &&
										command.dCBWDataTransferLength == (u16) ((command.CBWCB[7] << 8) | command.CBWCB[8]) &&
										command.CBWCB[9] == 0 // "CONTROL", just in case it's not zero.
									)
									{
										_usb_ms_scsi_info_data = USB_MS_SCSI_READ_FORMAT_CAPACITIES_DATA;
										_usb_ms_scsi_info_size = sizeof(USB_MS_SCSI_READ_FORMAT_CAPACITIES_DATA);
										static_assert(sizeof(USB_MS_SCSI_READ_FORMAT_CAPACITIES_DATA) <= USB_ENDPOINT_MS_IN_SIZE);
									}
									else
									{
										debug_pin_u8(3);
										debug_unhandled;
									}
								} break;

								case USBMSSCSIOpcode_read_capacity: // See: Source(13) @ Section(3.22) @ Page(155).
								{
									if
									(
										command.bmCBWFlags &&
										command.bCBWCBLength == 10 &&
										command.dCBWDataTransferLength == sizeof(USB_MS_SCSI_READ_CAPACITY_DATA) &&
										command.CBWCB[9] == 0 // "CONTROL", just in case it's not zero.
									)
									{
										_usb_ms_scsi_info_data = USB_MS_SCSI_READ_CAPACITY_DATA;
										_usb_ms_scsi_info_size = sizeof(USB_MS_SCSI_READ_CAPACITY_DATA);
										static_assert(sizeof(USB_MS_SCSI_READ_CAPACITY_DATA) <= USB_ENDPOINT_MS_IN_SIZE);
									}
									else
									{
										debug_pin_u8(4);
										debug_unhandled;
									}
								} break;

								case USBMSSCSIOpcode_read:
								{
									u32 sector_count = ((command.CBWCB[7] << 8) | command.CBWCB[8]);
									if
									(
										command.bmCBWFlags &&
										command.bCBWCBLength == 10 &&
										command.dCBWDataTransferLength == sector_count * FAT32_SECTOR_SIZE &&
										command.CBWCB[1] == 0 && // Some random bits??
										command.CBWCB[6] == 0 && // "GROUP NUMBER"
										command.CBWCB[9] == 0 // "CONTROL", just in case it's not zero.
									)
									{
										_usb_ms_abs_sector_address =
											(((u32) command.CBWCB[2]) << 24) |
											(((u32) command.CBWCB[3]) << 16) |
											(((u32) command.CBWCB[4]) <<  8) |
											(((u32) command.CBWCB[5]) <<  0);
										_usb_ms_sectors_left_to_send          = sector_count;
										_usb_ms_sending_sector_fragment_index = 0;
									}
									else
									{
										debug_pin_u8(5);
										debug_unhandled;
									}
								} break;

								case USBMSSCSIOpcode_write: // TODO For some reason, this is sent when we open Device Monitoring Studio afterwards...?
								{
									debug_pin_u8(6);
									debug_unhandled;
								} break;

								case USBMSSCSIOpcode_sync_cache:
								{
									if
									(
										!command.bmCBWFlags &&
										command.bCBWCBLength == 10 &&
										command.dCBWDataTransferLength == 0 &&
										command.CBWCB[1] == 0 && !memcmp(command.CBWCB + 1, command.CBWCB + 2, countof(command.CBWCB) - 2)
									)
									{
										// TODO Nothing?
									}
									else
									{
										debug_pin_u8(5);
										debug_unhandled;
									}
								} break;

								default:
								{
									debug_pin_u8(7);
									debug_unhandled;
								} break;
							}

							if (_usb_ms_scsi_info_size)
							{
								_usb_ms_state  = USBMSState_sending_data;
								_usb_ms_status =
									(struct USBMSCommandStatusWrapper)
									{
										.dCSWSignature   = USB_MS_COMMAND_STATUS_WRAPPER_SIGNATURE,
										.dCSWTag         = command.dCBWTag,
										.dCSWDataResidue = command.dCBWDataTransferLength - _usb_ms_scsi_info_size,
										.bCSWStatus      = 0x00,
									};
							}
							else if (_usb_ms_sectors_left_to_send)
							{
								_usb_ms_state  = USBMSState_sending_data;
								_usb_ms_status =
									(struct USBMSCommandStatusWrapper)
									{
										.dCSWSignature   = USB_MS_COMMAND_STATUS_WRAPPER_SIGNATURE,
										.dCSWTag         = command.dCBWTag,
										.dCSWDataResidue = 0,
										.bCSWStatus      = 0x00,
									};
							}
							else if (!command.dCBWDataTransferLength)
							{
								_usb_ms_state  = USBMSState_ready_for_status;
								_usb_ms_status =
									(struct USBMSCommandStatusWrapper)
									{
										.dCSWSignature   = USB_MS_COMMAND_STATUS_WRAPPER_SIGNATURE,
										.dCSWTag         = command.dCBWTag,
										.dCSWDataResidue = 0,
										.bCSWStatus      = 0x00,
									};
							}
							else
							{
								debug_pin_u8(255);
								debug_unhandled;
							}
						}
						else // The CBW isn't valid or meaningful. See: Source(12) @ Section(6.2) @ Page(17).
						{
							debug_pin_u8(8);
							debug_unhandled;
						}
					}
					else // We supposedly received a CBW packet that's not 31 bytes in length. See: Source(12) @ Section(6.2.1) @ Page(17).
					{
						debug_pin_u8(9);
						debug_unhandled;
					}

					UEINTX &= ~(1 << RXOUTI);
					UEINTX &= ~(1 << FIFOCON);
				}
			} break;

			case USBMSState_sending_data:
			{
				UENUM = USB_ENDPOINT_MS_IN;
				if (UEINTX & (1 << TXINI))
				{
					if (_usb_ms_scsi_info_size)
					{
						for (u8 i = 0; i < _usb_ms_scsi_info_size; i += 1)
						{
							UEDATX = ((u8*) &_usb_ms_scsi_info_data)[i];
						}
						_usb_ms_scsi_info_size = 0;
					}
					else if (_usb_ms_sectors_left_to_send)
					{
						const u8* sector_data = 0;

						switch (_usb_ms_abs_sector_address)
						{
							#define MAKE(SECTOR_DATA, SECTOR_ADDRESS) \
								case (SECTOR_ADDRESS): \
									static_assert(sizeof(SECTOR_DATA) == 512); \
									sector_data = (u8*) &(SECTOR_DATA); \
									break;
							FAT32_SECTOR_XMDT(MAKE)
							#undef MAKE
						}

						if (sector_data)
						{
							for (u8 i = 0; i < USB_ENDPOINT_MS_IN_SIZE; i += 1)
							{
								UEDATX = pgm_read_byte(&sector_data[_usb_ms_sending_sector_fragment_index * USB_ENDPOINT_MS_IN_SIZE + i]);
							}
						}
						else
						{
							for (u8 i = 0; i < USB_ENDPOINT_MS_IN_SIZE; i += 1)
							{
								UEDATX = 0;
							}
						}

						if (_usb_ms_sending_sector_fragment_index == FAT32_SECTOR_SIZE / USB_ENDPOINT_MS_IN_SIZE - 1)
						{
							_usb_ms_sending_sector_fragment_index  = 0;
							_usb_ms_sectors_left_to_send          -= 1;
							_usb_ms_abs_sector_address            += 1;
						}
						else
						{
							_usb_ms_sending_sector_fragment_index += 1;
						}
					}

					if (!_usb_ms_sectors_left_to_send)
					{
						_usb_ms_state = USBMSState_ready_for_status;
					}

					UEINTX &= ~(1 << TXINI);
					UEINTX &= ~(1 << FIFOCON);
				}
			} break;

			case USBMSState_ready_for_status:
			{
				UENUM = USB_ENDPOINT_MS_IN;
				if (UEINTX & (1 << TXINI))
				{
					static_assert(sizeof(_usb_ms_status) <= USB_ENDPOINT_MS_IN_SIZE);
					for (u8 i = 0; i < sizeof(_usb_ms_status); i += 1)
					{
						UEDATX = ((u8*) &_usb_ms_status)[i];
					}

					UEINTX &= ~(1 << TXINI);
					UEINTX &= ~(1 << FIFOCON);

					_usb_ms_state  = USBMSState_ready_for_command;
					_usb_ms_status = (struct USBMSCommandStatusWrapper) {0}; // TODO Obviously we shouldn't waste time clearing the entire thing.
				}
			} break;
		}
	}

	UDINT = 0; // Clear interrupt flags to prevent this routine from executing again.
}

static void
_usb_endpoint_0_in_pgm(const u8* payload_data, u16 payload_length)
{ // [Endpoint 0: Data-Transfer from Device to Host].

	const u8* reader    = payload_data;
	u16       remaining = payload_length;
	while (true)
	{
		if (UEINTX & (1 << RXOUTI)) // Received OUT-transaction?
		{
			UEINTX &= ~(1 << RXOUTI); // Clear interrupt flag for this OUT-transaction. See: Pseudocode @ Source(2) @ Section(22.12.2) @ Page(275).
			break;
		}
		else if (UEINTX & (1 << TXINI)) // Endpoint 0's buffer ready to be filled?
		{
			u8 writes_left = remaining;
			if (writes_left > USB_ENDPOINT_DFLT_SIZE)
			{
				writes_left = USB_ENDPOINT_DFLT_SIZE;
			}

			remaining -= writes_left;
			while (writes_left)
			{
				UEDATX       = pgm_read_byte(reader);
				reader      += 1;
				writes_left -= 1;
			}

			UEINTX &= ~(1 << TXINI); // Endpoint 0's buffer is ready for the next IN-transaction.
		}
	}
}

ISR(USB_COM_vect)
{ // See: [USB Endpoint Interrupt Routine].

	UENUM = USB_ENDPOINT_DFLT;
	if (UEINTX & (1 << RXSTPI)) // [Endpoint 0: SETUP-Transactions]. // TODO Remove this if-statement if we in the end aren't adding any other interrupt source.
	{
		UECONX |= (1 << STALLRQC); // SETUP-transactions lift STALL conditions. See: Source(2) @ Section(8.5.3.4) @ Page(228) & [Endpoint 0: Request Error].

		struct USBSetupRequest request;
		for (u8 i = 0; i < sizeof(request); i += 1)
		{
			((u8*) &request)[i] = UEDATX;
		}
		UEINTX &= ~(1 << RXSTPI); // Let the hardware know that we finished copying the SETUP-transaction's data-packet. See: Source(1) @ Section(22.12) @ Page(274).

		switch (request.type)
		{
			case USBSetupRequestType_get_desc:     // [Endpoint 0: GetDescriptor].
			case USBSetupRequestType_hid_get_desc: // [Endpoint 0: GetDescriptor].
			{
				u8        payload_length = 0; // Any payload we send will not exceed 255 bytes.
				const u8* payload_data   = 0;

				if (request.type == USBSetupRequestType_get_desc)
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
								payload_data   = (const u8*) &USB_CONFIG_HIERARCHY;
								payload_length = sizeof(USB_CONFIG_HIERARCHY);
								static_assert(sizeof(USB_CONFIG_HIERARCHY) < (((u64) 1) << bitsof(payload_length)));
							}
						} break;

						case USBDescType_device_qualifier: // See: Source(2) @ Section(9.6.2) @ Page(264).
						case USBDescType_string:           // See: [USB Strings] @ File(defs.h).
						{
							// We induce STALL condition. See: [Endpoint 0: Request Error].
						} break;

						default:
						{
							debug_pin_u8(10);
							debug_unhandled;
						} break;
					}
				}
				else if
				(
					request.hid_get_desc.interface_number == USB_HID_INTERFACE_INDEX &&
					request.hid_get_desc.desc_type        == USBDescType_hid_report
				)
				{
					payload_data   = (const u8*) &USB_DESC_HID_REPORT;
					payload_length = sizeof(USB_DESC_HID_REPORT);
					static_assert(sizeof(USB_DESC_HID_REPORT) < (((u64) 1) << bitsof(payload_length)));
				}
				else
				{
					debug_pin_u8(11);
					debug_unhandled;
				}

				if (payload_length)
				{
					_usb_endpoint_0_in_pgm // TODO Inline?
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

			case USBSetupRequestType_set_address: // [Endpoint 0: SetAddress].
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

			case USBSetupRequestType_set_config: // [Endpoint 0: SetConfiguration].
			{
				switch (request.set_config.value)
				{
					case 0:
					{
						error; // In the case that the host, for some reason, wants to set the device back to the "address state", we should handle this.
					} break;

					case USB_CONFIG_HIERARCHY_ID:
					{
						UEINTX &= ~(1 << TXINI); // Send out zero-length data-packet for the host's upcoming IN-transaction to acknowledge this request.
					} break;

					default:
					{
						UECONX |= (1 << STALLRQ);
					} break;
				}
			} break;

			case USBSetupRequestType_cdc_set_line_coding: // [Endpoint 0: CDC-Specific SetLineCoding].
			{
				if (request.cdc_set_line_coding.incoming_line_coding_datapacket_size == sizeof(struct USBCDCLineCoding))
				{
					while (!(UEINTX & (1 << RXOUTI))); // Wait for the completion of an OUT-transaction.

					// Copy data-packet of OUT-transaction.
					struct USBCDCLineCoding desired_line_coding;
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
							*(u16*) 0x0800 = 0x7777; // Magic!
							wdt_enable(WDTO_15MS);
							for(;;);
						} break;

						#if DEBUG
						case DIAGNOSTIC_BAUD_SIGNAL:
						{
							debug_usb_diagnostic_signal_received = true;
						} break;
						#endif
					}
				}
				else
				{
					UECONX |= (1 << STALLRQ);
				}
			} break;

			case USBSetupRequestType_ms_get_max_lun: // [Endpoint 0: MS-Specific GetMaxLUN].
			{
				while (!(UEINTX & ((1 << RXOUTI) | (1 << TXINI))));

				if (UEINTX & (1 << TXINI))
				{
					UEDATX  = 0;
					UEINTX &= ~(1 << TXINI);
				}

				if (UEINTX & (1 << RXOUTI))
				{
					UEINTX &= ~(1 << RXOUTI);
				}
			} break;

			case USBSetupRequestType_endpoint_clear_feature:
			{
				if (request.endpoint_clear_feature.feature_selector == 0)
				{
					switch (request.endpoint_clear_feature.endpoint_index)
					{
						case USB_ENDPOINT_MS_IN | USB_ENDPOINT_MS_IN_TRANSFER_DIR:
						{
							UENUM   = USB_ENDPOINT_MS_OUT;
							UECONX |= (1 << STALLRQC);
						} break;

						case USB_ENDPOINT_MS_OUT | USB_ENDPOINT_MS_OUT_TRANSFER_DIR:
						{
							UENUM   = USB_ENDPOINT_MS_OUT;
							UECONX |= (1 << STALLRQC);
						} break;

						default:
						{
							debug_unhandled;
						} break;
					}
				}
				else
				{
					debug_unhandled;
				}
			} break;

			case USBSetupRequestType_cdc_get_line_coding:        // [Endpoint 0: Extraneous CDC-Specific Requests].
			case USBSetupRequestType_cdc_set_control_line_state: // [Endpoint 0: Extraneous CDC-Specific Requests].
			case USBSetupRequestType_hid_set_idle:               // [Endpoint 0: HID-Specific SetIdle].
			{
				UECONX |= (1 << STALLRQ);
			} break;

			default:
			{
				UECONX |= (1 << STALLRQ);
				debug_unhandled;
			} break;
		}
	}
}

#if DEBUG
static void
debug_tx_chars(char* value, u16 value_size)
{ // TODO we could use memcpy if we need to speed this up
	if (debug_usb_diagnostic_signal_received)
	{
		pin_high(PIN_USB_SPINLOCKING);
		for (u16 i = 0; i < value_size; i += 1)
		{
			while (debug_usb_cdc_in_writer_masked(1) == debug_usb_cdc_in_reader_masked(0)); // Our write-cursor is before the interrupt's read-cursor.
			debug_usb_cdc_in_buffer[debug_usb_cdc_in_writer_masked(0)] = value[i];
			debug_usb_cdc_in_writer += 1;
		}
		pin_low(PIN_USB_SPINLOCKING);
	}
}
#endif

#if DEBUG
static void
debug_tx_cstr(char* value)
{
	if (debug_usb_diagnostic_signal_received)
	{
		debug_tx_chars(value, strlen(value));
	}
}
#endif

#if DEBUG
static void
debug_tx_u64(u64 value)
{
	if (debug_usb_diagnostic_signal_received)
	{
		char buffer[20];
		u8   length = serialize_u64(buffer, countof(buffer), value);
		debug_tx_chars(buffer, length);
	}
}
#endif

#if DEBUG
static u8 // Amount of data copied into dst.
debug_rx(char* dst, u8 dst_max_length) // Note that PuTTY sends only '\r' (0x13) for keyboard enter.
{ // TODO we could use memcpy if we need to speed this up
	u8 result = 0;

	if (debug_usb_diagnostic_signal_received)
	{
		while (result < dst_max_length && debug_usb_cdc_out_reader_masked(0) != debug_usb_cdc_out_writer_masked(0))
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
	the MCU for reprogramming; a huge quality-of-life improvement. The endpoints used for
	transferring the diagnostics are only enabled in DEBUG mode. This is just to keep things slim
	and ensure that we aren't bloating the firmware with unnecessary code. Currently though, the
	entire CDC functionality is still active in the configuration regardless of the state of DEBUG.
	This just makes development a little bit easier, but a potential TODO is to have the CDC
	function be entirely omitted since it's not really used to directly contribute to anything.

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
	(13) Clearing EPEN @ Source(1) @ Section(22.6) @ Page(272).
	(14) "Endpoint Reset" @ Source(1) @ Section(22.3) @ Page(270).
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
	MCU down to a halt.

	(1) "USB Device Controller Interrupt System" @ Source(1) @ Figure(22-4) @ Page(279).
	(2) Endpoint Memory Management @ Source(1) @ Section(21.9) @ Page(263-264).
	(3) "Endpoint Selection" @ Source(1) @ Section(22.5) @ Page(271).
	(4) "USB Reset" @ Source(1) @ Section(22.4) @ Page(271).
	(5) "USB Device Controller Endpoint Interrupt System" @ Source(1) @ Figure(22-5) @ Page(280).
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

	But this is pretty much just for endpoint 0's SETUP-transactions.

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

	Within our USB_CONFIG_HIERARCHY, we define the "Communication Class Interface".
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
	USB_CONFIG_HIERARCHY).

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
	TODO
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
	configuration value of USB_CONFIG_HIERARCHY_ID (which it got from GetDescriptor) to set to our
	only configuration that is USB_CONFIG_HIERARCHY. This request would only really matter when we
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

/* [Endpoint 0: MS-Specific GetMaxLUN].
	Despite (1) saying that the device MAY stall on this, Window's driver seems to be persistent
	on doing this request. Eventually, the driver will move on and do the mass storage enumeration
	stuff, but there'd be a solid couple seconds of delay. So we just might as well handle this
	request.

	(1) "Get Max LUN" @ Source(12) @ Section(3.2) @ Page(7).
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

/* TODO[USB Regulator vs Interface]
	There's a different bit for enabling the USB pad regulator and USB interface,
	but why are these separate? Perhaps for saving state?
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
