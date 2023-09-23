// TODO Interrupt behavior on USB disconnection?
// TODO is there a global jump that could be done for error purposes?
#define false            0
#define true             1
#define stringify_(X)    #X
#define stringify(X)     stringify_(X)
#define concat_(X, Y)    X##Y
#define concat(X, Y)     concat_(X, Y)
#define countof(...)     (sizeof(__VA_ARGS__)/sizeof((__VA_ARGS__)[0]))
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
	PinState_floating = 0b10, // Unconnected; reading from this pin is unreliable.
	PinState_input    = 0b11, // Pull-up resistor enabled for reliable reads. A truthy value will be read unless it is tied to GND strongly enough.
};

// Maps simple pin numberings to corresponding data-direction ports and bit-index. See: Arduino Leonardo Pinout Diagram @ Source(3).
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

enum USBEndpointSizeCode // See: Source(1) @ Section(22.18.2) @ Page(287).
{
	USBEndpointSizeCode_8   = 0b000,
	USBEndpointSizeCode_16  = 0b001,
	USBEndpointSizeCode_32  = 0b010,
	USBEndpointSizeCode_64  = 0b011,
	USBEndpointSizeCode_128 = 0b100,
	USBEndpointSizeCode_256 = 0b101,
	USBEndpointSizeCode_512 = 0b110,
};

enum USBEndpointTransferType // See: Source(2) @ Table(9-13) @ Page(270) & Source(1) @ Section(22.18.2) @ Page(286) & Source(2) @ Section(4.7) @ Page(20-21).
{
	USBEndpointTransferType_control     = 0b00, // Guaranteed data delivery. Used by the USB standard to configure devices, but is open to other implementors.
	USBEndpointTransferType_isochronous = 0b01, // Bounded latency, one-way, guaranteed data bandwidth (but no guarantee of delivery). Ex: audio/video.
	USBEndpointTransferType_bulk        = 0b10, // No latency guarantees, one-way, large amount of data. Ex: file upload.
	USBEndpointTransferType_interrupt   = 0b11, // Low-latency, one-way, small amount of data. Ex: keyboard.
};

// This enum is non-exhaustive; that is, not all possible SETUP-transactions are described here.
// ATmega32U4 is being compiled as little-endian, so the LSB of this enum value
// will correspond to "bmRequestType" and the MSB is "bRequest". See: Source(2) @ Table(9-2) @ Page(248).
enum USBSetupType
{
	// See: Source(2) @ Table(9-3) @ Page(250) & Source(2) @ Table(9-4) @ Page(251).
	USBSetupType_get_descriptor = 0b10000000 | (((u16) 6) << 8),
	USBSetupType_set_address    = 0b00000000 | (((u16) 5) << 8),
	USBSetupType_set_config     = 0b00000000 | (((u16) 9) << 8),

	// See: Source(6) @ Table(44) @ AbsPage(62-63) & Source(6) @ Table(46) @ AbsPage(64-65).
	USBSetupType_cdc_get_line_coding        = 0b10100001 | (((u16) 0x21) << 8),
	USBSetupType_cdc_set_line_coding        = 0b00100001 | (((u16) 0x20) << 8),
	USBSetupType_cdc_set_control_line_state = 0b00100001 | (((u16) 0x22) << 8),
};

enum USBClass // Non-exhuastive. See: Source(5).
{
	USBClass_null         = 0x00, // When used in device descriptors, the class is determined by the interface descriptors. Cannot be used in interface descriptors as it is "reserved for future use".
	USBClass_cdc          = 0x02, // See: "Communication Class Device" @ Source(6) @ Section(1) @ AbsPage(11).
	USBClass_hid          = 0x03, // TODO
	USBClass_mass_storage = 0x08, // TODO
	USBClass_cdc_data     = 0x0A, // See: "Data Interface Class" @ Source(6) @ Section(1) @ AbsPage(11).
};

enum USBDescType // See: Source(2) @ Table(9-5) @ Page(251).
{
	USBDescType_device             = 1,
	USBDescType_config             = 2,
	USBDescType_string             = 3,
	USBDescType_interface          = 4,
	USBDescType_endpoint           = 5,
	USBDescType_device_qualifier   = 6,
	USBDescType_other_speed_config = 7,
	USBDescType_interface_power    = 8,

