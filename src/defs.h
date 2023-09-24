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
	PinState_floating = 0b10, // Default state; reading from this pin is unreliable.
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

enum USBSetupRequestType // LSB corresponds to "bmRequestType" and MSB to "bRequest". See: Source(2) @ Table(9-2) @ Page(248).
{
	// Non-exhaustive. See: Source(2) @ Table(9-3) @ Page(250) & Source(2) @ Table(9-4) @ Page(251).
	USBSetupRequestType_get_descriptor = 0b10000000 | (((u16) 6) << 8),
	USBSetupRequestType_set_address    = 0b00000000 | (((u16) 5) << 8),
	USBSetupRequestType_set_config     = 0b00000000 | (((u16) 9) << 8),

	// Non-exhaustive. See: Source(6) @ Table(44) @ AbsPage(62-63) & Source(6) @ Table(46) @ AbsPage(64-65).
	USBSetupRequestType_cdc_get_line_coding        = 0b10100001 | (((u16) 0x21) << 8),
	USBSetupRequestType_cdc_set_line_coding        = 0b00100001 | (((u16) 0x20) << 8),
	USBSetupRequestType_cdc_set_control_line_state = 0b00100001 | (((u16) 0x22) << 8),
};

enum USBClass // Non-exhaustive. See: Source(5).
{
	USBClass_null         = 0x00, // See: [About: USBClass_null].
	USBClass_cdc          = 0x02, // See: "Communication Class Device" @ Source(6) @ Section(1) @ AbsPage(11).
	USBClass_hid          = 0x03, // TODO
	USBClass_mass_storage = 0x08, // TODO
	USBClass_cdc_data     = 0x0A, // See: "Data Interface Class" @ Source(6) @ Section(1) @ AbsPage(11).
};

enum USBDescType
{
	// See: Source(2) @ Table(9-5) @ Page(251).
	USBDescType_device             = 1,
	USBDescType_config             = 2,
	USBDescType_string             = 3, // See: [USB Strings].
	USBDescType_interface          = 4,
	USBDescType_endpoint           = 5,
	USBDescType_device_qualifier   = 6,
	USBDescType_other_speed_config = 7,
	USBDescType_interface_power    = 8,

	// See: Source(6) @ Table(24) @ AbsPage(44).
	USBDescType_cdc_interface = 0x24,
	USBDescType_cdc_endpoint  = 0x25,
};

enum USBDescCDCSubtype // Non-exhaustive. See: Source(6) @ Table(25) @ AbsPage(44-45).
{
	USBDescCDCSubtype_header          = 0x00,
	USBDescCDCSubtype_call_management = 0x01,
	USBDescCDCSubtype_acm_management  = 0x02,
	USBDescCDCSubtype_union           = 0x06,
};

enum USBMiscFlag
{
	// See: "bEndpointAddress" @ Source(2) @ Table(9-13) @ Page(269).
	USBMiscFlag_endpoint_address_in = 1 << 7,

	// See: Source(2) @ Table(9-10) @ Page(266).
	USBMiscFlag_config_attr_remote_wakeup = 1 << 5,
	USBMiscFlag_config_attr_self_powered  = 1 << 6,
	USBMiscFlag_config_attr_reserved_one  = 1 << 7, // Must always be set.
};

