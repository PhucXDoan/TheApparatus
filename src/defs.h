// TODO Interrupt behavior on USB disconnection?
// TODO is there a global jump that could be done for error purposes?
#define false            0
#define true             1
#define stringify_(X)    #X
#define concat_(X, Y)    X##Y
#define countof(...)     (sizeof(__VA_ARGS__)/sizeof((__VA_ARGS__)[0]))
#define stringify(X)     stringify_(X)
#define concat(X, Y)     concat_(X, Y)
#define static_assert(X) _Static_assert((X), #X);

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  b8;
typedef uint16_t b16;
typedef uint32_t b32;
typedef uint64_t b64;

//
// Pins.
//

enum PinState
{
	PinState_false    = 0b00, // Aliasing(false). Sink to GND.
	PinState_true     = 0b01, // Aliasing(true). Source of 5 volts.
	PinState_floating = 0b10, // Unconnected; reading from this pin is unreliable. Also called the "tri-state" or "Hi-Z" state.
	PinState_input    = 0b11, // Pull-up resistor enabled for reliable reads. A truthy value will be read unless it is tied to GND strongly enough.
};

// [Pin Mapping].
#define PIN_XMDT(X) \
	X( 0, D, 2) \
	X( 1, D, 3) \
	X( 2, D, 1) \
	X( 3, D, 0) \
	X( 4, D, 4) \
	X( 5, C, 6) \
	X( 6, D, 7) \
	X( 7, E, 6) \
	X( 8, B, 4) \
	X( 9, B, 5) \
	X(10, B, 6) \
	X(11, B, 7) \
	X(12, D, 6) \
	X(13, C, 7) \
	X(14, F, 7) \
	X(15, F, 6) \
	X(16, F, 5) \
	X(17, F, 4) \
	X(18, F, 1) \
	X(19, F, 0) \

//
// USB.
//

#define SIZEOF_ENDPOINT_SIZE_TYPE(X) (8 * (1 << (X)))
enum USBEndpointSizeType // See: Source(1) @ Section(22.18.2) @ Page(287).
{
	USBEndpointSizeType_8   = 0b000,
	USBEndpointSizeType_16  = 0b001,
	USBEndpointSizeType_32  = 0b010,
	USBEndpointSizeType_64  = 0b011,
	USBEndpointSizeType_128 = 0b100,
	USBEndpointSizeType_256 = 0b101,
	USBEndpointSizeType_512 = 0b110,
};

enum USBEndpointTransferType // See: Source(2) @ Table(9-13) @ Page(270) & Source(1) @ Section(22.18.2) @ Page(286) & Source(2) @ Section(4.7) @ Page(20-21).
{
	USBEndpointTransferType_control     = 0b00, // Guaranteed data delivery. Used by the USB standard to configure devices, but is open to other implementors.
	USBEndpointTransferType_isochronous = 0b01, // Bounded latency, one-way, guaranteed data bandwidth (but no guarantee of delivery). Ex: audio/video.
	USBEndpointTransferType_bulk        = 0b10, // No latency guarantees, one-way, large amount of data. Ex: file upload.
	USBEndpointTransferType_interrupt   = 0b11, // Low-latency, one-way, small amount of data. Ex: keyboard.
};

enum USBSetupRequest // See: Source(2) @ Table(9-3) @ Page(251).
{
	USBSetupRequest_get_status        = 0,
	USBSetupRequest_clear_feature     = 1,
	USBSetupRequest_set_feature       = 3, // Note: Skipped 2!
	USBSetupRequest_set_address       = 5, // Note: Skipped 4!
	USBSetupRequest_get_descriptor    = 6,
	USBSetupRequest_set_descriptor    = 7,
	USBSetupRequest_get_configuration = 8,
	USBSetupRequest_set_configuration = 9,
	USBSetupRequest_get_interface     = 10,
	USBSetupRequest_set_interface     = 11,
	USBSetupRequest_synch_frame       = 12,

	// This is non-exhaustive. See: CDC-Specific Request Codes @ Source(6) @ Table(46) @ Page(64-65).
	USBSetupRequest_communication_set_line_coding        = 0x20,
	USBSetupRequest_communication_get_line_coding        = 0x21,
	USBSetupRequest_communication_set_control_line_state = 0x22,
};

enum USBCommunicationControlSignalFlag
{
	USBCommunicationControlSignalFlag_
};

enum USBEndpointAddressFlag // See: "bEndpointAddress" @ Source(2) @ Table(9-13) @ Page(269).
{
	USBEndpointAddressFlag_in = 1 << 7
};

