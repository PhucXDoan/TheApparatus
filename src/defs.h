// TODO Interrupt behavior on USB disconnection and reconnection?
#define false            0
#define true             1
#define stringify_(X)    #X
#define stringify(X)     stringify_(X)
#define concat_(X, Y)    X##Y
#define concat(X, Y)     concat_(X, Y)
#define countof(...)     (sizeof(__VA_ARGS__)/sizeof((__VA_ARGS__)[0]))
#define bitsof(...)      (sizeof(__VA_ARGS__) * 8)
#define static_assert(X) _Static_assert((X), #X);

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef int8_t   b8;
typedef int16_t  b16;
typedef int32_t  b32;
typedef int64_t  b64;

//
// "pin.c"
//

#define PIN_ERROR 2
enum PinErrorSource
{
	PinErrorSource_diplomat = 0,
	PinErrorSource_usb      = 1,
};

enum PinState
{
	PinState_false    = 0b00, // Aliasing(false). Sink to GND.
	PinState_true     = 0b01, // Aliasing(true). Source of 5 volts.
	PinState_floating = 0b10, // Default state; reading from this pin is unreliable.
	PinState_input    = 0b11, // Pull-up resistor enabled for reliable reads. A truthy value will be read unless it is tied to GND strongly enough.
};

// Maps simple pin numberings to corresponding data-direction ports and bit-index. See: Source(3).
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
	USBEndpointTransferType_isochronous = 0b01, // Bounded latency, one-way, guaranteed data bandwidth (but no guarantee of successful delivery). Ex: audio/video.
	USBEndpointTransferType_bulk        = 0b10, // No latency guarantees, one-way, large amount of data. Ex: file upload.
	USBEndpointTransferType_interrupt   = 0b11, // Low-latency, one-way, small amount of data. Ex: keyboard.
};

enum USBClass // Non-exhaustive. See: Source(5).
{
	USBClass_null         = 0x00, // See: [About: USBClass_null].
	USBClass_cdc          = 0x02, // See: "Communication Class Device" @ Source(6) @ Section(1) @ AbsPage(11).
	USBClass_hid          = 0x03, // TODO
	USBClass_mass_storage = 0x08, // TODO
	USBClass_cdc_data     = 0x0A, // See: "Data Interface Class" @ Source(6) @ Section(1) @ AbsPage(11).
};

enum USBSetupRequestType // See: Source(2) @ Table(9-2) @ Page(248).
{
	#define MAKE(BM_REQUEST_TYPE, B_REQUEST) ((BM_REQUEST_TYPE) | (((u16) (B_REQUEST)) << 8))

	// Non-exhaustive. See: Source(2) @ Table(9-3) @ Page(250) & Source(2) @ Table(9-4) @ Page(251).
	USBSetupRequestType_get_desc    = MAKE(0b10000000, 6),
	USBSetupRequestType_set_address = MAKE(0b00000000, 5),
	USBSetupRequestType_set_config  = MAKE(0b00000000, 9),

	// Non-exhaustive. See: Source(6) @ Table(44) @ AbsPage(62-63) & Source(6) @ Table(46) @ AbsPage(64-65).
	USBSetupRequestType_cdc_get_line_coding        = MAKE(0b10100001, 0x21),
	USBSetupRequestType_cdc_set_line_coding        = MAKE(0b00100001, 0x20),
	USBSetupRequestType_cdc_set_control_line_state = MAKE(0b00100001, 0x22),

	#undef MAKE
};

struct USBSetupRequest // The data-packet that is sent in the SETUP-transaction. See: Source(2) @ Table(9-2) @ Page(248).
{
	u16 type; // Aliasing(enum USBSetupRequestType). The "bmRequestType" and "bRequest" bytes are combined into a single word here.
	union
	{
		struct // See: "GetDescriptor" @ Source(2) @ Section(9.4.3) @ Page(253).
		{
			u8  desc_index;       // Only for USBDescType_string or USBDescType_config. See: Source(2) @ Section(9.4.3) @ Page(253) & [USB Strings].
			u8  desc_type;        // Aliasing(enum USBDescType).
			u16 language_id;      // Only for USBDescType_string. See: Source(2) @ Section(9.4.3) @ Page(253) & [USB Strings].
			u16 requested_amount; // Amount of data the host expects back from the device.
		} get_desc;

		struct // See: "SetAddress" @ // See: Source(2) @ Section(9.4.6) @ Page(256).
		{
			u16 address; // Must be within 7-bits.
		} set_address;

