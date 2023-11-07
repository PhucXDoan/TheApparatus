#define F_CPU                   16'000'000
#define PROGRAM_DIPLOMAT        true
#define BOARD_PRO_MICRO         true
#define PIN_DEBUG_U16_DATA      2
#define PIN_DEBUG_U16_CLK       3
#define PIN_LCD_ENABLE          4
#define PIN_LCD_REGISTER_SELECT 5
#define PIN_LCD_DATA_4          6
#define PIN_LCD_DATA_5          7
#define PIN_LCD_DATA_6          8
#define PIN_LCD_DATA_7          9
#define PIN_ROTARY_BTN          10
#define PIN_ROTARY_CLK          A1
#define PIN_ROTARY_DAT          A3
#define PIN_USB_BUSY            A2
#define PIN_SD_SS               A0
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/crc16.h>
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "Diplomat_masks.h"
#include "pin.c"
#include "misc.c"
#include "spi.c"
#include "sd.c"
#include "usb.c"
#include "lcd.c"
#undef  PIN_HALT_SOURCE
#define PIN_HALT_SOURCE HaltSource_diplomat

int
main(void)
{
	//
	// Initialize.
	//

	if (USBCON != 0b0010'0000) // [Bootloader-Tampered State].
	{
		restart();
	}

	debug_u16(0);

	sei();
	lcd_init();
	spi_init();
	sd_init();
	usb_init();

	pin_input(PIN_ROTARY_BTN);
	pin_input(PIN_ROTARY_CLK);
	pin_input(PIN_ROTARY_DAT);
	b8 rotary_clk       = 0;
	u8 rotary_btn_bias  = 0;


	enum Menu
	{
		Menu_main,
		Menu_displaying,
	};

	enum MenuMainOption
	{
		// ... enum WordGameMap ...

		MenuMainOption_reset_filesystem = WordGameMap_COUNT,
		MenuMainOption_COUNT,
	};

	enum Menu           menu                             = {0};
	enum MenuMainOption menu_main_selected_option        = {0};
	enum MenuMainOption menu_main_first_displayed_option = {0};

	//
	// Loop.
	//

	while (true)
	{
		//
		// Update rotary.
		//

		i8 rotary_rotation   = 0;
		i8 rotary_transition = 0;

		if (pin_read(PIN_ROTARY_BTN)) // Rotary button unpressed?
		{
			//
			// Handle rotary rotation.
			//

			if (pin_read(PIN_ROTARY_CLK) != rotary_clk)
			{
				if (rotary_clk)
				{
					if (pin_read(PIN_ROTARY_DAT))
					{
						rotary_rotation = 1;
					}
					else
					{
						rotary_rotation = -1;
					}
				}

				rotary_clk = !rotary_clk;
			}

			//
			// Handle rotary button being unpressed.
			//

			if (rotary_btn_bias >= 128)
			{
				rotary_btn_bias -= 1;

				if (rotary_btn_bias < 128)
				{
					rotary_transition = -1;
					rotary_btn_bias   = 0;
				}
			}
			else
			{
				rotary_btn_bias = 0;
			}
		}
		else // Rotary button pressed.
		{
			if (rotary_btn_bias < 128)
			{
				rotary_btn_bias += 1;

				if (rotary_btn_bias >= 128)
				{
					rotary_transition = 1;
					rotary_btn_bias   = 255;
				}
			}
			else
			{
				rotary_btn_bias = 255;
			}
		}

		//
		// Update to the user input.
		//

		switch (menu)
		{
			case Menu_main:
			{
				menu_main_selected_option += MenuMainOption_COUNT + rotary_rotation;
				menu_main_selected_option %= MenuMainOption_COUNT;

				if ((menu_main_selected_option - menu_main_first_displayed_option + MenuMainOption_COUNT) % MenuMainOption_COUNT >= LCD_DIM_Y)
				{
					menu_main_first_displayed_option += MenuMainOption_COUNT + rotary_rotation;
					menu_main_first_displayed_option %= MenuMainOption_COUNT;
				}

				if (rotary_transition == -1)
				{
					if (menu_main_selected_option < (enum MenuMainOption) WordGameMap_COUNT)
					{
						assert(usb_ms_ocr_state == USBMSOCRState_ready);
						usb_ms_ocr_state          = USBMSOCRState_set;
						usb_ms_ocr_wordgame_board = pgm_u8(WORDGAME_MAP_INFO[menu_main_selected_option].board);

						menu = Menu_displaying;
					}
					else
					{
						switch (menu_main_selected_option)
						{
							case MenuMainOption_reset_filesystem:
							{
								cli();

								USBCON &= ~(1 << USBE);

								memset(sd_sector, 0, sizeof(sd_sector));
								for (u32 sector_address = 0; sector_address < FAT32_WIPE_SECTOR_COUNT; sector_address += 1)
								{
									if (!(sector_address & 0b1111))
									{
										lcd_reset();
										lcd_pstr
										(
											"Clearing sectors...\n"
											"\n"
											"     "
										);
										lcd_u64(sector_address);
										lcd_pstr("/");
										lcd_u64(FAT32_WIPE_SECTOR_COUNT);
										lcd_refresh();
									}

									sd_write(sector_address);
								}

								#define MAKE(SECTOR_DATA, SECTOR_ADDRESS) \
									{ \
										static_assert(sizeof(sd_sector) == sizeof(SECTOR_DATA)); \
										memcpy_P(sd_sector, &(SECTOR_DATA), sizeof(SECTOR_DATA)); \
										sd_write(SECTOR_ADDRESS); \
									}
								FAT32_SECTOR_XMDT(MAKE);
								#undef MAKE

								restart();
							} break;

							case MenuMainOption_COUNT:
							{
								error();
							} break;
						}
					}
				}
			} break;

			case Menu_displaying:
			{
				if (usb_ms_ocr_state == USBMSOCRState_ready && rotary_transition == -1)
				{
					menu = Menu_main;
				}
			} break;
		}

		//
		// Display menu.
		//

		lcd_reset();

		switch (menu)
		{
			case Menu_main:
			{
				for
				(
					u8 row = 0;
					row < (LCD_DIM_Y < WordGameMap_COUNT ? LCD_DIM_Y : WordGameMap_COUNT);
					row += 1
				)
				{
					enum MenuMainOption option = (menu_main_first_displayed_option + row) % MenuMainOption_COUNT;

					if (option < (enum MenuMainOption) WordGameMap_COUNT)
					{
						static_assert(2 + (sizeof(WORDGAME_MAP_INFO[option].print_name_cstr) - 1) <= LCD_DIM_X);
						lcd_pgm_cstr(WORDGAME_MAP_INFO[option].print_name_cstr);
					}
					else
					{
						switch (option)
						{
							case MenuMainOption_reset_filesystem : lcd_pstr("Reset file system"); break;
							case MenuMainOption_COUNT            : error(); break;
						}
					}

					if (option == menu_main_selected_option)
					{
						lcd_char(' ');
						lcd_char(LCD_LEFT_ARROW);
					}

					lcd_char('\n');
				}
			} break;

			case Menu_displaying:
			{
				switch (usb_ms_ocr_state)
				{
					case USBMSOCRState_ready:
					{
						switch (usb_ms_ocr_wordgame_board)
						{
							case WordGameBoard_anagrams_6:
							case WordGameBoard_anagrams_7:
							{
								for (u8 y = 0; y < pgm_u8(WORDGAME_BOARD_INFO[usb_ms_ocr_wordgame_board].dim_slots.y); y += 1)
								{
									for (u8 x = 0; x < pgm_u8(WORDGAME_BOARD_INFO[usb_ms_ocr_wordgame_board].dim_slots.x); x += 1)
									{
										lcd_pgm_char(LETTER_LCD_CODES[usb_ms_ocr_grid[0][x]]);
									}
									lcd_char('\n');
								}
							} break;

							case WordGameBoard_COUNT:
							{
								error();
							} break;
						}
					} break;

					case USBMSOCRState_set:
					{
						lcd_pstr("Waiting for BMP...");
					} break;

					case USBMSOCRState_processing:
					{
						lcd_pstr("Processing BMP...");
					} break;
				}
			} break;
		}

		lcd_refresh();
	}
}
//
// Documentation.
//

/* [Bootloader-Tampered State].
	For the ATmega32U4 that I have, the bootloader is executed whenever the MCU reinitializes from
	an external reset (RST was pulled low); other sources of resets do not. When the bootloader
	hands execution to the main program, it seems to leaves the USB-related registers (e.g. USBCON)
	not with their default values. This is pretty obviously inconvenient for us to set up USB
	correctly, and who knows what other registers are also tampered with by the bootloader!

	To resolve this, we just simply enable the watchdog timer and wait for it to trigger. This
	induces a hardware reset that won't have the MCU start with the bootloader messing around with
	our registers. AVR-GCC pretty much handles all for us, so don't have to worry about the minute
	details like the timed-writing sequences.

	See: "Watchdog Reset" @ Source(1) @ Section(8.6) @ Page(53-54).
	See: "Watchdog Timer" @ Source(1) @ Section(8.9) @ Page(55-56).
*/