enum USBConfigurationAttributeFlag // See: Source(2) @ Table(9-10) @ Page(266).
{
	USBConfigurationAttributeFlag_remote_wakeup = 1 << 5,
	USBConfigurationAttributeFlag_self_powered  = 1 << 6,
	USBConfigurationAttributeFlag_reserved_one  = 1 << 7, // Must always be set.
};

enum USBClass // ID that describes the generic function of the device or interface. This enum is non-exhuastive. See: Source(5).
{
	USBClass_null               = 0x00, // When used in device descriptors, the class is determined by the interface descriptors. Cannot otherwise be used in interface descriptors as it is reserved for future use.
	USBClass_communication      = 0x02,
	USBClass_human              = 0x03,
	USBClass_mass_storage       = 0x08,
	USBClass_communication_data = 0x0A,
};

enum USBDescriptorCommunicationSubtype // This enum is non-exhuastive. See: Source(6) @ Table(25) @ AbsPage(44-45).
{
	USBDescriptorCommunicationSubtype_header                      = 0x00,
	USBDescriptorCommunicationSubtype_call_management             = 0x01,
	USBDescriptorCommunicationSubtype_abstract_control_management = 0x02,
	USBDescriptorCommunicationSubtype_union                       = 0x06,
};

enum USBDescriptorType // See: Source(2) @ Table(9-5) @ Page(251).
{
	USBDescriptorType_device                    = 1,
	USBDescriptorType_configuration             = 2,
	USBDescriptorType_string                    = 3,
	USBDescriptorType_interface                 = 4,
	USBDescriptorType_endpoint                  = 5,
	USBDescriptorType_device_qualifier          = 6,
	USBDescriptorType_other_speed_configuration = 7,
	USBDescriptorType_interface_power           = 8,

	// Class-specific descriptor types for USBClass_communication. See: Source(6) @ Table(24) @ AbsPage(44).
	USBDescriptorType_communication_interface   = 0x24,
	USBDescriptorType_communication_endpoint    = 0x25,
};

enum USBCommunicationCallManagementCapabilitiesFlag // See: "bmCapabilities" @ Source(6) @ Table(27) @ AbsPage(46).
{
	USBCommunicationCallManagementCapabilitiesFlag_self_managed_device     = 1 << 0,
	USBCommunicationCallManagementCapabilitiesFlag_managable_by_data_class = 1 << 1,
};

enum USBCommunicationAbstractControlManagementCapabilitiesFlag
{
	USBCommunicationAbstractControlManagementCapabilitiesFlag_comm_group         = 1 << 0,
	USBCommunicationAbstractControlManagementCapabilitiesFlag_line_group         = 1 << 1,
	USBCommunicationAbstractControlManagementCapabilitiesFlag_send_break         = 1 << 2,
	USBCommunicationAbstractControlManagementCapabilitiesFlag_network_connection = 1 << 3,
};

struct USBSetupPacket
{
	union
	{
		struct USBSetupPacketUnknown // See: Source(2) @ Table(9-2) @ Page(248).
		{
			u8  bmRequestType; // TODO Have flags for this?
			u8  bRequest;      // Aliasing(enum USBSetupRequest).
			u16 wValue;
			u16 wIndex;
			u16 wLength;
		} unknown;

		struct USBSetupPacketGetDescriptor // See: Source(2) @ Section(9.4.3) @ Page(253).
		{
			u8  bmRequestType;    // Must be 0b1000'0000.
			u8  bRequest;         // Must be USBSetupRequest_get_descriptor.
			u8  descriptor_index;
			u8  descriptor_type;  // Aliasing(enum USBDescriptorType).
			u16 language_id;
			u16 wLength;
		} get_descriptor;

		struct USBSetupPacketSetAddress // See: Source(2) @ Section(9.4.6) @ Page(256).
		{
			u8  bmRequestType; // Must be 0b0000'0000.
			u8  bRequest;      // Must be USBSetupRequest_set_address.
			u16 address;
			u32 zero;          // Expect to be zero.
		} set_address;


		struct USBSetupPacketSetConfiguration // See: Source(2) @ Section(9.4.7) @ Page(257).
		{
			u8  bmRequestType;       // Must be 0b0000'0000.
			u8  bRequest;            // Must be USBSetupRequest_set_configuration.
			u16 configuration_value; // Configuration that the host wants to set the device to. Zero is reserved for making the device be in "Address State", so 1 must be used.
			u32 zero;                // Expect to be zero.
		} set_configuration;

		struct USBSetupPacketCommunicationGetLineCoding // See: Source(6) @ Section(6.2.13) @ AbsPage(69).
		{
			u8  bmRequestType;   // Must be 0b1010'0001.
			u8  bRequest;        // Must be USBSetupRequest_communication_get_line_coding.
			u16 zero;            // Expect zero.
			u16 interface_index; // Index of the communication interface to get the line-coding from.
			u16 structure_size;  // Must be size of struct USBCommunicationLineCoding.
		} communication_get_line_coding;

