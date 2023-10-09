#define false            0
#define true             1
#define stringify_(X)    #X
#define stringify(X)     stringify_(X)
#define concat_(X, Y)    X##Y
#define concat(X, Y)     concat_(X, Y)
#define countof(...)     (sizeof(__VA_ARGS__)/sizeof((__VA_ARGS__)[0]))
#define bitsof(...)      (sizeof(__VA_ARGS__) * 8)
#define static_assert(X) _Static_assert((X), #X)

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

#if DEBUG
#define DEBUG_PIN_U8_START 2
#endif

#define PIN_HALT 12
enum PinHaltSource
{
	PinHaltSource_diplomat = 0,
	PinHaltSource_usb      = 1,
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
// FAT32.
//

/* TODO Note this.
	"SYSTEM~1"
		.padEnd(11, " ")
		.split("")
		.map(c => c.charCodeAt(0))
		.reduce((acc, x) => (((acc >> 1) | ((acc & 1) << 7)) + x) & 0xFF)
		.toString(16)
		.toUpperCase()
*/

#define FAT32_PARTITION_SECTOR_ADDRESS 2
#define FAT32_PARTITION_SECTOR_COUNT   16777216
#define FAT32_RESERVED_SECTOR_COUNT    2
#define FAT32_TABLE_SECTOR_COUNT       64
#define FAT32_SECTOR_SIZE              512
#define FAT32_SECTORS_PER_CLUSTER      64

#define FAT32_MEDIA_TYPE                        0xF8
#define FAT32_BACKUP_BOOT_SECTOR_OFFSET         6
#define FAT32_FILE_STRUCTURE_INFO_SECTOR_OFFSET 1
#define FAT32_ROOT_CLUSTER                      2

struct FAT32PartitionEntry // See: "Partition table entries" @ Source(16).
{
	u8  status;               // 0x80 if this partition is active, otherwise 0x00.
	u8  chs_address_begin[3]; // Irrelevant.
	u8  partition_type;       // Seems to be must 0x0C for FAT32s with logical block addressing. See: "Partition table entry" @ Source(17).
	u8  chs_address_end[3];   // Irrelevant.
	u32 abs_sector_address;   // Where the partition begins.
	u32 sector_count;         // Amount of sectors for this partition.
};

struct FAT32MasterBootRecord // See: "Structure of a modern standard MBR" @ Source(16).
{
	u8                         bootstrap_code_area_fst[218]; // Irrelevant.
	u8                         disk_timestamp[6];            // Irrelevant.
	u8                         bootstrap_code_area_snd[216]; // Irrelevant.
	u32                        disk_signature;               // Some unique identifier for the host to work with.
	u16                        zero;                         // Must be zero.
	struct FAT32PartitionEntry partitions[4];
	u16                        signature;                    // Must be 0xAA55.
};

struct FAT32BootSector // See: Source(15) @ Section(3.1) @ Page(7-9) & Source(15) @ Section(3.3) @ Page(11-12).
{
	u8   BS_jmpBoot[3];    // Best as { 0xEB, 0x00, 0x90 }.
	char BS_OEMName[8];    // Best as "MSWIN 4.1". See: "Boot Sector and BPB" @ Source(17).
	u16  BPB_BytsPerSec;   // Must be 512, 1024, 2048, or 4096 bytes per sector.
	u8   BPB_SecPerClus;   // Must be power of two and BPB_BytsPerSec * BPB_SecPerClus <= 32'768.
	u16  BPB_RsvdSecCnt;   // Must be non-zero.
	u8   BPB_NumFATs;      // Fine as 1.
	u16  BPB_RootEntCnt;   // Must be zero.
	u16  BPB_TotSec16;     // Must be zero.
	u8   BPB_Media;        // Best as 0xF8 for "fixed (non-removable) media"; must be also in lower 8-bits of the first FAT entry.
	u16  BPB_FATSz16;      // Must be zero.
	u16  BPB_SecPerTrk;    // Irrelevant.
	u16  BPB_NumHeads;     // Irrelevant.
	u32  BPB_HiddSec;      // Seems irrelevant; best as 0.
	u32  BPB_TotSec32;     // Must be non-zero.
	u32  BPB_FATSz32;      // Sectors per FAT.
	u16  BPB_ExtFlags;     // Seems irrelevant; best as 0.
	u16  BPB_FSVer;        // Must be zero.
	u32  BPB_RootClus;     // Cluster number of the first cluster of the root directory; best as 2.
	u16  BPB_FSInfo;       // Sector of the "FSINFO"; usually 1.
	u16  BPB_BkBootSec;    // Sector of the duplicated boot record; best as 6.
	u8   BPB_Reserved[12]; // Must be zero.
	u8   BS_DrvNum;        // Seems irrelevant; best as 0x80.
	u8   BS_Reserved;      // Must be zero.
	u8   BS_BootSig;       // Must be 0x29.
	u32  BS_VolID;         // Irrelevant.
	char BS_VolLab[11];    // Best as "NO NAME    ".
	char BS_FilSysType[8]; // Must be "FAT32   ".
	u8   BS_BootCode[420]; // Irrelevant.
	u16  BS_BootSign;      // Must be 0xAA55.
};

struct FAT32FileStructureInfo // See: Source(15) @ Page(21-22).
{
	u32 FSI_LeadSig;        // Must be 0x41615252.
	u8  FSI_Reserved1[480];
	u32 FSI_StrucSig;       // Must be 0x61417272.
	u32 FSI_Free_Count;     // Last known free cluster; best as 0xFFFFFFFF to signify unknown.
	u32 FSI_Nxt_Free;       // Hints the FAT driver of where to look for the next free cluster; best as 0xFFFFFFFF to signify no hint.
	u8  FSI_Reserved2[12];
	u32 FSI_TrailSig;       // Must be 0xAA550000.
};

enum FAT32DirEntryAttrFlag // See: Source(15) @ Section(6) @ Page(23).
{
	FAT32DirEntryAttrFlag_read_only = 0x01,
	FAT32DirEntryAttrFlag_hidden    = 0x02,
	FAT32DirEntryAttrFlag_system    = 0x04,
	FAT32DirEntryAttrFlag_volume_id = 0x08,
	FAT32DirEntryAttrFlag_directory = 0x10,
	FAT32DirEntryAttrFlag_archive   = 0x20,
};

#define FAT32_DIR_ENTRY_ATTR_FLAGS_LONG /* */ \
	( \
		FAT32DirEntryAttrFlag_read_only | \
		FAT32DirEntryAttrFlag_hidden | \
		FAT32DirEntryAttrFlag_system | \
		FAT32DirEntryAttrFlag_volume_id \
	)
#define FAT32_LAST_LONG_DIR_ENTRY 0x40 // See: Source(15) @ Section(7) @ Page(30).

union FAT32DirEntry
{
	struct FAT32DirEntryShort // See: Source(15) @ Section(6) @ Page(23).
	{
		char DIR_Name[11];     // 8 characters for the main name followed by 3 for the extension where both are right-padded with spaces if necessary.
		u8   DIR_Attr;         // Aliasing(enum FAT32DirEntryAttrFlag). Must not be FAT32_DIR_ENTRY_ATTR_FLAGS_LONG.
		u8   DIR_NTRes;        // Must be zero.
		u8   DIR_CrtTimeTenth; // Irrelevant.
		u16  DIR_CrtTime;      // Irrelevant.
		u16  DIR_CrtDate;      // Irrelevant.
		u16  DIR_LstAccDate;   // irrelevant.
		u16  DIR_FstClusHI;    // "High word of first data cluster number for file/directory described by this entry".
		u16  DIR_WrtTime;      // Irrelevant.
		u16  DIR_WrtDate;      // Irrelevant.
		u16  DIR_FstClusLO;    // "Low word of first data cluster number for file/directory described by this entry".
		u32  DIR_FileSize;     // Size of file in bytes; must be zero for directories. See: Source(15) @ Section(6.2) @ Page(26).
	} short_entry;

