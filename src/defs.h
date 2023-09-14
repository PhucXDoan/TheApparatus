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

// TODO Document.
enum PinState
{
	PinState_false    = 0b00, // Alias: false.
	PinState_true     = 0b01, // Alias: true.
	PinState_floating = 0b10,
	PinState_input    = 0b11
};

// TODO Document.
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
	X(19, F, 0)

//
// USB.
//

#define USB_ENDPOINT_0_SIZE_TYPE USBEndpointSizeType_8
#define USB_ENDPOINT_0_SIZE      (1 << (3 + USB_ENDPOINT_0_SIZE_TYPE))

enum USBEndpointSizeType // Source(1) @ Section(22.18.2) @ Page(287).
{
	USBEndpointSizeType_8   = 0b000,
	USBEndpointSizeType_16  = 0b001,
	USBEndpointSizeType_32  = 0b010,
	USBEndpointSizeType_64  = 0b011,
	USBEndpointSizeType_128 = 0b100,
	USBEndpointSizeType_256 = 0b101,
	USBEndpointSizeType_512 = 0b110,
};

enum USBStandardRequestCode // Source(2) @ Table(9-3) @ Page(251).
{
	USBStandardRequestCode_get_status        = 0,
	USBStandardRequestCode_clear_feature     = 1,
	USBStandardRequestCode_set_feature       = 3, // Note: Skipped 2!
	USBStandardRequestCode_set_address       = 5, // Note: Skipped 4!
	USBStandardRequestCode_get_descriptor    = 6,
	USBStandardRequestCode_set_descriptor    = 7,
	USBStandardRequestCode_get_configuration = 8,
	USBStandardRequestCode_set_configuration = 9,
	USBStandardRequestCode_get_interface     = 10,
	USBStandardRequestCode_set_interface     = 11,
	USBStandardRequestCode_synch_frame       = 12,
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

// TODO Document.
__attribute__((packed))
struct USBStandardDescriptor // Source(2) @ Table(9-8) @ Page(262-263).
{
	u8 bLength;            // Size of the descriptor which should always be 18 bytes.
	u8 bDescriptorType;    // Alias: enum USBDescriptorType.
	u8 bcdUSB[2];          // USB Specification version that's being complied with.
	u8 bDeviceClass;       // TODO Document.
	u8 bDeviceSubClass;    // TODO Document.
	u8 bDevceProtocol;     // TODO Document.
	u8 bMaxPacketSize0;    // Max packet size of endpoint 0.
	u8 idVendor[2];        // TODO Document.
	u8 idProduct[2];       // TODO Document.
	u8 bcdDevice[2];       // TODO Document.
	u8 iManufacturer;      // Zero-based index of string descriptor for the manufacturer description.
	u8 iProduct;           // Zero-based index of string descriptor for the product description.
	u8 iSerialNumber;      // Zero-based index of string descriptor for the device's serial number.
	u8 bNumConfigurations; // TODO Document.
};

__attribute__((packed))
struct USBSetupPacket // See: Source(2) @ Table(9-2) @ Page(248).
{
	union
	{
		struct USBSetupPacketUnknown
		{
			u8 bmRequestType;
			u8 bRequest;         // Alias: enum USBStandardRequestCode.
			u8 wValue_parts[2];
			u8 wIndex_parts[2];
			u8 wLength_parts[2];
		} unknown;

		struct USBSetupPacketGetDescriptor
		{
			u8 bmRequestType;        // Should be 0b1000'0000.
			u8 bRequest;             // Should be USBStandardRequestCode_get_descriptor.
			u8 descriptor_index;     // Low byte of wValue.
			u8 descriptor_type;      // High byte of wValue. Alias: enum USBDescriptorType.
			u8 language_id_parts[2];
			u8 wLength_parts[2];
		} get_descriptor;