		struct USBSetupPacketCommunicationSetLineCoding // See: Source(6) @ Section(6.2.12) @ AbsPage(68-69).
		{
			u8  bmRequestType;   // Must be 0b0010'0001.
			u8  bRequest;        // Must be USBSetupRequest_communication_set_line_coding.
			u16 zero;            // Expect zero.
			u16 interface_index; // Index of the communication interface to set the line-coding from.
			u16 structure_size;  // Must be size of struct USBCommunicationLineCoding.
		} communication_set_line_coding;
	};
};

enum USBCommunicationLineCodingStopBit // See: "Stop Bits" @ Source(6) @ Table(50) @ AbsPage(69).
{
	USBCommunicationLineCodingStopBit_1   = 0,
	USBCommunicationLineCodingStopBit_1_5 = 1,
	USBCommunicationLineCodingStopBit_2   = 2,
};

enum USBCommunicationLineCodingParity // See: "Parity Type" @ Source(6) @ Table(50) @ AbsPage(69).
{
	USBCommunicationLineCodingParity_none  = 0,
	USBCommunicationLineCodingParity_odd   = 1,
	USBCommunicationLineCodingParity_even  = 2,
	USBCommunicationLineCodingParity_mark  = 3,
	USBCommunicationLineCodingParity_space = 4,
};

struct USBCommunicationLineCoding // See: Source(6) @ Table(50) @ Page(69).
{
	u32 dwDTERate;   // Baud rate.
	u8  bCharFormat; // Aliasing(enum USBCommunicationLineCodingStopBit).
	u8  bParityType; // Aliasing(enum USBCommunicationLineCodingParity).
	u8  bDataBits;   // Must be 5, 6, 7, 8, or 16.
};

struct USBDescriptorDevice // [USB Device Descriptor].
{
	u8  bLength;            // Must be the size of struct USBDescriptorDevice.
	u8  bDescriptorType;    // Aliasing(enum USBDescriptorType). Must be USBDescriptorType_device.
	u16 bcdUSB;             // USB specification version that's being complied with.
	u8  bDeviceClass;       // Aliasing(enum USBClass).
	u8  bDeviceSubClass;    // Might be used to further subdivide the device class. See: Source(2) @ Section(9.2.3) @ Page(245).
	u8  bDeviceProtocol;    // Might be used to indicate to the host on how to communicate with the device. See: Source(2) @ Section(9.2.3) @ Page(245).
	u8  bMaxPacketSize0;    // Max packet size of endpoint 0.
	u16 idVendor;           // UID assigned by the USB-IF organization for a company. This with idProduct apparently helps the host find the most appropriate driver for the device (see: Source(4) @ Chapter(5)).
	u16 idProduct;          // UID arbitrated by the vendor for a specific device made by them.
	u16 bcdDevice;          // Version number of the device.
	u8  iManufacturer;      // Index of string descriptor for the manufacturer description.
	u8  iProduct;           // Index of string descriptor for the product description.
	u8  iSerialNumber;      // Index of string descriptor for the device's serial number.
	u8  bNumConfigurations; // Count of configurations the device can be set to. See: Source(4) @ Chapter(5).
};

struct USBDescriptorConfiguration // [USB Configuration Descriptor].
{
	u8  bLength;             // Must be the size of struct USBDescriptorConfiguration.
	u8  bDescriptorType;     // Aliasing(enum USBDescriptorType). Must be USBDescriptorType_configuration.
	u16 wTotalLength;        // Amount of bytes describing the entire configuration including the configuration descriptor, endpoints, etc. See: Configuration Hierarchy Diagram @ Source(4) @ Chapter(5).
	u8  bNumInterfaces;      // Amount of interfaces that this configuration has.
	u8  bConfigurationValue; // "Value to use as an argument to the SetConfiguration() request to select this configuration". See: Source(2) @ Table(9-10) @ Page(265).
	u8  iConfiguration;      // Index of string descriptor describing this configuration for diagnostics.
	u8  bmAttributes;        // Aliasing(enum USBConfigurationAttributeFlag).
	u8  bMaxPower;           // Expressed in units of 2mA (e.g. bMaxPower = 50 -> 100mA usage).
};

