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
	PinState_false    = 0b00, // Sink to GND. Alias: false.
	PinState_true     = 0b01, // Source of 5 volts. Alias: true.
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

#define USB_ENDPOINT_0_SIZE_TYPE USBEndpointSizeType_8
#define USB_ENDPOINT_0_SIZE      (8 * (1 << USB_ENDPOINT_0_SIZE_TYPE))

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

enum USBEndpointTransferType // See: Source(2) @ Table(9-13) @ Page(270) & Source(1) @ Section(22.18.2) @ Page(286).
{
	USBEndpointTransferType_control     = 0b00,
	USBEndpointTransferType_isochronous = 0b01,
	USBEndpointTransferType_bulk        = 0b10,
	USBEndpointTransferType_interrupt   = 0b11,
};

enum USBEndpointSyncType // See: Source(2) @ Table(9-13) @ Page(270).
{
	USBEndpointSyncType_null     = 0b00,
	USBEndpointSyncType_async    = 0b01,
	USBEndpointSyncType_adaptive = 0b10,
	USBEndpointSyncType_sync     = 0b11,
};

enum USBEndpointUsageType // See: Source(2) @ Table(9-13) @ Page(270).
{
	USBEndpointUsageType_data                   = 0b00,
	USBEndpointUsageType_feedback               = 0b01,
	USBEndpointUsageType_implicit_feedback_data = 0b10,
	USBEndpointUsageType_reserved               = 0b11,
};

enum USBRequestCode // See: Source(2) @ Table(9-3) @ Page(251).
{
	USBRequestCode_get_status        = 0,
	USBRequestCode_clear_feature     = 1,
	USBRequestCode_set_feature       = 3, // Note: Skipped 2!
	USBRequestCode_set_address       = 5, // Note: Skipped 4!
	USBRequestCode_get_descriptor    = 6,
	USBRequestCode_set_descriptor    = 7,
	USBRequestCode_get_configuration = 8,
	USBRequestCode_set_configuration = 9,
	USBRequestCode_get_interface     = 10,
	USBRequestCode_set_interface     = 11,
	USBRequestCode_synch_frame       = 12,
};

enum USBEndpointAddressFlag // See: "bEndpointAddress" @ Source(2) @ Table(9-13) @ Page(269).
{
	USBEndpointAddressFlag_in = 1 << 7
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
};

struct USBSetupPacket // See: Source(2) @ Table(9-2) @ Page(248).
{
	union
	{
		struct USBSetupPacketUnknown
		{
			u8  bmRequestType;
			u8  bRequest;      // Alias: enum USBRequestCode.
			u16 wValue;
			u16 wIndex;
			u16 wLength;
		} unknown;

		struct USBSetupPacketGetDescriptor
		{
			u8  bmRequestType;    // Must be 0b1000'0000.
			u8  bRequest;         // Must be USBRequestCode_get_descriptor.
			u8  descriptor_index; // Low byte of wValue.
			u8  descriptor_type;  // High byte of wValue. Alias: enum USBDescriptorType.
			u16 language_id;
			u16 wLength;
		} get_descriptor;

		struct USBSetupPacketSetAddress
		{
			u8  bmRequestType; // Must be 0b0000'0000.
			u8  bRequest;      // Must be USBRequestCode_set_address.
			u16 address;
			u32 zero;          // Expect to be zero.
		} set_address;
	};
};

enum USBClass // ID that describes the generic function of the device or interface. This enum is non-exhuastive. See: Source(5).
{
	USBClass_null          = 0x00, // When used in device descriptors, the class is determined by the interface descriptors. Cannot otherwise be used in interface descriptors as it is reserved for future use.
	USBClass_communication = 0x02,
	USBClass_human         = 0x03,
	USBClass_mass_storage  = 0x08,
	USBClass_cdc_data      = 0x0A,
};

enum USBConfigurationAttributeFlag // See: Source(2) @ Table(9-10) @ Page(266).
{
	USBConfigurationAttributeFlag_remote_wakeup = 1 << 5,
	USBConfigurationAttributeFlag_self_powered  = 1 << 6,
	USBConfigurationAttributeFlag_reserved_one  = 1 << 7, // Must always be set.
};

struct USBDescriptorDevice // [USB Device Descriptor].
{
	u8  bLength;            // Must be the size of struct USBDescriptorDevice.
	u8  bDescriptorType;    // Aliasing(enum USBDescriptorType). Must be USBDescriptorType_device.
	u16 bcdUSB;             // USB Specification version that's being complied with.
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
	u8  bmAttributes;     // Starting from LSb, attributes include enum USBEndpointTransferType, enum USBEndpointSyncType, and enum USBEndpointUsageType.
	u16 wMaxPacketSize;   // Bits 10-0 specifies the maximum data-packet size that can be sent/received. Bits 12-11 are reserved for high-speed.
	u8  bInterval;        // Amount of frames (~1ms in full-speed USB) that the endpoint will be polled for data. Isochronous must be within [0, 16] and interrupts within [1, 255].
};

struct USBConfigurationHierarchy
{
	struct USBDescriptorConfiguration configuration;

	struct
	{
		struct USBDescriptorInterface descriptor;
		u8                            TEMP_0[5];
		u8                            TEMP_1[5];
		u8                            TEMP_2[4];
		u8                            TEMP_3[5];
		struct USBDescriptorEndpoint  endpoints[1];
	} interface_0;

	struct
	{
		struct USBDescriptorInterface descriptor;
		struct USBDescriptorEndpoint  endpoints[2];
	} interface_1;
};