		struct USBSetupPacketSetAddress
		{
			u8  bmRequestType;    // Should be 0b0000'0000.
			u8  bRequest;         // Should be USBStandardRequestCode_set_address.
			u8  address_parts[2];
			u32 zeros;            // Should all be zeros.
		} set_address;
	};
};

// TODO Document.
// TODO PROGMEMify.
static const struct USBStandardDescriptor USB_DEVICE_DESCRIPTOR =
	{
		18,					// bLength
		1,					// bDescriptorType
		{ 0x00, 0x02 },				// bcdUSB
		2,					// bDeviceClass
		0,					// bDeviceSubClass
		0,					// bDeviceProtocol
		USB_ENDPOINT_0_SIZE,				// bMaxPacketSize0
		{ 0xC0, 0x16 },		// idVendor
		{ 0x7A, 0x04 },	// idProduct
		{ 0x00, 0x01 },				// bcdDevice
		1,					// iManufacturer
		2,					// iProduct
		3,					// iSerialNumber
		1					// bNumConfigurations

//		.bLength            = sizeof(struct USBStandardDescriptor),
//		.bDescriptorType    = USBDescriptorType_device,
//		.bcdUSB             = { 0x00, 0x02 },
//		.bDeviceClass       = 0,   // TEMP
//		.bDeviceSubClass    = 0,   // TEMP
//		.bDevceProtocol     = 0,   // TEMP
//		.bMaxPacketSize0    = USB_ENDPOINT_0_SIZE,
//		.idVendor           = {0}, // TEMP
//		.idProduct          = {0}, // TEMP
//		.bcdDevice          = {0}, // TEMP
//		.iManufacturer      = 0,   // TEMP
//		.iProduct           = 0,   // TEMP
//		.iSerialNumber      = 0,   // TEMP
//		.bNumConfigurations = 1,
	};

// TODO Copypasta from m_usb.
#define CONFIG1_DESC_SIZE (9+9+5+5+4+5+7+9+7+7)
#define ENDPOINT0_SIZE		16
#define CDC_ACM_ENDPOINT	2
#define CDC_RX_ENDPOINT		3
#define CDC_TX_ENDPOINT		4
#define CDC_ACM_SIZE		16
#define CDC_ACM_BUFFER		EP_SINGLE_BUFFER
#define CDC_RX_SIZE		64
#define CDC_RX_BUFFER 		EP_DOUBLE_BUFFER
#define CDC_TX_SIZE		64
#define CDC_TX_BUFFER		EP_DOUBLE_BUFFER
static const u8 USB_CONFIGURATION[] =
	{
		// configuration descriptor, USB spec 9.6.3, page 264-266, Table 9-10
		9, 					// bLength;
		2,					// bDescriptorType;
		(CONFIG1_DESC_SIZE) & 0xFF,			// wTotalLength
		(CONFIG1_DESC_SIZE >> 8) & 0xFF,
		2,					// bNumInterfaces
		1,					// bConfigurationValue
		0,					// iConfiguration
		0xC0,					// bmAttributes
		50,					// bMaxPower
		// interface descriptor, USB spec 9.6.5, page 267-269, Table 9-12
		9,					// bLength
		4,					// bDescriptorType
		0,					// bInterfaceNumber
		0,					// bAlternateSetting
		1,					// bNumEndpoints
		0x02,					// bInterfaceClass
		0x02,					// bInterfaceSubClass
		0x01,					// bInterfaceProtocol
		0,					// iInterface
		// CDC Header Functional Descriptor, CDC Spec 5.2.3.1, Table 26
		5,					// bFunctionLength
		0x24,					// bDescriptorType
		0x00,					// bDescriptorSubtype
		0x10, 0x01,				// bcdCDC
		// Call Management Functional Descriptor, CDC Spec 5.2.3.2, Table 27
		5,					// bFunctionLength
		0x24,					// bDescriptorType
		0x01,					// bDescriptorSubtype
		0x01,					// bmCapabilities
		1,					// bDataInterface
		// Abstract Control Management Functional Descriptor, CDC Spec 5.2.3.3, Table 28
		4,					// bFunctionLength
		0x24,					// bDescriptorType
		0x02,					// bDescriptorSubtype
		0x06,					// bmCapabilities
		// Union Functional Descriptor, CDC Spec 5.2.3.8, Table 33
		5,					// bFunctionLength
		0x24,					// bDescriptorType
		0x06,					// bDescriptorSubtype
		0,					// bMasterInterface
		1,					// bSlaveInterface0
		// endpoint descriptor, USB spec 9.6.6, page 269-271, Table 9-13
		7,					// bLength
		5,					// bDescriptorType
		CDC_ACM_ENDPOINT | 0x80,		// bEndpointAddress
		0x03,					// bmAttributes (0x03=intr)
		CDC_ACM_SIZE, 0,			// wMaxPacketSize
		64,					// bInterval
		// interface descriptor, USB spec 9.6.5, page 267-269, Table 9-12
		9,					// bLength
		4,					// bDescriptorType
		1,					// bInterfaceNumber
		0,					// bAlternateSetting
		2,					// bNumEndpoints
		0x0A,					// bInterfaceClass
		0x00,					// bInterfaceSubClass
		0x00,					// bInterfaceProtocol
		0,					// iInterface
		// endpoint descriptor, USB spec 9.6.6, page 269-271, Table 9-13
		7,					// bLength
		5,					// bDescriptorType
		CDC_RX_ENDPOINT,			// bEndpointAddress
		0x02,					// bmAttributes (0x02=bulk)
		CDC_RX_SIZE, 0,				// wMaxPacketSize
		0,					// bInterval
		// endpoint descriptor, USB spec 9.6.6, page 269-271, Table 9-13
		7,					// bLength
		5,					// bDescriptorType
		CDC_TX_ENDPOINT | 0x80,			// bEndpointAddress
		0x02,					// bmAttributes (0x02=bulk)
		CDC_TX_SIZE, 0,				// wMaxPacketSize
		0					// bInterval
	};

//
// Internal Documentation.
//

/*
	Source(1) @ ATmega32U4 Datasheet ("Atmel-7766J-USB-ATmega16U4/32U4-Datasheet_04/2016").
	Source(2) @ USB 2.0 Specification @ ("Universal Serial Bus Specification Revision 2.0", April 27, 2000).
	Source(3) @ Arduino Leonardo Pinout Diagram (STORE.ARDUINO.CC/LEONARDO, 17/06/2020).

	TODO Redo documentation style.
	TODO Interrupt behavior on USB disconnection?
	TODO AVR-GCC is being an absolute pain and is complaining about `int x = {0};`. Disable the warning!
	TODO How does endianess work? Is doing shifts for u8s to make u16 the same as just interpreting as u16?
	TODO is __attribute__((packed)) necessary?
*/

// Note that the USB specification seems to call the descriptor
// the "USB Standard Device Descriptor", but this is kinda confusing
// since there's a bDescriptorType field that does not necessarily have
// to be DescriptorType_device.
// So we'll just refer to it as "USB Standard Descriptor" instead.

// X-Macro data-table of Arduino Leonardo's pin numbering,
// the corresponding ATmega32U4's data-direction register,
// and the port bit respectively.
//
// Pins 14-19 are analog pins, but they can still be nonetheless
// used as digital ones.
//
// * Arduino Leonardo Pinout Diagram @ Source(3) @ Page(1).
// * Analog as Digital Pins @ Source(1) @ Section(2.2.7) @ Page(6).