struct USBDescriptorInterface // [USB Interface Descriptor].
{
	u8 bLength;            // Must be the size of struct USBDescriptorInterface.
	u8 bDescriptorType;    // Aliasing(enum USBDescriptorType). Must be USBDescriptorType_interface.
	u8 bInterfaceNumber;   // Index of the interface within the configuration.
	u8 bAlternateSetting;  // If 1, then this interface is an alternate for the interface at index bInterfaceNumber.
	u8 bNumEndpoints;      // Number of endpoints (that aren't endpoint 0) used by this interface.
	u8 bInterfaceClass;    // Aliasing(enum USBClass). Value of USBClass_null is reserved.
	u8 bInterfaceSubClass; // Might be used to further subdivide the interface class.
	u8 bInterfaceProtocol; // Might be used to indicate to the host on how to communicate with the device.
	u8 iInterface;         // Index of string descriptor that describes this interface.
};

struct USBDescriptorEndpoint // [USB Endpoint Descriptor].
{
	u8  bLength;          // Must be the size of struct USBDescriptorEndpoint.
	u8  bDescriptorType;  // Aliasing(enum USBDescriptorType). Must be USBDescriptorType_endpoint.
	u8  bEndpointAddress; // Addresses must be specified in low-nibble. Can be applied with enum USBEndpointAddressFlag.
	u8  bmAttributes;     // Aliasing(enum USBEndpointTransferType). Other bits have meanings too, but only for isochronous endpoints, which we don't use.
	u16 wMaxPacketSize;   // Bits 10-0 specifies the maximum data-packet size that can be sent/received. Bits 12-11 are reserved for high-speed, which ATmega32U4 isn't capable of.
	u8  bInterval;        // Amount of frames (~1ms in full-speed USB) that the endpoint will be polled for data. Isochronous must be within [0, 16] and interrupts within [1, 255].
};

struct USBDescriptorCommunicationHeader // [USB Communication Header Functional Descriptor].
{
	u8  bLength;            // Must be the size of struct USBDescriptorCommunicationHeader.
	u8  bDescriptorType;    // Aliasing(enum USBDescriptorType). Must be USBDescriptorType_communication_interface.
	u8  bDescriptorSubtype; // Aliasing(enum USBDescriptorCommunicationSubtype). Must be USBDescriptorCommunicationSubtype_header.
	u16 bcdCDC;             // CDC specification version that's being complied with.
};

struct USBDescriptorCommunicationCallManagement // [USB Communication Call Management Functional Descriptor].
{
	u8 bLength;            // Must be the size of struct USBDescriptorCommunicationCallManagement.
	u8 bDescriptorType;    // Aliasing(enum USBDescriptorType). Must be USBDescriptorType_communication_interface.
	u8 bDescriptorSubtype; // Aliasing(enum USBDescriptorCommunicationSubtype). Must be USBDescriptorCommunicationSubtype_call_management.
	u8 bmCapabilities;     // Aliasing(enum USBCommunicationCallManagementCapabilitiesFlag).
	u8 bDataInterface;     // Index of the CDC-data interface that is used to transmit/receive call management (if needed).
};

struct USBDescriptorCommunicationAbstractControlManagement // [USB Communication Abstract Control Management Functional Descriptor].
{
	u8 bLength;            // Must be the size of struct USBDescriptorCommunicationAbstractControlManagement.
	u8 bDescriptorType;    // Aliasing(enum USBDescriptorType). Must be USBDescriptorType_communication_interface.
	u8 bDescriptorSubtype; // Aliasing(enum USBDescriptorCommunicationSubtype). Must be USBDescriptorCommunicationSubtype_abstract_control_management.
	u8 bmCapabilities;     // Aliasing(enum USBCommunicationAbstractControlManagementCapabilitiesFlag).
};

struct USBDescriptorCommunicationUnion // [USB Communication Union Functional Descriptor].
{
	u8 bLength;            // Must be the size of struct USBDescriptorCommunicationUnion.
	u8 bDescriptorType;    // Aliasing(enum USBDescriptorType). Must be USBDescriptorType_communication_interface.
	u8 bDescriptorSubtype; // Aliasing(enum USBDescriptorCommunicationSubtype). Must be USBDescriptorCommunicationSubtype_union.
	u8 bMasterInterface;   // Index to the master interface.
	u8 bSlaveInterface[1]; // Index to the slave interface that is controlled by bMasterInterface. A varying amount is allowed here, but an array of 1 is all we'll need.
};

struct USBConfigurationHierarchy // This layout is just for our device application. See: Source(4) @ Chapter(5).
{
	struct USBDescriptorConfiguration configuration;

	struct
	{
		struct USBDescriptorInterface                              descriptor;
		struct USBDescriptorCommunicationHeader                    communication_header;
		struct USBDescriptorCommunicationCallManagement            communication_call_management;
		struct USBDescriptorCommunicationAbstractControlManagement communication_abstract_control_management;
		struct USBDescriptorCommunicationUnion                     communication_union;
		struct USBDescriptorEndpoint                               endpoints[1];
	} interface_0;