	// Note that: TODO Explain better.
	//     - Order of directory entries containing the long name are reversed.
	//     - If there's leftover space, entries are null-terminated and then padded with 0xFFFFs.
	//     - Characters are in UTF-16.
	struct FAT32DirEntryLong // See: Source(15) @ Section(7) @ Page(30).
	{
		u8  LDIR_Ord;       // The Nth long-entry. If the entry is the "last" (as in it provides the last part of the long name), then it is additionally OR'd with FAT32_LAST_LONG_DIR_ENTRY.
		u16 LDIR_Name1[5];  // First five UTF-16 characters that this long-entry provides for the long name.
		u8  LDIR_Attr;      // Must be FAT32_DIR_ENTRY_ATTR_FLAGS_LONG.
		u8  LDIR_Type;      // Must be zero.
		u8  LDIR_Chksum;    // "Checksum of name in the associated short name directory entry at the end of the long name directory entry set". TODO Not so simple...
		u16 LDIR_Name2[6];  // The next six UTF-16 characters that this long-entry provides for the long name.
		u16 LDIR_FstClusLO; // Must be zero.
		u16 LDIR_Name3[2];  // The last two UTF-16 characters that this long-entry provides for the long name.
	} long_entry;
};

static_assert(FAT32_SECTORS_PER_CLUSTER && !(FAT32_SECTORS_PER_CLUSTER & (FAT32_SECTORS_PER_CLUSTER - 1)));
static_assert(((u64) FAT32_SECTORS_PER_CLUSTER) * FAT32_SECTOR_SIZE <= (((u64) 1) << 15));
static_assert((FAT32_PARTITION_SECTOR_COUNT - FAT32_RESERVED_SECTOR_COUNT - FAT32_TABLE_SECTOR_COUNT) / FAT32_SECTORS_PER_CLUSTER >= 65'525);

static const struct FAT32MasterBootRecord FAT32_MASTER_BOOT_RECORD PROGMEM =
	{
		.disk_signature = 0x424F4F42,
		.partitions =
			{
				{
					.status             = 0x80,
					.partition_type     = 0x0C,
					.abs_sector_address = FAT32_PARTITION_SECTOR_ADDRESS,
					.sector_count       = FAT32_PARTITION_SECTOR_COUNT,
				},
			},
		.signature = 0xAA55,
	};

static const struct FAT32BootSector FAT32_BOOT_SECTOR PROGMEM =
	{
		.BS_jmpBoot     = { 0xEB, 0x00, 0x90 },
		.BS_OEMName     = "MSWIN4.1",
		.BPB_BytsPerSec = FAT32_SECTOR_SIZE,
		.BPB_SecPerClus = FAT32_SECTORS_PER_CLUSTER,
		.BPB_RsvdSecCnt = FAT32_RESERVED_SECTOR_COUNT,
		.BPB_NumFATs    = 1,
		.BPB_Media      = FAT32_MEDIA_TYPE,
		.BPB_TotSec32   = FAT32_PARTITION_SECTOR_COUNT,
		.BPB_FATSz32    = FAT32_TABLE_SECTOR_COUNT,
		.BPB_RootClus   = FAT32_ROOT_CLUSTER,
		.BPB_FSInfo     = FAT32_FILE_STRUCTURE_INFO_SECTOR_OFFSET,
		.BPB_BkBootSec  = FAT32_BACKUP_BOOT_SECTOR_OFFSET,
		.BS_DrvNum      = 0x80,
		.BS_BootSig     = 0x29,
		.BS_VolLab      = "NO NAME    ",
		.BS_FilSysType  = "FAT32   ",
		.BS_BootSign    = 0xAA55,
	};

static const struct FAT32FileStructureInfo FAT32_FILE_STRUCTURE_INFO PROGMEM =
	{
		.FSI_LeadSig    = 0x41615252,
		.FSI_StrucSig   = 0x61417272,
		.FSI_Free_Count = 0xFFFFFFFF,
		.FSI_Nxt_Free   = 0xFFFFFFFF,
		.FSI_TrailSig   = 0xAA550000,
	};

static const u32 FAT32_TABLE[128] PROGMEM = // Most significant nibbles are reserved. See: Source(15) @ Section(4) @ Page(16).
	{
		[0] = 0x0FFFFF'00 | FAT32_MEDIA_TYPE, // See: Source(15) @ Section(4.2) @ Page(19).
		[1] = 0xFFFFFFFF,                     // For format utilities. Seems to be commonly always all set. See: Source(15) @ Section(4.2) @ Page(19).

		[FAT32_ROOT_CLUSTER] = 0x0'FFFFFFF,
	};
static_assert(sizeof(FAT32_TABLE) == 512);

static const union FAT32DirEntry FAT32_ROOT_DIR_ENTRIES[16] PROGMEM =
	{
	};
static_assert(sizeof(FAT32_ROOT_DIR_ENTRIES) == 512);

#define FAT32_SECTOR_XMDT(X) \
	X(FAT32_MASTER_BOOT_RECORD , 0) \
	X(FAT32_BOOT_SECTOR        , FAT32_PARTITION_SECTOR_ADDRESS) \
	X(FAT32_BOOT_SECTOR        , FAT32_PARTITION_SECTOR_ADDRESS + FAT32_BACKUP_BOOT_SECTOR_OFFSET) \
	X(FAT32_FILE_STRUCTURE_INFO, FAT32_PARTITION_SECTOR_ADDRESS + FAT32_FILE_STRUCTURE_INFO_SECTOR_OFFSET) \
	X(FAT32_FILE_STRUCTURE_INFO, FAT32_PARTITION_SECTOR_ADDRESS + FAT32_FILE_STRUCTURE_INFO_SECTOR_OFFSET + FAT32_BACKUP_BOOT_SECTOR_OFFSET) \
	X(FAT32_TABLE              , FAT32_PARTITION_SECTOR_ADDRESS + FAT32_RESERVED_SECTOR_COUNT) \
	X(FAT32_ROOT_DIR_ENTRIES   , FAT32_PARTITION_SECTOR_ADDRESS + FAT32_RESERVED_SECTOR_COUNT + FAT32_TABLE_SECTOR_COUNT) \

//
// "usb.c"
//

#define PIN_USB_SPINLOCKING 11

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
	USBClass_null     = 0x00, // See: [About: USBClass_null].
	USBClass_cdc      = 0x02, // See: "Communication Class Device" @ Source(6) @ Section(1) @ AbsPage(11).
	USBClass_hid      = 0x03, // See: "Human-Interface Device" @ Source(7) @ Section(3) @ AbsPage(19).
	USBClass_ms       = 0x08, // See: "USB Mass Storage Device" @ Source(11) @ Section(1.1) @ AbsPage(1).
	USBClass_cdc_data = 0x0A, // See: "Data Interface Class" @ Source(6) @ Section(1) @ AbsPage(11).
	USBClass_misc     = 0XEF, // See: IAD @ Source(9) @ Table(1-1) @ AbsPage(5).
};

enum USBSetupRequestType // "bmRequestType" and "bRequest" bytes are combined. See: Source(2) @ Table(9-2) @ Page(248).
{
	#define MAKE(BM_REQUEST_TYPE, B_REQUEST) ((BM_REQUEST_TYPE) | (((u16) (B_REQUEST)) << 8))