	// Non-exhaustive. See: Source(6) @ Table(24) @ AbsPage(44).
	USBDescType_cdc_interface = 0x24,
	USBDescType_cdc_endpoint  = 0x25,
};

enum USBDescCDCSubtype // Non-exhuastive. See: Source(6) @ Table(25) @ AbsPage(44-45).
{
	USBDescCDCSubtype_header          = 0x00,
	USBDescCDCSubtype_call_management = 0x01,
	USBDescCDCSubtype_acm_management  = 0x02,
	USBDescCDCSubtype_union           = 0x06,
};

enum USBMiscEnum
{
	// See: Line Coding's Stop Bits @ Source(6) @ Table(50) @ AbsPage(69).
	USBMiscEnum_cdc_lc_stop_bit_1   = 0,
	USBMiscEnum_cdc_lc_stop_bit_1_5 = 1,
	USBMiscEnum_cdc_lc_stop_bit_2   = 2,

	// See: Line Coding's Parity Type @ Source(6) @ Table(50) @ AbsPage(69).
	USBMiscEnum_cdc_lc_parity_none  = 0,
	USBMiscEnum_cdc_lc_parity_odd   = 1,
	USBMiscEnum_cdc_lc_parity_even  = 2,
	USBMiscEnum_cdc_lc_parity_mark  = 3,
	USBMiscEnum_cdc_lc_parity_space = 4,
};

enum USBMiscFlag
{
	// See: "bEndpointAddress" @ Source(2) @ Table(9-13) @ Page(269).
	USBMiscFlag_endpoint_address_in = 1 << 7,

	// See: Source(2) @ Table(9-10) @ Page(266).
	USBMiscFlag_config_attr_remote_wakeup = 1 << 5,
	USBMiscFlag_config_attr_self_powered  = 1 << 6,
	USBMiscFlag_config_attr_reserved_one  = 1 << 7, // Must always be set.

	// See: "bmCapabilities" @ Source(6) @ Table(27) @ AbsPage(46).
	USBMiscFlag_call_management_self_managed_device     = 1 << 0,
	USBMiscFlag_call_management_managable_by_data_class = 1 << 1,

	// See: "bmCapabilities" @ Source(6) @ Table(28) @ AbsPage(47).
	USBMiscFlag_acm_management_comm_group         = 1 << 0,
	USBMiscFlag_acm_management_line_group         = 1 << 1,
	USBMiscFlag_acm_management_send_break         = 1 << 2,
	USBMiscFlag_acm_management_network_connection = 1 << 3,
};

struct USBCDCLineCoding // See: Source(6) @ Table(50) @ Page(69).
{
	u32 dwDTERate;   // Baud rate.
	u8  bCharFormat; // Aliasing(enum USBMiscEnum_lc_stop_bit).
	u8  bParityType; // Aliasing(enum USBMiscEnum_lc_parity).
	u8  bDataBits;   // Must be 5, 6, 7, 8, or 16.
};

struct USBSetup // The data-packet that is sent in a SETUP-typed transaction. See: Source(2) @ Table(9-2) @ Page(248).
{
	u16 type; // Aliasing(enum USBSetupType). The "bmRequestType" and "bRequest" bytes are combined into a single word here.
	union
	{
		struct // See: Source(2) @ Section(9.4.3) @ Page(253).
		{
			u8  descriptor_index;
			u8  descriptor_type;   // Aliasing(enum USBDescType).
			u16 language_id;
			u16 requested_amount;
		} get_descriptor;

		struct // See: Source(2) @ Section(9.4.6) @ Page(256).
		{
			u16 address;
		} set_address;

		struct // See: Source(2) @ Section(9.4.7) @ Page(257).
		{
			u16 value; // Configuration that the host wants to set the device to. Zero is reserved for making the device be in "Address State".
		} set_config;