	struct
	{
		struct USBDescriptorInterface descriptor;
		struct USBDescriptorEndpoint  endpoints[2];
	} interface_1;
};

#define USB_ENDPOINT_0_SIZE_TYPE USBEndpointSizeType_8  // Also called "default endpoint". This is where control commands are sent.
#define USB_ENDPOINT_2_SIZE_TYPE USBEndpointSizeType_8  // For CDC's "notification element". Lets the host know of any changes in the communication.
#define USB_ENDPOINT_3_SIZE_TYPE USBEndpointSizeType_64 // For CDC-Data class to transmit communication data to host.
#define USB_ENDPOINT_4_SIZE_TYPE USBEndpointSizeType_64 // For CDC-Data class to receive communication data from host.

static const struct USBDescriptorDevice USB_DEVICE_DESCRIPTOR = // TODO PROGMEMify.
	{
		.bLength            = sizeof(struct USBDescriptorDevice),
		.bDescriptorType    = USBDescriptorType_device,
		.bcdUSB             = 0x0200,
		.bDeviceClass       = USBClass_communication, // TODO I think this makes the whole device a CDC. Can we take this out?
		.bDeviceSubClass    = 0, // Irrelevant for USBClass_communication. See Source(5) & Source(6) @ Table(20) @ AbsPage(42).
		.bDeviceProtocol    = 0, // Irrelevant for USBClass_communication. See Source(5) & Source(6) @ Table(20) @ AbsPage(42).
		.bMaxPacketSize0    = SIZEOF_ENDPOINT_SIZE_TYPE(USB_ENDPOINT_0_SIZE_TYPE),
		.idVendor           = 0, // Setting to zero doesn't seem to prevent functionality.
		.idProduct          = 0, // Setting to zero doesn't seem to prevent functionality.
		.bcdDevice          = 0, // Setting to zero doesn't seem to prevent functionality.
		.iManufacturer      = 0, // Not important; point to empty string.
		.iProduct           = 0, // Not important; point to empty string.
		.iSerialNumber      = 0, // Not important; point to empty string.
		.bNumConfigurations = 1
	};