		struct // See: "SetConfiguration" @ See: Source(2) @ Section(9.4.7) @ Page(257).
		{
			u16 value; // The configuration the host wants to set the device to. See: "bConfigurationValue" @ Source(2) @ Table(9-10) @ Page(265).
		} set_config;

		struct // See: CDC's "SetLineCoding" @ Source(6) @ Setion(6.2.12) @ AbsPage(68-69).
		{
			u16 _zero;
			u16 designated_interface_index;           // Index of the interface that the host wants to set the line-coding for. Should be to our only CDC-interface.
			u16 incoming_line_coding_datapacket_size; // Amount of bytes the host will be sending. Should be sizeof(struct USBCDCLineCoding).
		} cdc_set_line_coding;
	};
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
	USBDescType_interface_power    = 8, // PS: Can't find "USB Interface Power Management Specification" that supposedly defines the INTERFACE_POWER descriptor...

	// See: Source(6) @ Table(24) @ AbsPage(44).
	USBDescType_cdc_interface = 0x24,
	USBDescType_cdc_endpoint  = 0x25,
};

struct USBDescDevice // See: Source(2) @ Table(9-8) @ Page(262-263).
{
	u8  bLength;            // Must be sizeof(struct USBDescDevice).
	u8  bDescriptorType;    // Must be USBDescType_device.
	u16 bcdUSB;             // USB specification version that's being complied with.
	u8  bDeviceClass;       // Aliasing(enum USBClass).
	u8  bDeviceSubClass;    // Optionally used to further subdivide the device class. See: Source(2) @ Section(9.2.3) @ Page(245).
	u8  bDeviceProtocol;    // Optionally used to indicate to the host on how to communicate with the device. See: Source(2) @ Section(9.2.3) @ Page(245).
	u8  bMaxPacketSize0;    // Most amount of bytes endpoint 0 can send, i.e. USB_ENDPOINT_DFLT_SIZE.
	u16 idVendor;           // UID assigned by USB-IF for a company. With idProduct, this helps the host find drivers for the device. See: Source(4) @ Chapter(5).
	u16 idProduct;          // UID arbitrated by the vendor for a specific device made by them.
	u16 bcdDevice;          // Version number of the device.
	u8  iManufacturer;      // See: [USB Strings].
	u8  iProduct;           // See: [USB Strings].
	u8  iSerialNumber;      // See: [USB Strings].
	u8  bNumConfigurations; // Amount of configurations the device can be set to. Only a single configuration (USB_CONFIGURATION_HIERARCHY) is defined, so this is 1.
};

enum USBConfigAttrFlag // See: "bmAttributes" @ Source(2) @ Table(9-10) @ Page(266).
{
	USBConfigAttrFlag_remote_wakeup = 1 << 5,
	USBConfigAttrFlag_self_powered  = 1 << 6,
	USBConfigAttrFlag_reserved_one  = 1 << 7, // Must always be set.
};
struct USBDescConfig // See: Source(2) @ Table(9-10) @ Page(265) & About Configurations @ Source(4) @ Chapter(5).
{
	u8  bLength;             // Must be sizeof(struct USBDescConfig).
	u8  bDescriptorType;     // Must be USBDescType_config.
	u16 wTotalLength;        // Amount of bytes to describe the entire contiguous configuration hierarchy. See: Configuration Hierarchy Diagram @ Source(4) @ Chapter(5).
	u8  bNumInterfaces;      // Amount of interfaces that this configuration has.
	u8  bConfigurationValue; // Argument passed by SetConfiguration to select this configuration. See: Source(2) @ Table(9-10) @ Page(265).
	u8  iConfiguration;      // See: [USB Strings].
	u8  bmAttributes;        // Aliasing(enum USBConfigAttrFlag).
	u8  bMaxPower;           // Expressed in units of 2mA (e.g. bMaxPower = 50 -> 100mA usage).
};

struct USBDescInterface // See: Source(2) @ Table(9-12) @ Page(268-269) & About Interfaces @ Source(4) @ Chapter(5).
{
	u8 bLength;            // Must be sizeof(struct USBDescInterface).
	u8 bDescriptorType;    // Must be USBDescType_interface.
	u8 bInterfaceNumber;   // Index of the interface within the configuration.
	u8 bAlternateSetting;  // If 1, then this interface is an alternate for the interface at index bInterfaceNumber.
	u8 bNumEndpoints;      // Number of endpoints (that aren't endpoint 0) used by this interface.
	u8 bInterfaceClass;    // Aliasing(enum USBClass). Value of USBClass_null is reserved.
	u8 bInterfaceSubClass; // Optionally used to further subdivide the interface class.
	u8 bInterfaceProtocol; // Optionally used to indicate to the host on how to communicate with the device.
	u8 iInterface;         // See: [USB Strings].
};