struct USBSetupRequest // The data-packet that is sent in the SETUP-typed transaction. See: Source(2) @ Table(9-2) @ Page(248).
{
	u16 type; // Aliasing(enum USBSetupRequestType). The "bmRequestType" and "bRequest" bytes are combined into a single word here.
	union
	{
		struct // See: Source(2) @ Section(9.4.3) @ Page(253).
		{
			u8  descriptor_index; // Only for USBDescType_string or USBDescType_config. See: Source(2) @ Section(9.4.3) @ Page(253) & [USB Strings].
			u8  descriptor_type;  // Aliasing(enum USBDescType).
			u16 language_id;      // Only for USBDescType_string. See: Source(2) @ Section(9.4.3) @ Page(253) & [USB Strings].
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
	u8  iManufacturer;      // See: [USB Strings].
	u8  iProduct;           // See: [USB Strings].
	u8  iSerialNumber;      // See: [USB Strings].
	u8  bNumConfigurations; // Count of configurations the device can be set to. See: Source(4) @ Chapter(5).
};

struct USBDescConfig // See: Source(2) @ Table(9-10) @ Page(265) & About Configurations @ Source(4) @ Chapter(5).
{
	u8  bLength;             // Must be the size of struct USBDescConfig.
	u8  bDescriptorType;     // Must be USBDescType_config.
	u16 wTotalLength;        // Amount of bytes describing the entire configuration including the configuration descriptor, endpoints, etc. See: Configuration Hierarchy Diagram @ Source(4) @ Chapter(5).
	u8  bNumInterfaces;      // Amount of interfaces that this configuration has.
	u8  bConfigurationValue; // "Value to use as an argument to the SetConfiguration() request to select this configuration". See: Source(2) @ Table(9-10) @ Page(265).
	u8  iConfiguration;      // See: [USB Strings].
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
	u8 iInterface;         // See: [USB Strings].
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

struct USBDescCDCCallManagement // See: Source(6) @ Table(27) @ AbsPage(45-46) & "Call Management" @ Source(6) @ Section(1.4) @ AbsPage(15). // TODO What is call-management?
{
	u8 bLength;            // Must be the size of struct USBDescCDCCallManagement.
	u8 bDescriptorType;    // Must be USBDescType_cdc_interface.
	u8 bDescriptorSubtype; // Must be USBDescCDCSubtype_call_management.
	u8 bmCapabilities;     // Aliasing(enum USBMiscFlag_call_management).
	u8 bDataInterface;     // Index of the CDC-Data interface that is used to transmit/receive call management (if needed).
};

// TODO Seems like m_usb handles SET_LINE_CODING and the variants just to ignore it really. So maybe we can remove this capability completely? Or perhaps this will be important in being able to do a baud-touch reset.
struct USBDescCDCACMManagement // See: Source(6) @ Table(28) @ AbsPage(46-47). // TODO What is Abstract Control Management?
{
	u8 bLength;            // Must be the size of struct USBDescCDCAbstractControlManagement.
	u8 bDescriptorType;    // Must be USBDescType_cdc_interface.
	u8 bDescriptorSubtype; // Must be USBDescCDCSubtype_acm.
	u8 bmCapabilities;     // Aliasing(enum USBMiscFlag_acm_management).
};

// TODO To what extend is this important, I'm not entirely too sure. What does the host do with this information? Can this just be removed?
struct USBDescCDCUnion // See: Source(6) @ Table(33) @ AbsPage(51). // TODO What is CDC Union?
{
	u8 bLength;            // Must be the size of struct USBDescCDCUnion.
	u8 bDescriptorType;    // Must be USBDescType_cdc_interface.
	u8 bDescriptorSubtype; // Must be USBDescCDCSubtype_union.
	u8 bMasterInterface;   // Index to the master interface.
	u8 bSlaveInterface[1]; // Index to the slave interface that is controlled by bMasterInterface. A varying amount is allowed here, but an array of 1 is all we'll need.
};

// Endpoint buffer sizes must be one of the names of enum USBEndpointSizeCode.
// The maximum capacity between endpoints also differ. See: Source(1) @ Section(22.1) @ Page(270).
#define USB_ENDPOINT_0_BUFFER_SIZE 8
#define USB_ENDPOINT_3_BUFFER_SIZE 64 // CDC-Data class to transmit communication data to host.
#define USB_ENDPOINT_4_BUFFER_SIZE 64 // CDC-Data class to receive communication data from host.

static const struct USBDescDevice USB_DEVICE_DESCRIPTOR = // TODO PROGMEMify.
	{
		.bLength            = sizeof(struct USBDescDevice),
		.bDescriptorType    = USBDescType_device,
		.bcdUSB             = 0x0200,
		.bDeviceClass       = USBClass_cdc, // We use a CDC and CDC-data interfaces, so we have to assign CDC here. See: Source(6) @ Section(3.2) @ AbsPage(20).
		.bDeviceSubClass    = 0,            // Unused. See: Source(6) @ Table(20) @ AbsPage(42).
		.bDeviceProtocol    = 0,            // Unused. See: Source(6) @ Table(20) @ AbsPage(42).
		.bMaxPacketSize0    = USB_ENDPOINT_0_BUFFER_SIZE,
		.idVendor           = 0, // IDC; doesn't seem to affect functionality.
		.idProduct          = 0, // IDC; doesn't seem to affect functionality.
		.bcdDevice          = 0, // IDC; doesn't seem to affect functionality.
		.bNumConfigurations = 1
	};

struct USBConfigHierarchy // This layout is defined uniquely for our device application. See: Source(4) @ Chapter(5).
{
	struct USBDescConfig config;

	struct
	{
		struct USBDescInterface         descriptor;
		struct USBDescCDCHeader         cdc_header;
		struct USBDescCDCCallManagement cdc_call_management;
		struct USBDescCDCACMManagement  cdc_acm_management;
		struct USBDescCDCUnion          cdc_union;
	} cdc;