static const struct USBConfigurationHierarchy USB_CONFIGURATION_HIERARCHY = // See: Source(4) @ Chapter(5).
	{
		.configuration =
			{
				.bLength             = sizeof(struct USBDescriptorConfiguration),
				.bDescriptorType     = USBDescriptorType_configuration,
				.wTotalLength        = sizeof(struct USBConfigurationHierarchy),
				.bNumInterfaces      = 2,
				.bConfigurationValue = 1, // Argument used for SetConfiguration to select this configuration. Can't be zero, so 1 is used. See: Soure(2) @ Figure(11-10) @ Page(310).
				.iConfiguration      = 0, // Description of this configuration is not important.
				.bmAttributes        = USBConfigurationAttributeFlag_reserved_one | USBConfigurationAttributeFlag_self_powered, // TODO We should calculate our power consumption!
				.bMaxPower           = 50, // TODO We should calculate our power consumption!
			},
		.interface_0 =
			{
				.descriptor = // [USB Communication Interface]. TODO How do we know that the CDC class needs these specific functional headers?
					{
						.bLength            = sizeof(struct USBDescriptorInterface),
						.bDescriptorType    = USBDescriptorType_interface,
						.bInterfaceNumber   = 0,
						.bAlternateSetting  = 0,
						.bNumEndpoints      = countof(USB_CONFIGURATION_HIERARCHY.interface_0.endpoints),
						.bInterfaceClass    = USBClass_communication, // See: Source(6) @ Section(4.2) @ AbsPage(39).
						.bInterfaceSubClass = 0x2, // See: "Abstract Control Model" @ Source(6) @ Section(3.6.2) @ AbsPage(26).
						.bInterfaceProtocol = 0x1, // See: "V.25ter" @ Source(6) @ Table(17) @ AbsPage(39). TODO Is this protocol needed?
						.iInterface         = 0,   // Not important; point to empty string.
					},
				.communication_header = // [USB Communication Header Functional Descriptor].
					{
						.bLength            = sizeof(struct USBDescriptorCommunicationHeader),
						.bDescriptorType    = USBDescriptorType_communication_interface,
						.bDescriptorSubtype = USBDescriptorCommunicationSubtype_header,
						.bcdCDC             = 0x0110,
					},
				.communication_call_management = // [USB Communication Call Management Functional Descriptor].
					{
						.bLength            = sizeof(struct USBDescriptorCommunicationCallManagement),
						.bDescriptorType    = USBDescriptorType_communication_interface,
						.bDescriptorSubtype = USBDescriptorCommunicationSubtype_call_management,
						.bmCapabilities     = USBCommunicationCallManagementCapabilitiesFlag_self_managed_device,
						.bDataInterface     = 1,
					},
				.communication_abstract_control_management = // [USB Communication Abstract Control Management Functional Descriptor].
					{
						.bLength            = sizeof(struct USBDescriptorCommunicationAbstractControlManagement),
						.bDescriptorType    = USBDescriptorType_communication_interface,
						.bDescriptorSubtype = USBDescriptorCommunicationSubtype_abstract_control_management,
						.bmCapabilities     = USBCommunicationAbstractControlManagementCapabilitiesFlag_line_group | USBCommunicationAbstractControlManagementCapabilitiesFlag_send_break,
					},
				.communication_union = // [USB Communication Union Functional Descriptor].
					{
						.bLength            = sizeof(struct USBDescriptorCommunicationUnion),
						.bDescriptorType    = USBDescriptorType_communication_interface,
						.bDescriptorSubtype = USBDescriptorCommunicationSubtype_union,
						.bMasterInterface   = 0,
						.bSlaveInterface    = { 1 }
					},
				.endpoints = // [USB Communication Endpoints].
					{
						{
							.bLength          = sizeof(struct USBDescriptorEndpoint),
							.bDescriptorType  = USBDescriptorType_endpoint,
							.bEndpointAddress = 2 | USBEndpointAddressFlag_in,
							.bmAttributes     = USBEndpointTransferType_interrupt,
							.wMaxPacketSize   = SIZEOF_ENDPOINT_SIZE_TYPE(USB_ENDPOINT_2_SIZE_TYPE),
							.bInterval        = 64, // TODO We don't care about notifications, so we should max this out.
						},
					},
			},
		.interface_1 =
			{
				.descriptor = // [USB Communication-Data Interface].
					{
						.bLength            = sizeof(struct USBDescriptorInterface),
						.bDescriptorType    = USBDescriptorType_interface,
						.bInterfaceNumber   = 1,
						.bAlternateSetting  = 0,
						.bNumEndpoints      = countof(USB_CONFIGURATION_HIERARCHY.interface_1.endpoints),
						.bInterfaceClass    = USBClass_communication_data, // See: Source(6) @ Section(4.5) @ AbsPage(40).
						.bInterfaceSubClass = 0, // Should be left alone. See: Source(6) @ Section(4.6) @ AbsPage(40).
						.bInterfaceProtocol = 0, // Not using any specific communication protocol. See: Source(6) @ Table(19) @ AbsPage(40-41).
						.iInterface         = 0, // Not important; point to empty string.
					},
				.endpoints = // [USB Communiction-Data Endpoints].
					{
						{
							.bLength          = sizeof(struct USBDescriptorEndpoint),
							.bDescriptorType  = USBDescriptorType_endpoint,
							.bEndpointAddress = 3 | USBEndpointAddressFlag_in,
							.bmAttributes     = USBEndpointTransferType_bulk,
							.wMaxPacketSize   = SIZEOF_ENDPOINT_SIZE_TYPE(USB_ENDPOINT_3_SIZE_TYPE),
							.bInterval        = 0,
						},
						{
							.bLength          = sizeof(struct USBDescriptorEndpoint),
							.bDescriptorType  = USBDescriptorType_endpoint,
							.bEndpointAddress = 4,
							.bmAttributes     = USBEndpointTransferType_bulk,
							.wMaxPacketSize   = SIZEOF_ENDPOINT_SIZE_TYPE(USB_ENDPOINT_4_SIZE_TYPE),
							.bInterval        = 0,
						},
					}
			},
	};

static const struct USBCommunicationLineCoding USB_COMMUNICATION_LINE_CODING =
	{
		.dwDTERate   = 9600,
		.bCharFormat = USBCommunicationLineCodingStopBit_1,
		.bParityType = USBCommunicationLineCodingParity_none,
		.bDataBits   = 8,
	};

//
// Internal Documentation.
//