enum USBEndpointAddressFlag // See: "bEndpointAddress" @ Source(2) @ Table(9-13) @ Page(269).
{
	USBEndpointAddressFlag_in = 1 << 7,
};
struct USBDescEndpoint // See: Source(2) @ Table(9-13) @ Page(269-271) & Configuration Hierarchy Diagram @ Source(4) @ Chapter(5).
{
	u8  bLength;          // Must be sizeof(struct USBDescEndpoint).
	u8  bDescriptorType;  // Must be USBDescType_endpoint.
	u8  bEndpointAddress; // The "N" in "Endpoint N". Addresses must be specified in low-nibble. Can be applied with enum USBEndpointAddressFlag.
	u8  bmAttributes;     // Aliasing(enum USBEndpointTransferType). Other bits have meanings too, but only for isochronous endpoints, which we don't use.
	u16 wMaxPacketSize;   // Bits 10-0 specifies the maximum data-packet size that can be sent/received to/from this endpoint. Bits 12-11 are reserved for high-speed.
	u8  bInterval;        // Frames (1ms for full-speed USB) between each polling of this endpoint. Valid values differ depending on this endpoint's transfer-type.
};

enum USBDescCDCSubtype // Non-exhaustive. See: Source(6) @ Table(25) @ AbsPage(44-45).
{
	USBDescCDCSubtype_header          = 0x00,
	USBDescCDCSubtype_call_management = 0x01,
	USBDescCDCSubtype_acm_management  = 0x02,
	USBDescCDCSubtype_union           = 0x06,
};

struct USBDescCDCHeader // Source(6) @ Section(5.2.3.1) @ AbsPage(45).
{
	u8  bLength;            // Must be sizeof(struct USBDescCDCHeader).
	u8  bDescriptorType;    // Must be USBDescType_cdc_interface.
	u8  bDescriptorSubtype; // Must be USBDescCDCSubtype_header.
	u16 bcdCDC;             // CDC specification version that's being complied with.
};

struct USBDescCDCCallManagement // See: Source(6) @ Table(27) @ AbsPage(45-46).
{
	u8 bLength;            // Must be sizeof(struct USBDescCDCCallManagement).
	u8 bDescriptorType;    // Must be USBDescType_cdc_interface.
	u8 bDescriptorSubtype; // Must be USBDescCDCSubtype_call_management.
	u8 bmCapabilities;     // Describes what the CDC device is capable of in terms of call-management. Seems irrelevant for functionality.
	u8 bDataInterface;     // Index of CDC-data interface to receive call-management comamnds. Seems irrelevant for functionality.
};

struct USBDescCDCACMManagement // See: Source(6) @ Table(28) @ AbsPage(46-47).
{
	u8 bLength;            // Must be sizeof(struct USBDescCDCACMManagement).
	u8 bDescriptorType;    // Must be USBDescType_cdc_interface.
	u8 bDescriptorSubtype; // Must be USBDescCDCSubtype_acm.
	u8 bmCapabilities;     // Describes what commands the host can use for the CDC-interface. Seems irrelevant for functionality (despite handling SetControlLineState).
};

struct USBDescCDCUnion // See: Source(6) @ Table(33) @ AbsPage(51).
{
	u8 bLength;            // Must be sizeof(struct USBDescCDCUnion).
	u8 bDescriptorType;    // Must be USBDescType_cdc_interface.
	u8 bDescriptorSubtype; // Must be USBDescCDCSubtype_union.
	u8 bMasterInterface;   // Index to the master-interface.
	u8 bSlaveInterface[1]; // Indices to slave-interfaces that belong to bMasterInterface. USB allows varying amounts is allowed here, but 1 is all we need.
};

struct USBCDCLineCoding // See: Source(6) @ Table(50) @ AbsPage(69).
{
	u32 dwDTERate;
	u8  bCharFormat;
	u8  bParityType;
	u8  bDataBits;
};

// Endpoint buffer sizes must be one of the names of enum USBEndpointSizeCode.
// The maximum capacity between endpoints also differ. See: Source(1) @ Section(22.1) @ Page(270).