		struct // See: Source(6) @ Section(6.2.13) @ AbsPage(69).
		{
			u16 _zero;
			u16 interface_index; // Index of the CDC interface that this SETUP-transaction is for.
			u16 structure_size;  // Must be sizeof(struct USBCDCLineCoding).
		} cdc_get_line_coding;

		struct // See: Source(6) @ Section(6.2.12) @ AbsPage(68-69).
		{
			u16 _zero;
			u16 interface_index; // Index of the CDC interface that this SETUP-transaction is for.
			u16 structure_size;  // Must be sizeof(struct USBCDCLineCoding).
		} cdc_set_line_coding;
	};
};

struct USBDescDevice // See: Source(2) @ Table(9-8) @ Page(262-263).
{
	u8  bLength;            // Must be sizeof(struct USBDescDevice).
	u8  bDescriptorType;    // Must be USBDescType_device.
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

struct USBDescConfig // See: Source(2) @ Table(9-10) @ Page(265) & About Configurations @ Source(4) @ Chapter(5).
{
	u8  bLength;             // Must be the size of struct USBDescConfig.
	u8  bDescriptorType;     // Must be USBDescType_config.
	u16 wTotalLength;        // Amount of bytes describing the entire configuration including the configuration descriptor, endpoints, etc. See: Configuration Hierarchy Diagram @ Source(4) @ Chapter(5).
	u8  bNumInterfaces;      // Amount of interfaces that this configuration has.
	u8  bConfigurationValue; // "Value to use as an argument to the SetConfiguration() request to select this configuration". See: Source(2) @ Table(9-10) @ Page(265).
	u8  iConfiguration;      // Index of string descriptor describing this configuration for diagnostics.
	u8  bmAttributes;        // Aliasing(enum USBMiscFlag_config_attr).
	u8  bMaxPower;           // Expressed in units of 2mA (e.g. bMaxPower = 50 -> 100mA usage).
};

struct USBDescInterface // See: Source(2) @ Table(9-12) @ Page(268-269) & About Interfaces @ Source(4) @ Chapter(5).
{
	u8 bLength;            // Must be the size of struct USBDescInterface.
	u8 bDescriptorType;    // Must be USBDescType_interface.
	u8 bInterfaceNumber;   // Index of the interface within the configuration.
	u8 bAlternateSetting;  // If 1, then this interface is an alternate for the interface at index bInterfaceNumber.
	u8 bNumEndpoints;      // Number of endpoints (that aren't endpoint 0) used by this interface.
	u8 bInterfaceClass;    // Aliasing(enum USBClass). Value of USBClass_null is reserved.
	u8 bInterfaceSubClass; // Might be used to further subdivide the interface class.
	u8 bInterfaceProtocol; // Might be used to indicate to the host on how to communicate with the device.
	u8 iInterface;         // Index of string descriptor that describes this interface.
};

struct USBDescEndpoint // See: Source(2) @ Table(9-13) @ Page(269-271) & Configuration Hierarchy Diagram @ Source(4) @ Chapter(5).
{
	u8  bLength;          // Must be the size of struct USBDescEndpoint.
	u8  bDescriptorType;  // Must be USBDescType_endpoint.
	u8  bEndpointAddress; // Addresses must be specified in low-nibble. Can be applied with enum USBMiscFlag_endpoint_address.
	u8  bmAttributes;     // Aliasing(enum USBEndpointTransferType). Other bits have meanings too, but only for isochronous endpoints, which we don't use.
	u16 wMaxPacketSize;   // Bits 10-0 specifies the maximum data-packet size that can be sent/received. Bits 12-11 are reserved for high-speed, which ATmega32U4 isn't capable of.
	u8  bInterval;        // Amount of frames (~1ms in full-speed USB) that the endpoint will be polled for data. Isochronous must be within [0, 16] and interrupts within [1, 255].
};

struct USBDescCDCHeader // Source(6) @ Section(5.2.3.1) @ AbsPage(45) & "Functional Descriptors" @ Source(6) @ Section(5.2.3) @ AbsPage(43).
{
	u8  bLength;            // Must be the size of struct USBDescCDCHeader.
	u8  bDescriptorType;    // Must be USBDescType_cdc_interface.
	u8  bDescriptorSubtype; // Must be USBDescCDCSubtype_header.
	u16 bcdCDC;             // CDC specification version that's being complied with.
};

struct USBDescCDCCallManagement // See: Source(6) @ Table(27) @ AbsPage(45-46) & "Call Management" @ Source(6) @ Section(1.4) @ AbsPage(15).
{
	u8 bLength;            // Must be the size of struct USBDescCDCCallManagement.
	u8 bDescriptorType;    // Must be USBDescType_cdc_interface.
	u8 bDescriptorSubtype; // Must be USBDescCDCSubtype_call_management.
	u8 bmCapabilities;     // Aliasing(enum USBMiscFlag_call_management).
	u8 bDataInterface;     // Index of the CDC-Data interface that is used to transmit/receive call management (if needed).
};

// TODO Seems like m_usb handles SET_LINE_CODING and the variants just to ignore it really. So maybe we can remove this capability completely? Or perhaps this will be important in being able to do a baud-touch reset.
struct USBDescCDCACMManagement // See: Source(6) @ Table(28) @ AbsPage(46-47).
{
	u8 bLength;            // Must be the size of struct USBDescCDCAbstractControlManagement.
	u8 bDescriptorType;    // Must be USBDescType_cdc_interface.
	u8 bDescriptorSubtype; // Must be USBDescCDCSubtype_acm.
	u8 bmCapabilities;     // Aliasing(enum USBMiscFlag_acm_management).
};

// TODO To what extend is this important, I'm not entirely too sure. What does the host do with this information? Can this just be removed?
struct USBDescCDCUnion // See: Source(6) @ Table(33) @ AbsPage(51).
{
	u8 bLength;            // Must be the size of struct USBDescCDCUnion.
	u8 bDescriptorType;    // Must be USBDescType_cdc_interface.
	u8 bDescriptorSubtype; // Must be USBDescCDCSubtype_union.
	u8 bMasterInterface;   // Index to the master interface.
	u8 bSlaveInterface[1]; // Index to the slave interface that is controlled by bMasterInterface. A varying amount is allowed here, but an array of 1 is all we'll need.
};

struct USBConfigHierarchy // This layout is uniquely just for our device application. See: Source(4) @ Chapter(5).
{
	struct USBDescConfig config;