	// Non-exhaustive. See: Standard Requests @ Source(2) @ Table(9-3) @ Page(250) & Source(2) @ Table(9-4) @ Page(251).
	USBSetupRequestType_get_desc    = MAKE(0b10000000, 6),
	USBSetupRequestType_set_address = MAKE(0b00000000, 5),
	USBSetupRequestType_set_config  = MAKE(0b00000000, 9),

	// Non-exhaustive. See: CDC-Specific Requests @ Source(6) @ Table(44) @ AbsPage(62-63) & Source(6) @ Table(46) @ AbsPage(64-65).
	USBSetupRequestType_cdc_get_line_coding        = MAKE(0b10100001, 0x21),
	USBSetupRequestType_cdc_set_line_coding        = MAKE(0b00100001, 0x20),
	USBSetupRequestType_cdc_set_control_line_state = MAKE(0b00100001, 0x22),

	// Non-exhaustive. See: HID-Specific Requests @ Source(7) @ Section(7.1) @ AbsPage(58-63).
	USBSetupRequestType_hid_get_desc = MAKE(0b10000001, 6),
	USBSetupRequestType_hid_set_idle = MAKE(0b00100001, 0x0A),

	// Non-exhuastive. See: MS-Specific Requests @ Source(12) @ Section(3) @ Page(7).
	USBSetupRequestType_ms_get_max_lun = MAKE(0b10100001, 0b11111110),

	#undef MAKE
};

struct USBSetupRequest // See: Source(2) @ Table(9-2) @ Page(248).
{
	u16 type; // Aliasing(enum USBSetupRequestType). The "bmRequestType" and "bRequest" bytes are combined.
	union
	{
		struct // See: Standard "GetDescriptor" @ Source(2) @ Section(9.4.3) @ Page(253).
		{
			u8  desc_index;       // Only for USBDescType_string or USBDescType_config. See: Source(2) @ Section(9.4.3) @ Page(253) & [USB Strings].
			u8  desc_type;        // Aliasing(enum USBDescType).
			u16 language_id;      // Only for USBDescType_string. See: Source(2) @ Section(9.4.3) @ Page(253) & [USB Strings].
			u16 requested_amount; // Amount of data the host expects back from the device.
		} get_desc;

		struct // See: Standard "SetAddress" @ // See: Source(2) @ Section(9.4.6) @ Page(256).
		{
			u16 address; // Must be within 7-bits.
		} set_address;

		struct // See: Standard "SetConfiguration" @ See: Source(2) @ Section(9.4.7) @ Page(257).
		{
			u16 value; // The configuration the host wants to set the device to. See: "bConfigurationValue" @ Source(2) @ Table(9-10) @ Page(265).
		} set_config;

		struct // See: Standard "Clear Feature" on Endpoints @ Source(2) @ Section(9.4.1) @ Page(252).
		{
			u16 feature_selector; // Must be zero.
			u16 endpoint_index;
		} endpoint_clear_feature;

		struct // See: CDC-Specific "SetLineCoding" @ Source(6) @ Setion(6.2.12) @ AbsPage(68-69).
		{
			u16 _zero;
			u16 designated_interface_index;           // Index of the interface that the host wants to set the line-coding for. Should be to our only CDC-interface.
			u16 incoming_line_coding_datapacket_size; // Amount of bytes the host will be sending. Should be sizeof(struct USBCDCLineCoding).
		} cdc_set_line_coding;

		struct // See: HID-Specific "GetDescriptor" @ Source(7) @ Section(7.1.1) @ AbsPage(59).
		{
			u8  desc_index;
			u8  desc_type;
			u16 interface_number;
			u16 requested_amount;
		} hid_get_desc;
	};
};

enum USBDescType
{
	// Non-exhaustive. See: Standard Descriptors @ Source(10) @ Table(9-5) @ Page(315).
	USBDescType_device                = 1,
	USBDescType_config                = 2,
	USBDescType_string                = 3, // See: [USB Strings].
	USBDescType_interface             = 4,
	USBDescType_endpoint              = 5,
	USBDescType_device_qualifier      = 6,
	USBDescType_interface_association = 11,

	// See: CDC-Specific Descriptors @ Source(6) @ Table(24) @ AbsPage(44).
	USBDescType_cdc_interface = 0x24,
	USBDescType_cdc_endpoint  = 0x25,

	// See: HID-Specific Descriptors @ Source(7) @ Section(7.1) @ AbsPage(59).
	USBDescType_hid          = 0x21,
	USBDescType_hid_report   = 0x22,
	USBDescType_hid_physical = 0x23,
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
	u8  bNumConfigurations; // Amount of configurations the device can be set to. Only a single configuration (USB_CONFIG_HIERARCHY) is defined, so this is 1.
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
	u8 bAlternateSetting;  // The nth alternative interface of the one at index bInterfaceNumber; 0 is the default interface that'll be used by the device.
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
	u8 bDataInterface;     // Index of CDC-Data interface to receive call-management command. Seems irrelevant for functionality.
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

struct USBDescHID // See: Source(7) @ Section(6.2.1) @ AbsPage(32).
{
	u8  bLength;                  // Must be sizeof(struct USBDescHID).
	u8  bDescriptorType;          // Must be USBDescType_hid.
	u16 bcdHID;                   // HID specification version that's being complied with.
	u8  bCountryCode;             // Irrelevant for functionality.
	u8  bNumDescriptors;          // Must be countof(subordinate_descriptors).
	struct                        // Descriptors that'd belong to this HID descriptor. See: Source(7) @ Section(5.1) @ AbsPage(22).
	{
		u8  bDescriptorType;      // Alias: enum USBDescType.
		u16 wDescriptorLength;    // Must be the amount of bytes of corresponding descriptor.
	} subordinate_descriptors[1]; // The amount of descriptor-type and descriptor-length pairs could vary here, but 1 is sufficient for our application.
};

struct USBDescIAD // See: Source(10) @ Table(9-16) @ Page(336).
{
	u8 bLength;           // Must be sizeof(struct USBDescIAD).
	u8 bDescriptorType;   // Must be USBDescType_interface_association.
	u8 bFirstInterface;
	u8 bInterfaceCount;
	u8 bFunctionClass;
	u8 bFunctionSubClass;
	u8 bFunctionProtocol;
	u8 iFunction;         // See: [USB Strings].
};

struct USBCDCLineCoding // See: Source(6) @ Table(50) @ AbsPage(69).
{
	u32 dwDTERate;   // Baud-rate.
	u8  bCharFormat; // Describes amount of stop-bits.
	u8  bParityType; // Describes mode of parity-bits.
	u8  bDataBits;   // Describes amount of data-bits.
};

enum USBHIDItem // Short-items with hardcoded amount of data bytes. See: Source(7) @ Section(6.2.2.2) @ AbsPage(36).
{
	// Non-exhaustive. See: "Main Items" @ Source(7) @ Section(6.2.2.4) @ AbsPage(38).
	USBHIDItem_main_input            = 0b1000'00'01,
	USBHIDItem_main_begin_collection = 0b1010'00'01,
	USBHIDItem_main_end_collection   = 0b1100'00'00,