#define USB_ENDPOINT_DFLT                  0                               // "Default Endpoint" is synonymous with endpoint 0.
#define USB_ENDPOINT_DFLT_TRANSFER_TYPE    USBEndpointTransferType_control // The default endpoint is always a control-typed endpoint.
#define USB_ENDPOINT_DFLT_SIZE             8
#define USB_ENDPOINT_DFLT_CODE_SIZE        concat(USBEndpointSizeCode_, USB_ENDPOINT_DFLT_SIZE)

#define USB_ENDPOINT_CDC_IN                2
#define USB_ENDPOINT_CDC_IN_TRANSFER_TYPE  USBEndpointTransferType_bulk
#define USB_ENDPOINT_CDC_IN_SIZE           64
#define USB_ENDPOINT_CDC_IN_SIZE_CODE      concat(USBEndpointSizeCode_, USB_ENDPOINT_CDC_IN_SIZE)

#define USB_ENDPOINT_CDC_OUT               3
#define USB_ENDPOINT_CDC_OUT_TRANSFER_TYPE USBTransferType_bulk
#define USB_ENDPOINT_CDC_OUT_SIZE          64
#define USB_ENDPOINT_CDC_OUT_SIZE_CODE     concat(USBEndpointSizeCode_, USB_ENDPOINT_CDC_OUT_SIZE)

static const struct USBDescDevice USB_DEVICE_DESCRIPTOR = // TODO PROGMEMify.
	{
		.bLength            = sizeof(struct USBDescDevice),
		.bDescriptorType    = USBDescType_device,
		.bcdUSB             = 0x0200,
		.bDeviceClass       = USBClass_cdc, // We use a CDC and CDC-data interfaces, so we have to assign CDC here. See: Source(6) @ Section(3.2) @ AbsPage(20).
		.bDeviceSubClass    = 0,            // Unused. See: Source(6) @ Table(20) @ AbsPage(42).
		.bDeviceProtocol    = 0,            // Unused. See: Source(6) @ Table(20) @ AbsPage(42).
		.bMaxPacketSize0    = USB_ENDPOINT_DFLT_SIZE,
		.idVendor           = 0, // Seems irrelevant for functionality.
		.idProduct          = 0, // Seems irrelevant for functionality.
		.bcdDevice          = 0, // Seems irrelevant for functionality.
		.bNumConfigurations = 1
	};

struct USBConfigHierarchy // This layout is defined uniquely for our device application.
{
	struct USBDescConfig config;

	struct
	{
		struct USBDescInterface         desc;
		struct USBDescCDCHeader         cdc_header;
		struct USBDescCDCCallManagement cdc_call_management;
		struct USBDescCDCACMManagement  cdc_acm_management;
		struct USBDescCDCUnion          cdc_union;
	} cdc;