	struct
	{
		struct USBDescInterface         descriptor;
		struct USBDescCDCHeader         cdc_header;
		struct USBDescCDCCallManagement cdc_call_management;
		struct USBDescCDCACMManagement  cdc_acm_management;
		struct USBDescCDCUnion          cdc_union;
		struct USBDescEndpoint          endpoints[1];
	} interface_0;

	struct
	{
		struct USBDescInterface descriptor;
		struct USBDescEndpoint  endpoints[2];
	} interface_1;
};

// Endpoint buffer sizes must be one of enum USBEndpointSizeCode.
// The maximum capacity between endpoints also differ. See: Source(1) @ Section(22.1) @ Page(270).
#define USB_ENDPOINT_0_BUFFER_SIZE 8
#define USB_ENDPOINT_2_BUFFER_SIZE 8  // CDC's "notification element".
#define USB_ENDPOINT_3_BUFFER_SIZE 64 // CDC-Data class to transmit communication data to host.
#define USB_ENDPOINT_4_BUFFER_SIZE 64 // CDC-Data class to receive communication data from host.

static const struct USBDescDevice USB_DEVICE_DESCRIPTOR = // TODO PROGMEMify.
	{
		.bLength            = sizeof(struct USBDescDevice),
		.bDescriptorType    = USBDescType_device,
		.bcdUSB             = 0x0200,
		.bDeviceClass       = USBClass_cdc, // TODO I think this makes the whole device a CDC. Can we take this out?
		.bDeviceSubClass    = 0, // Irrelevant for USBClass_cdc. See Source(5) & Source(6) @ Table(20) @ AbsPage(42).
		.bDeviceProtocol    = 0, // Irrelevant for USBClass_cdc. See Source(5) & Source(6) @ Table(20) @ AbsPage(42).
		.bMaxPacketSize0    = USB_ENDPOINT_0_BUFFER_SIZE,
		.idVendor           = 0, // Setting to zero doesn't seem to prevent functionality.
		.idProduct          = 0, // Setting to zero doesn't seem to prevent functionality.
		.bcdDevice          = 0, // Setting to zero doesn't seem to prevent functionality.
		.iManufacturer      = 0, // Not important; point to empty string.
		.iProduct           = 0, // Not important; point to empty string.
		.iSerialNumber      = 0, // Not important; point to empty string.
		.bNumConfigurations = 1
	};

static const struct USBConfigHierarchy USB_CONFIGURATION_HIERARCHY =
	{
		.config =
			{
				.bLength             = sizeof(struct USBDescConfig),
				.bDescriptorType     = USBDescType_config,
				.wTotalLength        = sizeof(struct USBConfigHierarchy),
				.bNumInterfaces      = 2,
				.bConfigurationValue = 1, // Argument used for SetConfiguration command to select this configuration. Zero resets the device's configuration state, so 1 is used instead. See: Soure(2) @ Figure(11-10) @ Page(310).
				.iConfiguration      = 0, // Description of this configuration is not important.
				.bmAttributes        = USBMiscFlag_config_attr_reserved_one | USBMiscFlag_config_attr_self_powered, // TODO We should calculate our power consumption!
				.bMaxPower           = 50, // TODO We should calculate our power consumption!
			},
		.interface_0 =
			{
				.descriptor = // [USB CDC Interface]. TODO How do we know that the CDC class needs these specific functional headers?
					{
						.bLength            = sizeof(struct USBDescInterface),
						.bDescriptorType    = USBDescType_interface,
						.bInterfaceNumber   = 0,
						.bAlternateSetting  = 0,
						.bNumEndpoints      = countof(USB_CONFIGURATION_HIERARCHY.interface_0.endpoints),
						.bInterfaceClass    = USBClass_cdc, // See: Source(6) @ Section(4.2) @ AbsPage(39).
						.bInterfaceSubClass = 0x2, // See: "Abstract Control Model" @ Source(6) @ Section(3.6.2) @ AbsPage(26).
						.bInterfaceProtocol = 0x1, // See: "V.25ter" @ Source(6) @ Table(17) @ AbsPage(39). TODO Is this protocol needed?
						.iInterface         = 0,   // Not important; point to empty string.
					},
				.cdc_header = // [USB CDC Header Functional Descriptor].
					{
						.bLength            = sizeof(struct USBDescCDCHeader),
						.bDescriptorType    = USBDescType_cdc_interface,
						.bDescriptorSubtype = USBDescCDCSubtype_header,
						.bcdCDC             = 0x0110,
					},
				.cdc_call_management = // [USB CDC Call Management Functional Descriptor].
					{
						.bLength            = sizeof(struct USBDescCDCCallManagement),
						.bDescriptorType    = USBDescType_cdc_interface,
						.bDescriptorSubtype = USBDescCDCSubtype_call_management,
						.bmCapabilities     = USBMiscFlag_call_management_self_managed_device,
						.bDataInterface     = 1,
					},
				.cdc_acm_management = // [USB CDC Abstract Control Management Functional Descriptor].
					{
						.bLength            = sizeof(struct USBDescCDCACMManagement),
						.bDescriptorType    = USBDescType_cdc_interface,
						.bDescriptorSubtype = USBDescCDCSubtype_acm_management,
						.bmCapabilities     = USBMiscFlag_acm_management_line_group | USBMiscFlag_acm_management_send_break,
					},
				.cdc_union = // [USB CDC Union Functional Descriptor].
					{
						.bLength            = sizeof(struct USBDescCDCUnion),
						.bDescriptorType    = USBDescType_cdc_interface,
						.bDescriptorSubtype = USBDescCDCSubtype_union,
						.bMasterInterface   = 0,
						.bSlaveInterface    = { 1 }
					},
				.endpoints = // [USB CDC Endpoints].
					{
						{
							.bLength          = sizeof(struct USBDescEndpoint),
							.bDescriptorType  = USBDescType_endpoint,
							.bEndpointAddress = 2 | USBMiscFlag_endpoint_address_in,
							.bmAttributes     = USBEndpointTransferType_interrupt,
							.wMaxPacketSize   = USB_ENDPOINT_2_BUFFER_SIZE,
							.bInterval        = 64, // TODO We don't care about notifications, so we should max this out.
						},
					},
			},
		.interface_1 =
			{
				.descriptor = // [USB CDC-Data Interface].
					{
						.bLength            = sizeof(struct USBDescInterface),
						.bDescriptorType    = USBDescType_interface,
						.bInterfaceNumber   = 1,
						.bAlternateSetting  = 0,
						.bNumEndpoints      = countof(USB_CONFIGURATION_HIERARCHY.interface_1.endpoints),
						.bInterfaceClass    = USBClass_cdc_data, // See: Source(6) @ Section(4.5) @ AbsPage(40).
						.bInterfaceSubClass = 0, // Should be left alone. See: Source(6) @ Section(4.6) @ AbsPage(40).
						.bInterfaceProtocol = 0, // Not using any specific communication protocol. See: Source(6) @ Table(19) @ AbsPage(40-41).
						.iInterface         = 0, // Not important; point to empty string.
					},
				.endpoints = // [USB CDCData Endpoints].
					{
						{
							.bLength          = sizeof(struct USBDescEndpoint),
							.bDescriptorType  = USBDescType_endpoint,
							.bEndpointAddress = 3 | USBMiscFlag_endpoint_address_in,
							.bmAttributes     = USBEndpointTransferType_bulk,
							.wMaxPacketSize   = USB_ENDPOINT_3_BUFFER_SIZE,
							.bInterval        = 0,
						},
						{
							.bLength          = sizeof(struct USBDescEndpoint),
							.bDescriptorType  = USBDescType_endpoint,
							.bEndpointAddress = 4,
							.bmAttributes     = USBEndpointTransferType_bulk,
							.wMaxPacketSize   = USB_ENDPOINT_4_BUFFER_SIZE,
							.bInterval        = 0,
						},
					}
			},
	};

static const struct USBCDCLineCoding USB_COMMUNICATION_LINE_CODING = // TODO How important is this?
	{
		.dwDTERate   = 9600,
		.bCharFormat = USBMiscEnum_cdc_lc_stop_bit_1,
		.bParityType = USBMiscEnum_cdc_lc_parity_none,
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

















//
// TODO Revise.
//

/* [USB CDC Interface].
	This interface sets up the first half of the CDC (Communications Device Class)
	interface so we can send diagnostic information to the host. I will admit that
	I am quite too young to even begin to understand what modems are or the old ways of
	technology before the internet became hip and cool. Nonetheless, I will document
	my understanding of what has to be done in order to set the communication interface
	up.

	We set bInterfaceClass to USBClass_cdc to indicate to the host that this
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

/* [USB CDC Endpoints].
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

	(1) CDC Endpoints @ Source(6) @ Section(3.4.1) @ AbsPage(23).
	(2) "Management Element" @ Source(6) @ Section(1.4) @ AbsPage(16).
	(3) Endpoints on Abstract Control Models @ Source(6) @ Section(3.6.2) @ AbsPage(26).
	(4) "Notification Element" @ Source(6) @ Section(1.4) @ AbsPage(16).
	(5) "Notification Element Notifications" @ Source(6) @ Section(6.3) @ AbsPage(84).
*/

/* [USB CDC-Data Interface].
	I'm not entirely too sure what a "data class" is exactly used for here, but based
	on loose descriptions of its usage, it's to respresent the part of the communication
	where there might be compression, error correction, modulation, etc. as seen in (1).

	Regardless, this interface is where the I/O streams of the communication between
	host and device is held.

	(1) Abstract Control Model Diagram @ Source(6) @ Figure(3) @ AbsPage(15).
*/

/* [USB CDCData Endpoints].
	Stated by (1), the endpoints that will be carrying data between the host and device
	must be either both isochronous or both bulk transfer types.

	(1) "Data Class Endpoint Requirements" @ Source(6) @ Section(3.4.2) @ AbsPage(23).
*/