static const struct USBDescriptorDevice USB_DEVICE_DESCRIPTOR = // TODO PROGMEMify.
	{
		.bLength            = sizeof(struct USBDescriptorDevice),
		.bDescriptorType    = USBDescriptorType_device,
		.bcdUSB             = 0x0200,
		.bDeviceClass       = USBClass_communication,
		.bDeviceSubClass    = 0, // Irrelevant for USBClass_communication. See Source(5).
		.bDeviceProtocol    = 0, // Irrelevant for USBClass_communication. See Source(5).
		.bMaxPacketSize0    = USB_ENDPOINT_0_SIZE,
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
				.descriptor =
					{
						.bLength            = sizeof(struct USBDescriptorInterface),
						.bDescriptorType    = USBDescriptorType_interface,
						.bInterfaceNumber   = 0,
						.bAlternateSetting  = 0,
						.bNumEndpoints      = countof(USB_CONFIGURATION_HIERARCHY.interface_0.endpoints),
						.bInterfaceClass    = USBClass_communication, // See: Source(6) @ Section(4.2) @ AbsPage(20).
						.bInterfaceSubClass = 2, // "Abstract Control Model". See: Source(6) @ Section(4.3) @ AbsPage(20).
						.bInterfaceProtocol = 1, // Use the "V.250 (AT) Commands" for communication. See: Source(7) @ Section(3.2.2) @ AbsPage(15).
						.iInterface         = 0, // Not important; point to empty string.
					},
				.TEMP_0 = // TODO CDC Header Functional Descriptor, CDC Spec 5.2.3.1, Table 26
					{
						5,                       // bFunctionLength
						0x24,                    // bDescriptorType
						0x00,                    // bDescriptorSubtype
						0x10, 0x01,              // bcdCDC
					},

				.TEMP_1 = // TODO Call Management Functional Descriptor, CDC Spec 5.2.3.2, Table 27
					{
						5,                       // bFunctionLength
						0x24,                    // bDescriptorType
						0x01,                    // bDescriptorSubtype
						0x01,                    // bmCapabilities
						1,                       // bDataInterface
					},

				.TEMP_2 = // TODO Abstract Control Management Functional Descriptor, CDC Spec 5.2.3.3, Table 28
					{
						4,                       // bFunctionLength
						0x24,                    // bDescriptorType
						0x02,                    // bDescriptorSubtype
						0x06,                    // bmCapabilities
					},

				.TEMP_3 = // TODO Union Functional Descriptor, CDC Spec 5.2.3.8, Table 33
					{
						5,                       // bFunctionLength
						0x24,                    // bDescriptorType
						0x06,                    // bDescriptorSubtype
						0,                       // bMasterInterface
						1,                       // bSlaveInterface0
					},
				.endpoints =
					{
						{
							.bLength          = sizeof(struct USBDescriptorEndpoint),
							.bDescriptorType  = USBDescriptorType_endpoint,
							.bEndpointAddress = USBEndpointAddressFlag_in | 1,
							.bmAttributes     = USBEndpointTransferType_interrupt,
							.wMaxPacketSize   = 1, // TODO ????
							.bInterval        = 64, // TODO Why this value specifically?
						},
					},
			},
		.interface_1 = // TODO What is CDC-data?
			{
				.descriptor =
					{
						.bLength            = sizeof(struct USBDescriptorInterface),
						.bDescriptorType    = USBDescriptorType_interface,
						.bInterfaceNumber   = 1,
						.bAlternateSetting  = 0,
						.bNumEndpoints      = countof(USB_CONFIGURATION_HIERARCHY.interface_1.endpoints),
						.bInterfaceClass    = USBClass_cdc_data, // See: Source(6) @ Section(4.5) @ AbsPage(21).
						.bInterfaceSubClass = 0, // Should be left alone. See: Source(6) @ Section(4.6) @ AbsPage(21).
						.bInterfaceProtocol = 0, // Not using any specific communication protocol. See: Source(6) @ Section(4.7) @ AbsPage(21-22).
						.iInterface         = 0, // Not important; point to empty string.
					},
				.endpoints =
					{
						{
							.bLength          = sizeof(struct USBDescriptorEndpoint),
							.bDescriptorType  = USBDescriptorType_endpoint,
							.bEndpointAddress = USBEndpointAddressFlag_in | 2,
							.bmAttributes     = USBEndpointTransferType_bulk,
							.wMaxPacketSize   = 64, // TODO How large can we make this?
							.bInterval        = 0, // TODO What does this mean?
						},
						{ // TODO Can we remove the endpoint that sends data from host to device?
							.bLength          = sizeof(struct USBDescriptorEndpoint),
							.bDescriptorType  = USBDescriptorType_endpoint,
							.bEndpointAddress = 3,
							.bmAttributes     = USBEndpointTransferType_bulk,
							.wMaxPacketSize   = 64, // TODO How large can we make this?
							.bInterval        = 0, // TODO What does this mean?
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
	Source(6) := USB CDC Specification v1.2 (Dated: November 3, 2010).
	Source(7) := USB PSTN Specification v1.2 (Dated: February 9, 2007).

	We are working within the environment of the ATmega32U4 microcontroller,
	which is an 8-bit CPU. This consequently means that there are no padding bytes to
	worry about within structs, and that the CPU doesn't exactly have the concept of
	"endianess", since all words are single bytes, so it's really up to the compiler
	on how it will layout memory and perform arithmetic calculations on operands greater
	than a byte. That said, it seems like the MCU's opcodes are 16-bits and are stored in
	little-endian format, so AVR-GCC then also use the same convention for everything else.
	In short: no padding bytes exist and we are in little-endian.
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