	struct
	{
		struct USBDescInterface desc;
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
				.bmAttributes        = USBConfigAttrFlag_reserved_one | USBConfigAttrFlag_self_powered, // TODO We should calculate our power consumption!
				.bMaxPower           = 50,                                                              // TODO We should calculate our power consumption!
			},
		.cdc =
			{
				.desc =
					{
						.bLength            = sizeof(struct USBDescInterface),
						.bDescriptorType    = USBDescType_interface,
						.bInterfaceNumber   = 0,
						.bAlternateSetting  = 0,
						.bNumEndpoints      = 0,
						.bInterfaceClass    = USBClass_cdc, // See: Source(6) @ Section(4.2) @ AbsPage(39).
						.bInterfaceSubClass = 0x2,          // See: "Abstract Control Model" @ Source(6) @ Table(16) @ AbsPage(39).
						.bInterfaceProtocol = 0,            // Seems irrelevant for functionality. See: Source(6) @ Table(17) @ AbsPage(40).
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
					},
				.cdc_acm_management =
					{
						.bLength            = sizeof(struct USBDescCDCACMManagement),
						.bDescriptorType    = USBDescType_cdc_interface,
						.bDescriptorSubtype = USBDescCDCSubtype_acm_management,
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
				.desc =
					{
						.bLength            = sizeof(struct USBDescInterface),
						.bDescriptorType    = USBDescType_interface,
						.bInterfaceNumber   = 1,
						.bAlternateSetting  = 0,
						.bNumEndpoints      = countof(USB_CONFIGURATION_HIERARCHY.cdc_data.endpoints),
						.bInterfaceClass    = USBClass_cdc_data, // See: Source(6) @ Section(4.5) @ AbsPage(40).
						.bInterfaceSubClass = 0, // Should be left alone. See: Source(6) @ Section(4.6) @ AbsPage(40).
						.bInterfaceProtocol = 0, // Seems irrelevant for functionality. See: Source(6) @ Table(19) @ AbsPage(40-41).
					},
				.endpoints =
					{
						{
							.bLength          = sizeof(struct USBDescEndpoint),
							.bDescriptorType  = USBDescType_endpoint,
							.bEndpointAddress = USB_ENDPOINT_CDC_IN | USBEndpointAddressFlag_in,
							.bmAttributes     = USBEndpointTransferType_bulk,
							.wMaxPacketSize   = USB_ENDPOINT_CDC_IN_SIZE,
							.bInterval        = 0,
						},
						{
							.bLength          = sizeof(struct USBDescEndpoint),
							.bDescriptorType  = USBDescType_endpoint,
							.bEndpointAddress = USB_ENDPOINT_CDC_OUT,
							.bmAttributes     = USBEndpointTransferType_bulk,
							.wMaxPacketSize   = USB_ENDPOINT_CDC_OUT_SIZE,
							.bInterval        = 0,
						},
					}
			},
	};

static u8 _usb_cdc_in_buffer [USB_ENDPOINT_CDC_IN_SIZE ] = {0};
static u8 _usb_cdc_out_buffer[USB_ENDPOINT_CDC_OUT_SIZE] = {0};

static          u8 _usb_cdc_in_writer  =  0; // Main program writes.
static volatile u8 _usb_cdc_in_reader  = -1; // Interrupt routine reads. Must be before _usb_cdc_in_writer.
static volatile u8 _usb_cdc_out_writer =  0; // Interrupt routine writes.
static          u8 _usb_cdc_out_reader = -1; // Main program reads. Must be before _usb_cdc_out_writer.

#define _usb_cdc_in_writer_masked(OFFSET)  ((_usb_cdc_in_writer  + (OFFSET)) & (countof(_usb_cdc_in_buffer ) - 1))
#define _usb_cdc_in_reader_masked(OFFSET)  ((_usb_cdc_in_reader  + (OFFSET)) & (countof(_usb_cdc_in_buffer ) - 1))
#define _usb_cdc_out_writer_masked(OFFSET) ((_usb_cdc_out_writer + (OFFSET)) & (countof(_usb_cdc_out_buffer) - 1))
#define _usb_cdc_out_reader_masked(OFFSET) ((_usb_cdc_out_reader + (OFFSET)) & (countof(_usb_cdc_out_buffer) - 1))

// A read/write index with a size greater than a byte makes "atomic" read/write operations difficult to guarantee; it can be done, but probably not necessary.
static_assert(sizeof(_usb_cdc_in_writer) == 1 && sizeof(_usb_cdc_in_reader) == 1 && sizeof(_usb_cdc_out_writer) == 1 && sizeof(_usb_cdc_out_reader) == 1);

// The read/write indices must be able to address any element in the corresponding buffer.
static_assert(countof(_usb_cdc_in_buffer ) < (((u64) 1) << bitsof(_usb_cdc_in_reader )));
static_assert(countof(_usb_cdc_in_buffer ) < (((u64) 1) << bitsof(_usb_cdc_in_writer )));
static_assert(countof(_usb_cdc_out_buffer) < (((u64) 1) << bitsof(_usb_cdc_out_reader)));
static_assert(countof(_usb_cdc_out_buffer) < (((u64) 1) << bitsof(_usb_cdc_out_writer)));

// Buffer sizes must be a power of two for the "_usb_cdc_X_Y_masked" macros.
static_assert(countof(_usb_cdc_in_buffer ) && !(countof(_usb_cdc_in_buffer ) & (countof(_usb_cdc_in_buffer ) - 1)));
static_assert(countof(_usb_cdc_out_buffer) && !(countof(_usb_cdc_out_buffer) & (countof(_usb_cdc_out_buffer) - 1)));

#if DEBUG
static b8 debug_usb_rx_diagnostic_signal = false;
#endif

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
	than a byte. That said, the MCU's 16-bit opcodes and AVR-GCC's ABI are defined to use
	little-endian. In short: no padding bytes exist and we can take advantage of LSB being first
	before the MSB.

	The USB protocol is pretty intense. Hopefully this codebase is well documented
	enough that it is easy to follow along to understand why things are being done
	the way they are. If they aren't, then many citations and sources are linked to
	sites, datasheets, specifications, etc. that describes a particular part in more
	detail. "NEFASTOR ONLINE" (nefastor.com/micrcontrollers/stm32/usb/) contains some
	articles explaining parts of the USB protocol, and Ben Eater on YouTube goes pretty
	in-depth on the electrical USB transmission sides of things.

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