/*
	Source(1) := ATmega32U4 Datasheet ("Atmel-7766J-USB-ATmega16U4/32U4-Datasheet_04/2016").
	Source(2) := USB 2.0 Specification ("Universal Serial Bus Specification Revision 2.0") (Dated: April 27, 2000).
	Source(3) := Arduino Leonardo Pinout Diagram (STORE.ARDUINO.CC/LEONARDO) (Dated: 17/06/2020).
	Source(4) := USB in a NutShell by BeyondLogic (Accessed: September 19, 2023).
	Source(5) := USB-IF Defined Class Codes (usb.org/defined-class-codes) (Accessed: September 19, 2023).
	*Source(6) := USB CDC Specification v1.1 (Dated: January 19, 1999).
	Source(7) := USB PSTN Specification v1.2 (Dated: February 9, 2007).

	We are working within the environment of the ATmega32U4 microcontroller,
	which is an 8-bit CPU. This consequently means that there are no padding bytes to
	worry about within structs, and that the CPU doesn't exactly have the concept of
	"endianess", since all words are single bytes, so it's really up to the compiler
	on how it will layout memory and perform arithmetic calculations on operands greater
	than a byte. That said, it seems like the MCU's opcodes are 16-bits and are stored
	in little-endian format, so AVR-GCC then also use the same convention for everything
	else. In short: no padding bytes exist and we are in little-endian.

	The USB protocol is pretty intense. Hopefully this codebase is well documented
	enough that it is easy to follow along to understand why things are being done
	the way they are. If they aren't, then many citations and sources are linked to
	sites, datasheets, specifications, etc. that describes a particular part in more
	detail. "NEFASTOR ONLINE" (nefastor.com/micrcontrollers/stm32/usb/) contains some
	articles explaining parts of the USB protocol, and Ben Eater on YouTube goes pretty
	in-depth on the USB tranmission sides of things.

	* I seem to not be able to find a comprehensive document of v1.2. USB.org has a zip
	file containing an errata version of v1.2., but even that seem to be missing quite
	a lot of information compared to v1.1. Perhaps we're supposed to use the errata for
	the more up-to-date details, but fallback to v1.1 when needed. Regardless, USB
	should be quite backwards-compatiable, so we should be fine with v1.1.
*/

/* [Pin Mapping].
	Maps simple pin numberings to corresponding data-direction ports and bit-index.
	This can be found in pinout diagrams such as (1).

	(1) Arduino Leonardo Pinout Diagram @ Source(3).
*/

/* [USB Device Descriptor]
	The host wants to learn what the device actually is and how to further work with it.

	(1) Device Descriptor Layout @ Source(2) @ Table(9-8) @ Page(262-263).
*/

/* [USB Configuration Descriptor].
	A USB device configuration describes the high-level settings of the entire device
	(e.g. whether or not the device is self-powered or host-powered (1)),
	and there can be multiple configurations on a single device, to which the host will
	pick the most appropriate one.

	A configuration primarily describes the set of interfaces, which can be thought of
	as the set of "features" that this device in this specific configuration has.

	(1) Configurations @ Source(4) @ Chapter(5).
	(2) Configuration Descriptor Layout @ Source(2) @ Table(9-10) @ Page(265).
*/

/* [USB Interface Descriptor].
	The following paragraph from (1) describes the idea behind interfaces in a
	configuration pretty well:
		> The interface descriptor could be seen as a header or grouping of the
		> endpoints into a functional group performing a single feature of the device.
		> For example you could have a multi-function fax/scanner/printer device.
		> Interface descriptor one could describe the endpoints of the fax function,
		> Interface descriptor two the scanner function and Interface descriptor three
		> the printer function. Unlike the configuration descriptor, there is no
		> limitation as to having only one interface enabled at a time. A device could
		> have 1 or many interface descriptors enabled at once.

	(1) Interface Synopsis @ Source(4) @ Chapter(5).
	(2) Interface Descriptor Layout @ Source(2) @ Table(9-12) @ Page(268-269).
*/

/* [USB Endpoint Descriptor].
	An endpoint descriptor describe the properties of the endpoint for a specific
	interface of a specific configuration of the device, as visually seen in (1).

	(1) Configuration Hierarchy Diagram @ Source(4) @ Chapter(5).
	(2) Endpoint Descriptor Layout @ Source(2) @ Table(9-13) @ Page(269-271).
*/

/* [USB Communication Interface].
	This interface sets up the first half of the CDC (Communications Device Class)
	interface so we can send diagnostic information to the host. I will admit that
	I am quite too young to even begin to understand what modems are or the old ways of
	technology before the internet became hip and cool. Nonetheless, I will document
	my understanding of what has to be done in order to set the communication interface
	up.

	We set bInterfaceClass to USBClass_communication to indicate to the host that this
	interface is for the communication class, and as a result will contain specific
	information that is not specified at all within the USB specification.

	bInterfaceSubClass then further narrows the interface's class down to
	"Abstract Control Model" (1), which from what I understand, makes the USB device
	emulate a COM port that could then receive and transmit data serially.

	TODO
		For bInterfaceProtocol, I assume we should be using the protocol that interprets
		AT/V250/V.25ter/Hayes commands, since (2) says that the abstract control model
		understands these. But I don't think we use this obscure system anymore...
		So can we remove it? (3) says that if the control model doesn't require it,
		which PSTN that defines ACM doesn't seem to enforce, then we should set it to 0.

	(1) "Abstract Control Model" @ Source(6) @ Section(3.6.2) @ AbsPage(26).
	(2) ACM with AT Commands @ Source(7) @ Section(3.2.2) @ AbsPage(15).
	(3) Control Model on Protocols @ Source(6) @ Section(4.4) @ AbsPage(40).
*/

