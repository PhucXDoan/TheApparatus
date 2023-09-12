/*
	Source(1) @ ATmega32U4 Datasheet ("Atmel-7766J-USB-ATmega16U4/32U4-Datasheet_04/2016").
	Source(2) @ USB 2.0 Specification @ ("Universal Serial Bus Specification Revision 2.0", April 27, 2000).

	TODO Interrupt behavior on USB disconnection?
	TODO AVR-GCC is being an absolute pain and is complaining about `int x = {0};`. Disable the warning!
	TODO How does endianess work? Is doing shifts for u8s to make u16 the same as just interpreting as u16?
	TODO is __attribute__((packed)) necessary?
*/

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

enum USBEndpointSize // See: EPSIZE2:0 @ Source(1) @ Section(22.18.2) @ Page(287).
{
	USBEndpointSize_8   = 0b000,
	USBEndpointSize_16  = 0b001,
	USBEndpointSize_32  = 0b010,
	USBEndpointSize_64  = 0b011,
	USBEndpointSize_128 = 0b100,
	USBEndpointSize_256 = 0b101,
	USBEndpointSize_512 = 0b110,
};

enum USBStandardRequestCode // See: Source(2) @ Table(9-3) @ Page(251).
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

enum DescriptorType // See: Source(2) @ Table(9-5) @ Page(251).
{
	DescriptorType_device                    = 1,
	DescriptorType_configuration             = 2,
	DescriptorType_string                    = 3,
	DescriptorType_interface                 = 4,
	DescriptorType_endpoint                  = 5,
	DescriptorType_device_qualifier          = 6,
	DescriptorType_other_speed_configuration = 7,
	DescriptorType_interface_power           = 8,
};

// Note that the USB specification seems to call the descriptor
// the "USB Standard Device Descriptor", but this is kinda confusing
// since there's a bDescriptorType field that does not necessarily have
// to be DescriptorType_device.
// So we'll just refer to it as "USB Standard Descriptor" instead.
__attribute__((packed))
struct USBStandardDescriptor // Source(2) @ Table(9-8) @ Page(262-263).
{
	u8 bLength;            // Size of the descriptor which should always be 18 bytes.
	u8 bDescriptorType;    // Alias: enum DescriptorType.
	u8 bcdUSB[2];          // USB Specification version that's being complied with.
	u8 bDeviceClass;       // TODO document
	u8 bDeviceSubClass;    // TODO document
	u8 bDevceProtocol;     // TODO document
	u8 bMaxPacketSize0;    // Max packet size of endpoint 0.
	u8 idVendor[2];        // TODO document
	u8 idProduct[2];       // TODO document
	u8 bcdDevice[2];       // TODO document
	u8 iManufacturer;      // Zero-based index of string descriptor for the manufacturer description.
	u8 iProduct;           // Zero-based index of string descriptor for the product description.
	u8 iSerialNumber;      // Zero-based index of string descriptor for the device's serial number.
	u8 bNumConfigurations; // See: Source(2) @ Section(9.6.3) @ Page(264).
};

__attribute__((packed))
struct USBSetupPacket // See: struct USBSetupPacketUnknown.
{
	union
	{
		struct USBSetupPacketUnknown // See: Source(2) @ Table(9-2) @ Page(248).
		{
			u8 bmRequestType;
			u8 bRequest; // Alias: enum USBStandardRequestCode.
			u8 wValue[2];
			u8 wIndex[2];
			u8 wLength[2];
		} unknown;

		struct USBSetupPacketGetDescriptor // See: Source(2) @ Section(9.4.3) @ Page(253).
		{
			u8 bmRequestType;    // Should be 0b1000'0000.
			u8 bRequest;         // Should be USBStandardRequestCode_get_descriptor.
			u8 descriptor_index; // Low byte of wValue.
			u8 descriptor_type;  // High byte of wValue. Alias: enum DescriptorType.
			u8 language_id[2];
			u8 wLength[2];
		} get_descriptor;
	};
};

#define USB_ENDPOINT_0_SIZE USBEndpointSize_64

// TODO document
// TODO PROGMEMify
static const struct USBStandardDescriptor USB_DEVICE_DESCRIPTOR =
	{
		.bLength            = sizeof(struct USBStandardDescriptor),
		.bDescriptorType    = DescriptorType_device,
		.bcdUSB             = { 0x00, 0x02 },
		.bDeviceClass       = 0,   // TEMP
		.bDeviceSubClass    = 0,   // TEMP
		.bDevceProtocol     = 0,   // TEMP
		.bMaxPacketSize0    = 8 << USB_ENDPOINT_0_SIZE,
		.idVendor           = {0}, // TEMP
		.idProduct          = {0}, // TEMP
		.bcdDevice          = {0}, // TEMP
		.iManufacturer      = 0,   // TEMP
		.iProduct           = 0,   // TEMP
		.iSerialNumber      = 0,   // TEMP
		.bNumConfigurations = 1,
	};