	// Non-exhaustive. See: "Global Items" @ Source(7) @ Section(6.2.2.7) @ AbsPage(45).
	USBHIDItem_global_usage_page   = 0b0000'01'01,
	USBHIDItem_global_logical_min  = 0b0001'01'01,
	USBHIDItem_global_logical_max  = 0b0010'01'01,
	USBHIDItem_global_report_size  = 0b0111'01'01, // In terms of bits.
	USBHIDItem_global_report_count = 0b1001'01'01,

	// Non-exhaustive. See: "Local Items" @ Source(7) @ Section(6.2.2.8) @ AbsPage(50).
	USBHIDItem_local_usage     = 0b0000'10'01,
	USBHIDItem_local_usage_min = 0b0001'10'01,
	USBHIDItem_local_usage_max = 0b0010'10'01,
};

enum USBHIDItemMainInputFlag // Non-exhaustive. See: Source(7) @ Section(6.2.2.4) @ AbsPage(38) & Source(7) @ Section(6.2.2.5) @ AbsPage(40-41).
{
	USBHIDItemMainInputFlag_padding  = ((u16) 1) << 0, // Also called "constant", but it's essentially used for padding.
	USBHIDItemMainInputFlag_variable = ((u16) 1) << 1, // A buffer that can be filled up with data as opposed to it just being a simple bitmap or integer.
	USBHIDItemMainInputFlag_relative = ((u16) 1) << 2, // Data is a delta.
};

enum USBHIDItemMainCollectionType // Non-exhaustive. See: Source(7) @ Section(6.2.2.4) @ AbsPage(38) & Source(7) @ Section(6.2.2.6) @ AbsPage(42-43).
{
	USBHIDItemMainCollectionType_physical    = 0x00, // Groups report data together to represent a measurement at one "geometric point".
	USBHIDItemMainCollectionType_application = 0x01, // Packages up all the report layouts together to represent one single-purpose device, e.g. mouse.
};

enum USBHIDUsagePage // Non-exhaustive. See: Source(8) @ Section(3) @ AbsPage(17).
{
	USBHIDUsagePage_generic_desktop = 0x01, // Category of common computer peripherals such as mice, keyboard, joysticks, etc. See: enum USBHIDUsageGenericDesktop.
	USBHIDUsagePage_button          = 0x09, // Details how buttons are defined. See: Source(8) @ Section(12) @ AbsPage(110).
};

enum USBHIDUsageGenericDesktop // Non-exhaustive. See: Source(8) @ Section(4) @ AbsPage(32).
{
	USBHIDUsageGenericDesktop_pointer = 0x01, // "A collection of axes that generates a value to direct, indicate, or point user intentions to an application."
	USBHIDUsageGenericDesktop_mouse   = 0x02, // A peripheral that has some buttons and indicates 2D movement; self-explainatory.
	USBHIDUsageGenericDesktop_x       = 0x30, // Linear translation from left-to-right.
	USBHIDUsageGenericDesktop_y       = 0x31, // Linear translation from far-to-near, so in the context of a mouse, top-down.
};

#define USB_MS_COMMAND_BLOCK_WRAPPER_SIGNATURE 0x43425355
struct USBMSCommandBlockWrapper // See: Source(12) @ Table(5.1) @ Page(13).
{
	u32 dCBWSignature;          // Must be USB_MS_COMMAND_BLOCK_WRAPPER_SIGNATURE.
	u32 dCBWTag;                // Value copied for the command status wrapper.
	u32 dCBWDataTransferLength; // Amount of bytes transferred from host to device or device to host.
	u8  bmCBWFlags;             // If the MSb is set, data transfer is from device to host. Any other bit must be cleared.
	u8  bCBWLUN;                // "Logical Unit Number" the command is designated for; high-nibble must be cleared.
	u8  bCBWCBLength;           // Amount of bytes defined in CBWCB; must be within [1, 16] and high 3 bits be cleared.
	u8  CBWCB[16];              // Bytes of SCSI command descriptor block. See: Source(13).
};

#define USB_MS_COMMAND_STATUS_WRAPPER_SIGNATURE 0x53425355
struct USBMSCommandStatusWrapper
{
	u32 dCSWSignature;
	u32 dCSWTag;
	u32 dCSWDataResidue;
	u8  bCSWStatus;
};

enum USBMSSCSIOpcode // See: Source(13) @ Section(3) @ Page(65) & Source(14).
{
	USBMSSCSIOpcode_test_unit_ready        = 0x00,
	USBMSSCSIOpcode_inquiry                = 0x12,
	USBMSSCSIOpcode_mode_sense             = 0x1A,
	USBMSSCSIOpcode_read_format_capacities = 0x23, // See: Source(14).
	USBMSSCSIOpcode_read_capacity          = 0x25,
	USBMSSCSIOpcode_read                   = 0x28,
};

enum USBMSState
{
	USBMSState_ready_for_command,
	USBMSState_sending_data,
	USBMSState_ready_for_status,
};

// Endpoint buffer sizes must be one of the names of enum USBEndpointSizeCode.
// The maximum capacity between endpoints also differ. See: Source(1) @ Section(22.1) @ Page(270).

#define USB_ENDPOINT_DFLT                  0                               // "Default Endpoint" is synonymous with endpoint 0.
#define USB_ENDPOINT_DFLT_TRANSFER_TYPE    USBEndpointTransferType_control // The default endpoint is always a control-typed endpoint.
#define USB_ENDPOINT_DFLT_TRANSFER_DIR     0                               // See: [ATmega32U4's Configuration of Endpoint 0].
#define USB_ENDPOINT_DFLT_SIZE             8

#define USB_ENDPOINT_CDC_IN                2
#define USB_ENDPOINT_CDC_IN_TRANSFER_TYPE  USBEndpointTransferType_bulk
#define USB_ENDPOINT_CDC_IN_TRANSFER_DIR   USBEndpointAddressFlag_in
#define USB_ENDPOINT_CDC_IN_SIZE           64

#define USB_ENDPOINT_CDC_OUT               3
#define USB_ENDPOINT_CDC_OUT_TRANSFER_TYPE USBEndpointTransferType_bulk
#define USB_ENDPOINT_CDC_OUT_TRANSFER_DIR  0
#define USB_ENDPOINT_CDC_OUT_SIZE          64

#define USB_ENDPOINT_HID               4
#define USB_ENDPOINT_HID_TRANSFER_TYPE USBEndpointTransferType_interrupt
#define USB_ENDPOINT_HID_TRANSFER_DIR  USBEndpointAddressFlag_in
#define USB_ENDPOINT_HID_SIZE          8

#define USB_ENDPOINT_MS_IN               5
#define USB_ENDPOINT_MS_IN_TRANSFER_TYPE USBEndpointTransferType_bulk
#define USB_ENDPOINT_MS_IN_TRANSFER_DIR  USBEndpointAddressFlag_in
#define USB_ENDPOINT_MS_IN_SIZE          64

#define USB_ENDPOINT_MS_OUT               6
#define USB_ENDPOINT_MS_OUT_TRANSFER_TYPE USBEndpointTransferType_bulk
#define USB_ENDPOINT_MS_OUT_TRANSFER_DIR  0
#define USB_ENDPOINT_MS_OUT_SIZE          64

static const u8 USB_ENDPOINT_UECFGNX[][2] PROGMEM = // UECFG0X and UECFG1X that an endpoint will be configured with.
	{
		#define MAKE(ENDPOINT_NAME) \
			[USB_ENDPOINT_##ENDPOINT_NAME] = \
				{ \
					(USB_ENDPOINT_##ENDPOINT_NAME##_TRANSFER_TYPE << EPTYPE0) | ((!!USB_ENDPOINT_##ENDPOINT_NAME##_TRANSFER_DIR) << EPDIR), \
					(concat(USBEndpointSizeCode_, USB_ENDPOINT_##ENDPOINT_NAME##_SIZE) << EPSIZE0) | (1 << ALLOC), \
				},
		MAKE(DFLT)
		MAKE(CDC_IN )
		MAKE(CDC_OUT)
		MAKE(HID)
		MAKE(MS_IN )
		MAKE(MS_OUT)
		#undef MAKE
	};

struct USBConfigHierarchy // This layout is defined uniquely for our device application.
{
	struct USBDescConfig config;

	struct USBDescIAD iad; // Must be placed here to group the CDC and CDC-Data interfaces together. See: Source(9) @ Figure(2-1) @ AbsPage(6).

	#define USB_CDC_INTERFACE_INDEX 0
	struct
	{
		struct USBDescInterface         desc;
		struct USBDescCDCHeader         cdc_header;
		struct USBDescCDCCallManagement cdc_call_management;
		struct USBDescCDCACMManagement  cdc_acm_management;
		struct USBDescCDCUnion          cdc_union;
	} cdc;

	#define USB_CDC_DATA_INTERFACE_INDEX 1
	struct
	{
		struct USBDescInterface desc;
		struct USBDescEndpoint  endpoints[2];
	} cdc_data;

	#define USB_HID_INTERFACE_INDEX 2
	struct
	{
		struct USBDescInterface desc;
		struct USBDescHID       hid;
		struct USBDescEndpoint  endpoints[1];
	} hid;

	#define USB_MS_INTERFACE_INDEX 3
	struct
	{
		struct USBDescInterface desc;
		struct USBDescEndpoint  endpoints[2];
	} ms;

	#define USB_INTERFACE_COUNT 4
};

static const u8 USB_DESC_HID_REPORT[] PROGMEM =
	{
		USBHIDItem_global_usage_page,     USBHIDUsagePage_generic_desktop,
		USBHIDItem_local_usage,           USBHIDUsageGenericDesktop_mouse,
		USBHIDItem_main_begin_collection, USBHIDItemMainCollectionType_application,

			USBHIDItem_local_usage,           USBHIDUsageGenericDesktop_pointer,
			USBHIDItem_main_begin_collection, USBHIDItemMainCollectionType_physical,

				// LSb for primary mouse button being held.
				USBHIDItem_global_usage_page,   USBHIDUsagePage_button,
				USBHIDItem_local_usage_min,     1,
				USBHIDItem_local_usage_max,     1,
				USBHIDItem_global_logical_min,  0,
				USBHIDItem_global_logical_max,  1,
				USBHIDItem_global_report_size,  1,
				USBHIDItem_global_report_count, 1,
				USBHIDItem_main_input,          USBHIDItemMainInputFlag_variable,

				// Padding bits.
				USBHIDItem_global_report_size,  7,
				USBHIDItem_global_report_count, 1,
				USBHIDItem_main_input,          USBHIDItemMainInputFlag_padding,

				// Two signed bytes for delta-x and delta-y respectively.
				USBHIDItem_global_usage_page,   USBHIDUsagePage_generic_desktop,
				USBHIDItem_local_usage,         USBHIDUsageGenericDesktop_x,
				USBHIDItem_local_usage,         USBHIDUsageGenericDesktop_y,
				USBHIDItem_global_logical_min,  -128,
				USBHIDItem_global_logical_max,  127,
				USBHIDItem_global_report_size,  8,
				USBHIDItem_global_report_count, 2,
				USBHIDItem_main_input,          USBHIDItemMainInputFlag_variable | USBHIDItemMainInputFlag_relative,

			USBHIDItem_main_end_collection,

		USBHIDItem_main_end_collection,
	};

static const u8 USB_MS_SCSI_INQUIRY_DATA[] = // See: Source(13) @ Section(3.6.2) @ Page(94).
	{
		// "PERIPHERAL QUALIFIER"         : We support the following specified peripheral device type. See: Source(13) @ Table(60) @ Page(95).
		//     | "PERIPHERAL DEVICE TYPE" : The device is a "direct access block device", e.g. SD cards, flashdrives, etc. See: Source(13) @ Table(61) @ Page(96).
		//     |    |
		//    vvv vvvvv
			0b000'00000,

		// "RMB" : Is the medium removable? See: Source(13) @ Section(3.6.2) @ Page(96).
		//    | Reserved.
		//    |    |
		//    v vvvvvvv
			0b1'0000000, // TODO What does "removable" mean?

		// TODO What??
		// "VERSION" : Indicate which SPC standard is being used. See: Source(13) @ Table(62) @ Page(97).
			0x02,

		// Obsolete.
		//    | "NORMACA"                   : TODO We don't support "ACA" or something... See: Source(13) @ Section(3.6.2) @ Page(97).
		//    |  | "HISUP"                  : Is a hierarchy used to organize logical units? See: Source(13) @ Section(3.6.2) @ Page(97).
		//    |  | | "RESPONSE DATA FORMAT" : Pretty much has to be 2. See: Source(13) @ Section(3.6.2) @ Page(97).
		//    |  | | |
		//    vv v v vvvv
			0b00'0'0'0010,

		// "ADDITIONAL LENGTH" : Amount of bytes remaining after this byte. The inquiry data has to be at least 36, so 36 - 5. See: Source(13) @ Section(3.6.2) @ Page(94).
			0x1F,

		// "SCCS"                  : Does the device have a "embedded storage array controller component"? See: Source(13) @ Section(3.6.2) @ Page(97).
		//    | "ACC"              : Can the "access controls coordinator" be addressed at this logical unit? See: Source(13) @ Section(3.6.2) @ Page(98).
		//    | | "TPGS"           : The types of "asymmetric logical unit access" that can be done. See: Source(13) @ Table(63) @ Page(98).
		//    | | | "3PC"          : "Third-party copy commands" supported? See: Source(13) @ Section(3.6.2) @ Page(98).
		//    | | |  | Reserved.
		//    | | |  | | "PROTECT" : Information protection supported? See: Source(13) @ Section(3.6.2) @ Page(98).
		//    | | |  | |  |
		//    v v vv v vv v
			0b0'0'00'0'00'0,

		// Obsolete.
		//    | "ENCSERV"          : Does the device have "embedded enclosure services component"? See: Source(13) @ Section(3.6.2) @ Page(98).
		//    | | Vendor-Specific.
		//    | | | "MULTIP"       : Does the device support multiple ports? See: Source(13) @ Section(3.6.2) @ Page(98).
		//    | | | |  Obsolete.
		//    | | | |  |
		//    v v v v vvvv
			0b0'0'0'0'0000,

		// Obsolete.
		//      | "CMDQUE" : TODO The hell is a BQUE bit? See: Source(13) @ Section(3.6.2) @ Page(99).
		//      |    | Vendor-Specific.
		//      |    | |
		//    vvvvvv v v
			0b000000'0'0,

		// "T10 VENDOR IDENTIFICATION" : TODO WTF is this? See: Source(13) @ Section(3.6.2) @ Page(99).
			'G', 'e', 'n', 'e', 'r', 'a', 'l', ' ',

		// "PRODUCT IDENTIFICATION" : TODO WTF! See: Source(13) @ Section(3.6.2) @ Page(99).
			'U', 'D', 'i', 's', 'k', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',

		// "PRODUCT REVISION LEVEL" : TODO WTF! See: Source(13) @ Section(3.6.2) @ Page(99).
			'5', '.', '0', '0',
	};

static const u8 USB_MS_SCSI_READ_FORMAT_CAPACITIES_DATA[] = // See: Source(14) @ Table(702) @ AbsPage(1).
	{
		// "Capacity List Header". See: Source(14) @ Table(703) @ AbsPage(2).
		//
		// Reserved.
		//     | "Capacity List Length" : Amount of bytes after this byte.
		//     |     |
		//  vvvvvvv  v
			0, 0, 0, 8,

		// "Current/Maximum Capacity Descriptor". See: Source(14) @ Table(704) @ AbsPage(2).
		//
		// "Number of Blocks"                       : Number of addressable blocks, which is written in big-endian.
		//        |        Reserved.
		//        |            | "Descriptor Type"  : The type of the media the device has. See: Source(14) @ Table(705) @ AbsPage(2).
		//        |            |   | "Block Length" : Amount of bytes of each addressable block, which is written in big-endian.
		//        |            |   |      |
		//  vvvvvvvvvvvv    vvvvvv vv  vvvvvvv
			0, 240, 0, 0, 0b000000'10, 0, 2, 0 // TODO Update with FAT32
	};

static const u8 USB_MS_SCSI_READ_CAPACITY_DATA[] = // See: Source(13) @ Table(120) @ Page(156).
	{
		// "RETURNED LOGICAL BLOCK ADDRESS" in big-endian. TODO Is this the largest block address?
			0, 239, 255, 255,

		// "BLOCK LENGTH IN BYTES" in big-endian.
			((u32) FAT32_SECTOR_SIZE >> 24) & 0xFF,
			((u32) FAT32_SECTOR_SIZE >> 16) & 0xFF,
			((u32) FAT32_SECTOR_SIZE >>  8) & 0xFF,
			((u32) FAT32_SECTOR_SIZE >>  0) & 0xFF,
	};

static const u8 USB_MS_SCSI_MODE_SENSE[] = { 0x0B, 0x00, 0x00, 0x08, 0x00, 0x03, 0xE3, 0x11, 0x00, 0x00, 0x08, 0x00 };

static const struct USBDescDevice USB_DESC_DEVICE PROGMEM =
	{
		.bLength            = sizeof(struct USBDescDevice),
		.bDescriptorType    = USBDescType_device,
		.bcdUSB             = 0x0200,        // We are still pretty much USB 2.0 despite using IADs.
		.bDeviceClass       = USBClass_misc, // This gives the host a heads up to load drivers for our multi-function device. See: Source(9) @ Section(1) @ AbsPage(5).
		.bDeviceSubClass    = 0x02,          // See: "bDeviceSubClass" @ Source(9) @ Table(1-1) @ AbsPage(5).
		.bDeviceProtocol    = 0x01,          // See: "bDeviceProtocol" @ Source(9) @ Table(1-1) @ AbsPage(5).
		.bMaxPacketSize0    = USB_ENDPOINT_DFLT_SIZE,
		.idVendor           = 0, // Seems irrelevant for functionality.
		.idProduct          = 0, // Seems irrelevant for functionality.
		.bcdDevice          = 0, // Seems irrelevant for functionality.
		.bNumConfigurations = 1
	};

#define USB_CONFIG_HIERARCHY_ID 1 // Must be non-zero. See: Soure(2) @ Figure(11-10) @ Page(310).
static const struct USBConfigHierarchy USB_CONFIG_HIERARCHY PROGMEM =
	{
		.config =
			{
				.bLength             = sizeof(struct USBDescConfig),
				.bDescriptorType     = USBDescType_config,
				.wTotalLength        = sizeof(struct USBConfigHierarchy),
				.bNumInterfaces      = USB_INTERFACE_COUNT,
				.bConfigurationValue = USB_CONFIG_HIERARCHY_ID,
				.bmAttributes        = USBConfigAttrFlag_reserved_one | USBConfigAttrFlag_self_powered, // TODO We should calculate our power consumption!
				.bMaxPower           = 50,                                                              // TODO We should calculate our power consumption!
			},
		.iad =
			{
				.bLength           = sizeof(struct USBDescIAD),
				.bDescriptorType   = USBDescType_interface_association,
				.bFirstInterface   = USB_CDC_INTERFACE_INDEX,
				.bInterfaceCount   = 2,
				.bFunctionClass    = USBClass_cdc,
				.bFunctionSubClass = 0x2, // This field will act like "bDeviceSubClass". See: "Abstract Control Model" @ Source(6) @ Table(16) @ AbsPage(39).
				.bFunctionProtocol = 0,   // This field will act like "bDeviceProtocol". Seems irrelevant for functionality. See: Source(6) @ Table(17) @ AbsPage(40).
			},
		.cdc =
			{
				.desc =
					{
						.bLength            = sizeof(struct USBDescInterface),
						.bDescriptorType    = USBDescType_interface,
						.bInterfaceNumber   = USB_CDC_INTERFACE_INDEX,
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
						.bInterfaceNumber   = USB_CDC_DATA_INTERFACE_INDEX,
						.bAlternateSetting  = 0,
						.bNumEndpoints      = countof(USB_CONFIG_HIERARCHY.cdc_data.endpoints),
						.bInterfaceClass    = USBClass_cdc_data, // See: Source(6) @ Section(4.5) @ AbsPage(40).
						.bInterfaceSubClass = 0,                 // Should be left alone. See: Source(6) @ Section(4.6) @ AbsPage(40).
						.bInterfaceProtocol = 0,                 // Seems irrelevant for functionality. See: Source(6) @ Table(19) @ AbsPage(40-41).
					},
				.endpoints =
					{
						{
							.bLength          = sizeof(struct USBDescEndpoint),
							.bDescriptorType  = USBDescType_endpoint,
							.bEndpointAddress = USB_ENDPOINT_CDC_IN | USB_ENDPOINT_CDC_IN_TRANSFER_DIR,
							.bmAttributes     = USB_ENDPOINT_CDC_IN_TRANSFER_TYPE,
							.wMaxPacketSize   = USB_ENDPOINT_CDC_IN_SIZE,
							.bInterval        = 0,
						},
						{
							.bLength          = sizeof(struct USBDescEndpoint),
							.bDescriptorType  = USBDescType_endpoint,
							.bEndpointAddress = USB_ENDPOINT_CDC_OUT | USB_ENDPOINT_CDC_OUT_TRANSFER_DIR,
							.bmAttributes     = USB_ENDPOINT_CDC_OUT_TRANSFER_TYPE,
							.wMaxPacketSize   = USB_ENDPOINT_CDC_OUT_SIZE,
							.bInterval        = 0,
						},
					}
			},
		.hid =
			{
				.desc =
					{
						.bLength            = sizeof(struct USBDescInterface),
						.bDescriptorType    = USBDescType_interface,
						.bInterfaceNumber   = USB_HID_INTERFACE_INDEX,
						.bAlternateSetting  = 0,
						.bNumEndpoints      = countof(USB_CONFIG_HIERARCHY.hid.endpoints),
						.bInterfaceClass    = USBClass_hid,
						.bInterfaceSubClass = 0, // Set to 1 if we support a boot interface, which we don't need to. See: Source(7) @ Section(4.2) @ AbsPage(18).
						.bInterfaceProtocol = 0, // Since we don't support a boot interface, this is also 0. See: Source(7) @ Section(4.3) @ AbsPage(19).
					},
				.hid =
					{
						.bLength                 = sizeof(struct USBDescHID),
						.bDescriptorType         = USBDescType_hid,
						.bcdHID                  = 0x0111,
						.bNumDescriptors         = countof(USB_CONFIG_HIERARCHY.hid.hid.subordinate_descriptors),
						.subordinate_descriptors =
							{
								{
									.bDescriptorType   = USBDescType_hid_report,
									.wDescriptorLength = sizeof(USB_DESC_HID_REPORT),
								},
							}
					},
				.endpoints =
					{
						{
							.bLength          = sizeof(struct USBDescEndpoint),
							.bDescriptorType  = USBDescType_endpoint,
							.bEndpointAddress = USB_ENDPOINT_HID | USB_ENDPOINT_HID_TRANSFER_DIR,
							.bmAttributes     = USB_ENDPOINT_HID_TRANSFER_TYPE,
							.wMaxPacketSize   = USB_ENDPOINT_HID_SIZE,
							.bInterval        = 1,
						}
					},
			},
		.ms =
			{
				.desc =
					{
						.bLength            = sizeof(struct USBDescInterface),
						.bDescriptorType    = USBDescType_interface,
						.bInterfaceNumber   = USB_MS_INTERFACE_INDEX,
						.bAlternateSetting  = 0,
						.bNumEndpoints      = countof(USB_CONFIG_HIERARCHY.ms.endpoints),
						.bInterfaceClass    = USBClass_ms,
						.bInterfaceSubClass = 0x06, // See: "SCSI Transparent Command Set" @ Source(11) @ AbsPage(3).
						.bInterfaceProtocol = 0x50, // See: "Bulk-Only Transport" @ Source(12) @ Page(11).
					},
				.endpoints =
					{
						{
							.bLength          = sizeof(struct USBDescEndpoint),
							.bDescriptorType  = USBDescType_endpoint,
							.bEndpointAddress = USB_ENDPOINT_MS_IN | USB_ENDPOINT_MS_IN_TRANSFER_DIR,
							.bmAttributes     = USB_ENDPOINT_MS_IN_TRANSFER_TYPE,
							.wMaxPacketSize   = USB_ENDPOINT_MS_IN_SIZE,
							.bInterval        = 0,
						},
						{
							.bLength          = sizeof(struct USBDescEndpoint),
							.bDescriptorType  = USBDescType_endpoint,
							.bEndpointAddress = USB_ENDPOINT_MS_OUT | USB_ENDPOINT_MS_OUT_TRANSFER_DIR,
							.bmAttributes     = USB_ENDPOINT_MS_OUT_TRANSFER_TYPE,
							.wMaxPacketSize   = USB_ENDPOINT_MS_OUT_SIZE,
							.bInterval        = 0,
						}
					},
			},
	};

#if DEBUG
	static volatile u8 debug_usb_cdc_in_buffer [USB_ENDPOINT_CDC_IN_SIZE ] = {0};
	static volatile u8 debug_usb_cdc_out_buffer[USB_ENDPOINT_CDC_OUT_SIZE] = {0};

	static volatile u8 debug_usb_cdc_in_writer  = 0; // Main program writes the IN-buffer.
	static volatile u8 debug_usb_cdc_in_reader  = 0; // Interrupt routine reads the IN-buffer.
	static volatile u8 debug_usb_cdc_out_writer = 0; // Interrupt routine writes the OUT-buffer.
	static volatile u8 debug_usb_cdc_out_reader = 0; // Main program reads the OUT-buffer.

	#define debug_usb_cdc_in_writer_masked(OFFSET)  ((debug_usb_cdc_in_writer  + (OFFSET)) & (countof(debug_usb_cdc_in_buffer ) - 1))
	#define debug_usb_cdc_in_reader_masked(OFFSET)  ((debug_usb_cdc_in_reader  + (OFFSET)) & (countof(debug_usb_cdc_in_buffer ) - 1))
	#define debug_usb_cdc_out_writer_masked(OFFSET) ((debug_usb_cdc_out_writer + (OFFSET)) & (countof(debug_usb_cdc_out_buffer) - 1))
	#define debug_usb_cdc_out_reader_masked(OFFSET) ((debug_usb_cdc_out_reader + (OFFSET)) & (countof(debug_usb_cdc_out_buffer) - 1))

	// A read/write index with a size greater than a byte makes "atomic" read/write operations difficult to guarantee; it can be done, but probably not worthwhile.
	static_assert(sizeof(debug_usb_cdc_in_writer) == 1 && sizeof(debug_usb_cdc_in_reader) == 1 && sizeof(debug_usb_cdc_out_writer) == 1 && sizeof(debug_usb_cdc_out_reader) == 1);

	// The read/write indices must be able to address any element in the corresponding buffer.
	static_assert(countof(debug_usb_cdc_in_buffer ) < (((u64) 1) << bitsof(debug_usb_cdc_in_reader )));
	static_assert(countof(debug_usb_cdc_in_buffer ) < (((u64) 1) << bitsof(debug_usb_cdc_in_writer )));
	static_assert(countof(debug_usb_cdc_out_buffer) < (((u64) 1) << bitsof(debug_usb_cdc_out_reader)));
	static_assert(countof(debug_usb_cdc_out_buffer) < (((u64) 1) << bitsof(debug_usb_cdc_out_writer)));

	// Buffer sizes must be a power of two for the "debug_usb_cdc_X_Y_masked" macros.
	static_assert(countof(debug_usb_cdc_in_buffer ) && !(countof(debug_usb_cdc_in_buffer ) & (countof(debug_usb_cdc_in_buffer ) - 1)));
	static_assert(countof(debug_usb_cdc_out_buffer) && !(countof(debug_usb_cdc_out_buffer) & (countof(debug_usb_cdc_out_buffer) - 1)));

	static volatile b8 debug_usb_diagnostic_signal_received = false;
#endif

#define USB_MOUSE_CALIBRATIONS_REQUIRED 128

// Only the interrupt can read and write these.
static u8 _usb_mouse_calibrations = 0;
static u8 _usb_mouse_curr_x       = 0; // Origin is top-left.
static u8 _usb_mouse_curr_y       = 0; // Origin is top-left.
static b8 _usb_mouse_held         = false;

static volatile u16 _usb_mouse_command_buffer[8] = {0}; // See: [USB Mouse Commands].
static volatile u8  _usb_mouse_command_writer    = 0;   // Main program writes the buffer.
static volatile u8  _usb_mouse_command_reader    = 0;   // Interrupt reads the buffer.

#define _usb_mouse_command_writer_masked(OFFSET) ((_usb_mouse_command_writer + (OFFSET)) & (countof(_usb_mouse_command_buffer) - 1))
#define _usb_mouse_command_reader_masked(OFFSET) ((_usb_mouse_command_reader + (OFFSET)) & (countof(_usb_mouse_command_buffer) - 1))

// A read/write index with a size greater than a byte makes "atomic" read/write operations difficult to guarantee; it can be done, but probably not worthwhile.
static_assert(sizeof(_usb_mouse_command_writer) == 1 && sizeof(_usb_mouse_command_reader) == 1);

// The read/write indices must be able to address any element in the corresponding buffer.
static_assert(countof(_usb_mouse_command_buffer) < (((u64) 1) << bitsof(_usb_mouse_command_reader)));
static_assert(countof(_usb_mouse_command_buffer) < (((u64) 1) << bitsof(_usb_mouse_command_writer)));

// Buffer sizes must be a power of two for the "_usb_mouse_X_masked" macros.
static_assert(countof(_usb_mouse_command_buffer) && !(countof(_usb_mouse_command_buffer) & (countof(_usb_mouse_command_buffer) - 1)));

static enum USBMSState                  _usb_ms_state                         = USBMSState_ready_for_command;
static const u8*                        _usb_ms_scsi_info_data                = 0;
static u8                               _usb_ms_scsi_info_size                = 0;
static u32                              _usb_ms_abs_sector_address            = 0;
static u32                              _usb_ms_sectors_left_to_send          = 0;
static u8                               _usb_ms_sending_sector_fragment_index = 0;
static struct USBMSCommandStatusWrapper _usb_ms_status                        = {0};

//
// Documentation.
//

/*
	source(1)  := ATmega32U4 Datasheet ("Atmel-7766J-USB-ATmega16U4/32U4-Datasheet_04/2016").
	Source(2)  := USB 2.0 Specification (Dated: April 27, 2000).
	Source(3)  := Arduino Leonardo Pinout Diagram (STORE.ARDUINO.CC/LEONARDO) (Dated: 17/06/2020).
	Source(4)  := USB in a NutShell by BeyondLogic (Accessed: September 19, 2023).
	Source(5)  := USB-IF Defined Class Codes (usb.org/defined-class-codes) (Accessed: September 19, 2023).
	*Source(6) := USB CDC Specification v1.1 (Dated: January 19, 1999).
	Source(7)  := Device Class Definition for Human Interface Devices (HID) Version 1.11 (Dated: 5/27/01).
	Source(8)  := HID Usage Tables v1.4 (Dated: 1996-2022).
	Source(9)  := USB Interface Association Descriptor Device Class Code and Use Model (Dated: July 23, 2003).
	Source(10) := USB 3.0 Specification (Dated: November 12, 2008).
	Source(11) := Universal Serial Bus Mass Storage Class Specification Overview (Dated: February 19, 2010).
	Source(12) := Universal Serial Bus Mass Storage Class Bulk-Only Transport (Dated: September 31, 1999).
	Source(13) := SCSI Commands Reference Manual (Dated: October 2016).
	Source(14) := Mysterious TOSHIBA Draft TODO??
	Source(15) := Microsoft FAT Specification (Dated: August 30 2005).
	Source(16) := "Master boot record" on Wikipedia (Accessed: October 7, 2023).
	Source(17) := FAT Filesystem by Elm-Chan (Updated on: October 31, 2020).

	We are working within the environment of the ATmega32U4 microcontroller,
	which is an 8-bit CPU. This consequently means that there are no padding bytes to
	worry about within structs, and that the CPU doesn't exactly have the concept of
	"endianess", since all words are single bytes, so it's really up to the compiler
	on how it will layout memory and perform arithmetic calculations on operands greater
	than a byte. That said, the MCU's 16-bit opcodes and AVR-GCC's ABI are defined to use
	little-endian. In short: no padding bytes exist and we can take advantage of LSB being first
	before the MSB.

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
	When USBClass_null is used in device descriptors (bDeviceClass), the class is supposedly
	determined by the interfaces of the configurations (bInterfaceClass). (1) says that
	USBClass_null cannot be used in interface descriptors as it is "reserved for future
	use". However, (2) states that it is for the "null class code triple" now,
	whatever that means! Anyways, USBClass_null in the device descriptor level doesn't really
	help with creating a device with multi-function capabilities, which I am supposing is due to
	the fact that USB drivers can't seem to be able to parse the configuration descriptor properly.
	And thus, IAD was introduced and handles it all for us.

	(1) "bInterfaceClass" @ Source(2) @ Table(9-12) @ Page(268).
	(2) "Base Class 00h (Device)" @ Source(5).
*/

/* [ATmega32U4's Configuration of Endpoint 0].
	For UECFG0X, endpoint 0 will always be a control-typed endpoint as enforced
	by the USB specification, so that part of the configuration is straight-forward.
	For the data-direction of endpoint 0, it seems like the ATmega32U4 datasheet wants
	us to define endpoint 0 to have an "OUT" direction (1), despite the fact that
	control-typed endpoints are bidirectional.

	(1) EPDIR @ Source(1) @ Section(22.18.2) @ Page(287).
*/