/* [USB Communication Header Functional Descriptor].
	This is a class-specific descriptor, in the sense that the USB 2.0 specification
	doesn't mention this structure at all. This descriptor is just for CDC for which
	its specification does actually state the layout (1).

	This descriptor is used first before the other class-specific descriptors in CDC,
	all to just simply announce the specification version we're complying with.

	(1) "Functional Descriptors" @ Source(6) @ Section(5.2.3) @ AbsPage(43).
	(2) Header Descriptor Layout @ Source(6) @ Section(5.2.3.1) @ AbsPage(45).
*/

/* [USB Communication Call Management Functional Descriptor].
	This CDC-specific descriptor informs the host about the "call management" of the
	communication. According to (1), this is what it means:
		> Refers to a process that is responsible for the setting up and tearing down of
		> calls. This same process also controls the operational parameters of the call.
		> The term "call," and therefore "call management," describes processes which
		> refer to a higher level of call control, rather than those processes
		> responsible for the physical connection.

	To what extend that this actually matters or contributes to opening a virtual COM
	port to send diagnostics to, I'm not entirely too sure. The particular settings
	were chosen simply based off of m_usb.c by J. Fiene & J. Romano.

	(1) "Call Management" @ Source(6) @ Section(1.4) @ AbsPage(15).
	(2) Call Management Descriptor Layout @ Source(6) @ Table(27) @ AbsPage(45-46).
*/

/* [USB Communication Abstract Control Management Functional Descriptor].
	This descriptor seems to simply show what things the host can request from the
	abstract control model, such as requesting or setting baud rates.

	TODO Seems like m_usb handles SET_LINE_CODING and the variants just to ignore
	it really. So maybe we can remove this capability completely? Or perhaps this
	will be important in being able to do a baud-touch reset.

	(2) Abstract Control Management Descriptor Layout @ Source(6) @ Table(28) @ AbsPage(46-47).
*/

/* [USB Communication Union Functional Descriptor].
	This descriptor groups interfaces together with one of them being the "master" of
	the rest. Messages sent to the master can in result "act upon the group as a whole",
	and "notifications for the entire group can be sent from this interface but apply
	to the entire group...".

	TODO To what extend is this important, I'm not entirely too sure. What does the host
	do with this information? Can this just be removed?

	(1) Union Descriptor @ Source(6) @ Table(33) @ AbsPage(51).
*/

/* [USB Communication Endpoints].
	It is stated in (1) that every communication class interface must have a
	"management element", which is just an endpoint that transfer commands to
	manages the communication between host and device as defined in (2). This
	management element also happens to be always endpoint 0 for all communication
	devices.

	As declared in (3), abstract control models need to also have the
	"notification element". Like with management element, it is just another
	endpoint, but this time it is used to notify the host of any events,
	and is often (but not necessarily required) an interrupt transfer typed endpoint
	(so it can't be endpoint 0).

	TODO probably better in usb.c
	Every once in a while, the host would send a request defined in (5) to the
	notification element, and if the device ACKs, TODO what exactly happens???

	(1) Communication Endpoints @ Source(6) @ Section(3.4.1) @ AbsPage(23).
	(2) "Management Element" @ Source(6) @ Section(1.4) @ AbsPage(16).
	(3) Endpoints on Abstract Control Models @ Source(6) @ Section(3.6.2) @ AbsPage(26).
	(4) "Notification Element" @ Source(6) @ Section(1.4) @ AbsPage(16).
	(5) "Notification Element Notifications" @ Source(6) @ Section(6.3) @ AbsPage(84).
*/

/* [USB Communication-Data Interface].
	I'm not entirely too sure what a "data class" is exactly used for here, but based
	on loose descriptions of its usage, it's to respresent the part of the communication
	where there might be compression, error correction, modulation, etc. as seen in (1).

	Regardless, this interface is where the I/O streams of the communication between
	host and device is held.

	(1) Abstract Control Model Diagram @ Source(6) @ Figure(3) @ AbsPage(15).
*/

/* [USB Communiction-Data Endpoints].
	Stated by (1), the endpoints that will be carrying data between the host and device
	must be either both isochronous or both bulk transfer types.

	(1) "Data Class Endpoint Requirements" @ Source(6) @ Section(3.4.2) @ AbsPage(23).
*/