	struct
	{
		struct USBDescInterface descriptor;
		struct USBDescEndpoint  endpoints[2];
	} cdc_data;
};

#define USB_CONFIGURATION_HIERARCHY_CONFIGURATION_VALUE 1 // Argument used for "SetConfiguration" command. Zero resets the device's configuration state, so 1 is used instead. See: Soure(2) @ Figure(11-10) @ Page(310).
static const struct USBConfigHierarchy USB_CONFIGURATION_HIERARCHY =
	{
		.config =
			{
				.bLength             = sizeof(struct USBDescConfig),
				.bDescriptorType     = USBDescType_config,
				.wTotalLength        = sizeof(struct USBConfigHierarchy),
				.bNumInterfaces      = 2,
				.bConfigurationValue = USB_CONFIGURATION_HIERARCHY_CONFIGURATION_VALUE,
				.bmAttributes        = USBMiscFlag_config_attr_reserved_one | USBMiscFlag_config_attr_self_powered, // TODO We should calculate our power consumption!
				.bMaxPower           = 50, // TODO We should calculate our power consumption!
			},
		.cdc =
			{
				.descriptor =
					{
						.bLength            = sizeof(struct USBDescInterface),
						.bDescriptorType    = USBDescType_interface,
						.bInterfaceNumber   = 0,
						.bAlternateSetting  = 0,
						.bNumEndpoints      = 0,
						.bInterfaceClass    = USBClass_cdc, // See: Source(6) @ Section(4.2) @ AbsPage(39).
						.bInterfaceSubClass = 0x2, // See: "Abstract Control Model" @ Source(6) @ Section(3.6.2) @ AbsPage(26).
						.bInterfaceProtocol = 0x1, // See: "V.25ter" @ Source(6) @ Table(17) @ AbsPage(39). TODO Is this protocol needed?
					},
				.cdc_header =
					{
						.bLength            = sizeof(struct USBDescCDCHeader),
						.bDescriptorType    = USBDescType_cdc_interface,
						.bDescriptorSubtype = USBDescCDCSubtype_header,
						.bcdCDC             = 0x0110,
					},
				.cdc_call_management =
					{
						.bLength            = sizeof(struct USBDescCDCCallManagement),
						.bDescriptorType    = USBDescType_cdc_interface,
						.bDescriptorSubtype = USBDescCDCSubtype_call_management,
						.bmCapabilities     = 0,
						.bDataInterface     = 1,
					},
				.cdc_acm_management =
					{
						.bLength            = sizeof(struct USBDescCDCACMManagement),
						.bDescriptorType    = USBDescType_cdc_interface,
						.bDescriptorSubtype = USBDescCDCSubtype_acm_management,
						.bmCapabilities     = 0,
					},
				.cdc_union =
					{
						.bLength            = sizeof(struct USBDescCDCUnion),
						.bDescriptorType    = USBDescType_cdc_interface,
						.bDescriptorSubtype = USBDescCDCSubtype_union,
						.bMasterInterface   = 0,
						.bSlaveInterface    = { 1 }
					},
			},
		.cdc_data =
			{
				.descriptor =
					{
						.bLength            = sizeof(struct USBDescInterface),
						.bDescriptorType    = USBDescType_interface,
						.bInterfaceNumber   = 1,
						.bAlternateSetting  = 0,
						.bNumEndpoints      = countof(USB_CONFIGURATION_HIERARCHY.cdc_data.endpoints),
						.bInterfaceClass    = USBClass_cdc_data, // See: Source(6) @ Section(4.5) @ AbsPage(40).
						.bInterfaceSubClass = 0, // Should be left alone. See: Source(6) @ Section(4.6) @ AbsPage(40).
						.bInterfaceProtocol = 0, // Not using any specific communication protocol. See: Source(6) @ Table(19) @ AbsPage(40-41).
					},
				.endpoints =
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

/* [USB Strings].
	USB devices can return strings of usually human-readable text back to the host for
	things such as the name of the manufacturer. (1), however, states that devices are
	not required at all to implement this, and thus we will not bother with string
	descriptors.

	(1) "String" @ Source(2) @ Section(9.6.7) @ Page(273).
*/

/* [About: USBClass_null].
	When USBClass_null is used in device descriptors (bDeviceClass), the class is
	determined by the interfaces of the configurations (bInterfaceClass). (1) says that
	USBClass_null cannot be used in interface descriptors as it is "reserved for future
	use". However, (2) states that it is for the "null class code triple" now,
	whatever that means!

	(1) "bInterfaceClass" @ Source(2) @ Table(9-12) @ Page(268).
	(2) "Base Class 00h (Device)" @ Source(5).
*/
