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
#include "W:/data/masks.h"
#include "pin.c"
#include "misc.c"
#include "spi.c"
#include "sd.c"
#include "lcd.c"
#include "usb.c"
#undef  PIN_HALT_SOURCE
#define PIN_HALT_SOURCE HaltSource_diplomat

enum Menu
{
	Menu_main,
	Menu_chosen_map,
	Menu_displaying,
};

#define MENU_CHOSEN_MAP_OPTION_XMDT(X) \
	X(back     , "Back"       ) \
	X(auto_play, "Auto play"  ) \
	X(on_guard , "Be on guard") \
	X(datamine , "Datamine"   )

#define MENU_CHOSEN_MAP_OPTION_MAX_MESSAGE_SIZE_(NAME, MESSAGE) u8 NAME[sizeof(MESSAGE)];
#define MENU_CHOSEN_MAP_OPTION_MAX_MESSAGE_SIZE sizeof(union { MENU_CHOSEN_MAP_OPTION_XMDT(MENU_CHOSEN_MAP_OPTION_MAX_MESSAGE_SIZE_) })

enum MenuChosenMapOption
{
	#define MAKE(NAME, ...) MenuChosenMapOption_##NAME,
	MENU_CHOSEN_MAP_OPTION_XMDT(MAKE)
	#undef MAKE
	MenuChosenMapOption_COUNT,
};

static const char MENU_CHOSEN_MAP_OPTIONS[MenuChosenMapOption_COUNT][MENU_CHOSEN_MAP_OPTION_MAX_MESSAGE_SIZE] PROGMEM =
	{
		#define MAKE(NAME, MESSAGE) MESSAGE,
		MENU_CHOSEN_MAP_OPTION_XMDT(MAKE)
		#undef MAKE
	};

enum MenuMainOption
{
	// ... enum WordGame ...

	MenuMainOption_reset_filesystem = WordGame_COUNT,
	MenuMainOption_COUNT,
};

struct Rotary
{
	u8 clk;
	u8 btn_bias;
	i8 rotation;
	i8 transition;
};

static void
update_rotary(struct Rotary* rotary)
{
	rotary->rotation   = 0;
	rotary->transition = 0;

	if (pin_read(PIN_ROTARY_BTN)) // Rotary button unpressed?
	{
		//
		// Handle rotary rotation.
		//

		if (pin_read(PIN_ROTARY_CLK) != rotary->clk)
		{
			if (rotary->clk)
			{
				if (pin_read(PIN_ROTARY_DAT))
				{
					rotary->rotation = 1;
				}
				else
				{
					rotary->rotation = -1;
				}
			}

			rotary->clk = !rotary->clk;
		}

		//
		// Handle rotary button being unpressed.
		//

		if (rotary->btn_bias >= 128)
		{
			rotary->btn_bias -= 1;

			if (rotary->btn_bias < 128)
			{
				rotary->transition = -1;
				rotary->btn_bias   = 0;
			}
		}
		else
		{
			rotary->btn_bias = 0;
		}
	}
	else // Rotary button pressed.
	{
		if (rotary->btn_bias < 128)
		{
			rotary->btn_bias += 1;

			if (rotary->btn_bias >= 128)
			{
				rotary->transition = 1;
				rotary->btn_bias   = 255;
			}
		}
		else
		{
			rotary->btn_bias = 255;
		}
	}
}

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
	struct Rotary              rotary                                 = {0};
	enum   Menu                menu                                   = {0};
	enum   MenuMainOption      menu_main_selected_option              = {0};
	enum   MenuMainOption      menu_main_first_displayed_option       = {0};
	enum   MenuChosenMapOption menu_chosen_map_selected_option        = {0};
	enum   MenuChosenMapOption menu_chosen_map_first_displayed_option = {0};
	u16                        scroll_tick                            = 0;
	u8                         scroll_y                               = 0;

	//
	// Loop.
	//

	while (true)
	{
		update_rotary(&rotary);

		switch (menu)
		{
			case Menu_main:
			{
				//
				// Update selection.
				//

				menu_main_selected_option += MenuMainOption_COUNT + rotary.rotation;
				menu_main_selected_option %= MenuMainOption_COUNT;
				if ((menu_main_selected_option - menu_main_first_displayed_option + MenuMainOption_COUNT) % MenuMainOption_COUNT >= LCD_DIM_Y)
				{
					menu_main_first_displayed_option += MenuMainOption_COUNT + rotary.rotation;
					menu_main_first_displayed_option %= MenuMainOption_COUNT;
				}

				//
				// Render.
				//

				lcd_reset();

				for
				(
					u8 row = 0;
					row < (LCD_DIM_Y < WordGame_COUNT ? LCD_DIM_Y : WordGame_COUNT);
					row += 1
				)
				{
					enum MenuMainOption option = (menu_main_first_displayed_option + row) % MenuMainOption_COUNT;

					if (option < (enum MenuMainOption) WordGame_COUNT)
					{
						static_assert(2 + (WORDGAME_MAX_PRINT_NAME_SIZE - 1) <= LCD_DIM_X);
						lcd_pgm_cstr(WORDGAME_INFO[option].print_name_cstr);
					}
					else
					{
						switch (option) // TODO?
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

				lcd_refresh();

				//
				// React to selection.
				//

				if (rotary.transition == -1)
				{
					if (menu_main_selected_option < (enum MenuMainOption) WordGame_COUNT)
					{
						menu = Menu_chosen_map;
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

			case Menu_chosen_map:
			{
				assert(menu_main_selected_option < (enum MenuMainOption) WordGame_COUNT);

				//
				// Update selection.
				//

				menu_chosen_map_selected_option += MenuChosenMapOption_COUNT + rotary.rotation;
				menu_chosen_map_selected_option %= MenuChosenMapOption_COUNT;
				if ((menu_chosen_map_selected_option - menu_chosen_map_first_displayed_option + MenuChosenMapOption_COUNT) % MenuChosenMapOption_COUNT >= LCD_DIM_Y - 1)
				{
					menu_chosen_map_first_displayed_option += MenuChosenMapOption_COUNT + rotary.rotation;
					menu_chosen_map_first_displayed_option %= MenuChosenMapOption_COUNT;
				}

				//
				// Render.
				//

				lcd_reset();

				lcd_pgm_cstr(WORDGAME_INFO[menu_main_selected_option].print_name_cstr);
				lcd_char('\n');
				for
				(
					u8 row = 0;
					row < (LCD_DIM_Y - 1 < WordGame_COUNT ? LCD_DIM_Y - 1: WordGame_COUNT);
					row += 1
				)
				{
					enum MenuChosenMapOption option = (menu_chosen_map_first_displayed_option + row) % MenuChosenMapOption_COUNT;

					static_assert(MENU_CHOSEN_MAP_OPTION_MAX_MESSAGE_SIZE + 2 + 2 <= LCD_DIM_X);
					lcd_pstr("  ");
					lcd_pgm_cstr(MENU_CHOSEN_MAP_OPTIONS[option]);

					if (option == menu_chosen_map_selected_option)
					{
						lcd_char(' ');
						lcd_char(LCD_LEFT_ARROW);
					}

					lcd_char('\n');
				}

				lcd_refresh();

				//
				// React to selection.
				//

				if (rotary.transition == -1)
				{
					switch (menu_chosen_map_selected_option)
					{
						case MenuChosenMapOption_back:
						{
							menu = Menu_main;
						} break;

						case MenuChosenMapOption_auto_play:
						{
						} break;

						case MenuChosenMapOption_on_guard:
						{
							assert(usb_ms_ocr_state == USBMSOCRState_ready);
							usb_ms_ocr_state    = USBMSOCRState_set;
							usb_ms_ocr_wordgame = (enum WordGame) menu_main_selected_option;
							menu                = Menu_displaying;
						} break;

						case MenuChosenMapOption_datamine:
						{
							#define WAIT(MS) \
								do \
								{ \
									for (u16 i = 0; i < (MS); i += 1) \
									{ \
										update_rotary(&rotary); \
										if (rotary.transition == 1) \
										{ \
											goto BREAK_DATAMINING; \
										} \
										else \
										{ \
											_delay_ms(1.0); \
										} \
									} \
								} \
								while (false)
							#define CLICK(X, Y) \
								do \
								{ \
									usb_mouse_command(false, (X), (Y)); \
									WAIT(500); \
									usb_mouse_command(true, (X), (Y)); \
									WAIT(128); \
									usb_mouse_command(false, (X), (Y)); \
								} \
								while (false)

							u16 screenshot_count = 0;
							while (true)
							{
								lcd_reset();
								lcd_pgm_cstr(WORDGAME_INFO[menu_main_selected_option].print_name_cstr);
								lcd_pstr
								(
									"\n"
									"  Datamining #"
								);
								lcd_u64(screenshot_count);
								lcd_pstr
								(
									"...\n"
									"  Hold to cancel  "
								);
								lcd_refresh();

								usb_mouse_command(false, 0, 0);

								CLICK(45, 185); // Game Pigeon icon on iMessage bar.
								WAIT(350);

								CLICK(14, 255); // Word Games category.
								WAIT(350);

								u8 play_button_y = {0};
								switch ((enum WordGame) menu_main_selected_option)
								{
									case WordGame_anagrams_english_6:
									case WordGame_anagrams_english_7:
									case WordGame_anagrams_russian:
									case WordGame_anagrams_french:
									case WordGame_anagrams_german:
									case WordGame_anagrams_spanish:
									case WordGame_anagrams_italian:
									{
										CLICK(30, 238); // Anagrams game.
										WAIT(350);

										if (!screenshot_count)
										{
											// TODO Explain.
											CLICK(8, 250); // Language menu.
											WAIT(350);

											// TODO Language enum.
											u8 language_index = {0};
											switch ((enum WordGame) menu_main_selected_option)
											{
												case WordGame_anagrams_english_6 :
												case WordGame_anagrams_english_7 : language_index = 0; break;
												case WordGame_anagrams_russian   : language_index = 1; break;
												case WordGame_anagrams_french    : language_index = 2; break;
												case WordGame_anagrams_german    : language_index = 3; break;
												case WordGame_anagrams_spanish   : language_index = 4; break;
												case WordGame_anagrams_italian   : language_index = 5; break;
												case WordGame_wordhunt_4x4       :
												case WordGame_wordhunt_o         :
												case WordGame_wordhunt_x         :
												case WordGame_wordhunt_5x5       :
												case WordGame_wordbites          :
												case WordGame_COUNT              : error(); break;
											}

											CLICK(22 + language_index * 17, 255);
											WAIT(350);

											if ((enum WordGame) menu_main_selected_option == WordGame_anagrams_english_6)
											{
												CLICK(67, 255);
											}
											else if ((enum WordGame) menu_main_selected_option == WordGame_anagrams_english_7)
											{
												CLICK(103, 255);
											}
										}

										play_button_y = 200;
									} break;

									case WordGame_wordhunt_4x4:
									case WordGame_wordhunt_o:
									case WordGame_wordhunt_x:
									case WordGame_wordhunt_5x5:
									{
										CLICK(64, 238); // Wordhunt games.
										WAIT(350);

										if (!screenshot_count)
										{
											u8 layout_index = {0};
											switch ((enum WordGame) menu_main_selected_option)
											{
												case WordGame_wordhunt_4x4       : layout_index = 0; break;
												case WordGame_wordhunt_o         : layout_index = 1; break;
												case WordGame_wordhunt_x         : layout_index = 2; break;
												case WordGame_wordhunt_5x5       : layout_index = 3; break;
												case WordGame_anagrams_english_6 :
												case WordGame_anagrams_english_7 :
												case WordGame_anagrams_russian   :
												case WordGame_anagrams_french    :
												case WordGame_anagrams_german    :
												case WordGame_anagrams_spanish   :
												case WordGame_anagrams_italian   :
												case WordGame_wordbites          :
												case WordGame_COUNT              : error(); break;
											}

											CLICK(24 + 27 * layout_index, 255);
											WAIT(350);
										}

										play_button_y = 212;
									} break;

									case WordGame_wordbites:
									{
										CLICK(99, 238); // WordBites game.
										WAIT(350);

										play_button_y = 212;
									} break;

									case WordGame_COUNT:
									{
										error();
									} break;
								}

								CLICK(121, 168); // Send.
								WAIT(1000);

								CLICK(64, 120); // Open game.
								WAIT(500);

								CLICK(64, play_button_y); // Start.
								WAIT(500);

								// Double tap AssistiveTouch.
								usb_mouse_command(false, ASSISTIVE_TOUCH_X, ASSISTIVE_TOUCH_Y);
								WAIT(500);
								usb_mouse_command(true, ASSISTIVE_TOUCH_X, ASSISTIVE_TOUCH_Y);
								WAIT(64);
								usb_mouse_command(false, ASSISTIVE_TOUCH_X, ASSISTIVE_TOUCH_Y);
								WAIT(64);
								usb_mouse_command(true, ASSISTIVE_TOUCH_X, ASSISTIVE_TOUCH_Y);
								WAIT(64);
								usb_mouse_command(false, ASSISTIVE_TOUCH_X, ASSISTIVE_TOUCH_Y);
								screenshot_count += 1;
								WAIT(750);

								CLICK(122, 25); // Close game.
								WAIT(750);

								// Swipe away screenshot.
								usb_mouse_command(false, 25, 255);
								WAIT(500);
								usb_mouse_command(true, 0, 255);
								WAIT(100);
								usb_mouse_command(false, 0, 255);
							}
							BREAK_DATAMINING:;
							lcd_reset();
							lcd_pgm_cstr(WORDGAME_INFO[menu_main_selected_option].print_name_cstr);
							lcd_pstr
							(
								"\n"
								"  Datamined "
							);
							lcd_u64(screenshot_count);
							lcd_pstr
							(
								"!\n"
								"  Release to finish"
							);
							lcd_refresh();
							while (rotary.transition != -1)
							{
								update_rotary(&rotary);
							}

							#undef WAIT
							#undef CLICK
						} break;

						case MenuChosenMapOption_COUNT:
						{
							error();
						} break;
					}
				}
			} break;

			case Menu_displaying:
			{
				//
				// Render.
				//

				lcd_reset();
				switch (usb_ms_ocr_state)
				{
					case USBMSOCRState_ready:
					{
						switch (usb_ms_ocr_wordgame)
						{
							case WordGame_anagrams_english_6:
							case WordGame_anagrams_english_7:
							case WordGame_anagrams_russian:
							case WordGame_anagrams_french:
							case WordGame_anagrams_german:
							case WordGame_anagrams_spanish:
							case WordGame_anagrams_italian:
							case WordGame_wordhunt_4x4:
							{
								for
								(
									i8 y = pgm_u8(WORDGAME_INFO[usb_ms_ocr_wordgame].dim_slots.y) - 1;
									y >= 0;
									y -= 1
								)
								{
									for (u8 x = 0; x < pgm_u8(WORDGAME_INFO[usb_ms_ocr_wordgame].dim_slots.x); x += 1)
									{
										lcd_char(pgm_u8(LETTER_LCD_CODES[usb_ms_ocr_grid[y][x]]));
									}
									lcd_char('\n');
								}
							} break;

							case WordGame_wordhunt_o:
							case WordGame_wordhunt_x:
							case WordGame_wordhunt_5x5:
							case WordGame_wordbites:
							{
								u8_2 board_dim =
									{
										pgm_u8(WORDGAME_INFO[usb_ms_ocr_wordgame].dim_slots.x),
										pgm_u8(WORDGAME_INFO[usb_ms_ocr_wordgame].dim_slots.y),
									};

								scroll_tick += 1;
								if (!(scroll_tick & 0b0000'0111'1111'1111))
								{
									scroll_y += 1;
									scroll_y %= board_dim.y + 1;
								}

								for (u8 row = 0; row < LCD_DIM_Y; row += 1)
								{
									if ((scroll_y + row) % (board_dim.y + 1) < board_dim.y)
									{
										u8 y = board_dim.y - 1 - (scroll_y + row) % (board_dim.y + 1);
										for (u8 x = 0; x < pgm_u8(WORDGAME_INFO[usb_ms_ocr_wordgame].dim_slots.x); x += 1)
										{
											if (is_slot_excluded(usb_ms_ocr_wordgame, x, y))
											{
												lcd_char(pgm_u8(LETTER_LCD_CODES[Letter_null]));
											}
											else
											{
												lcd_char(pgm_u8(LETTER_LCD_CODES[usb_ms_ocr_grid[y][x]]));
											}
										}
									}

									lcd_char('\n');
								}
							} break;

							case WordGame_COUNT:
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
				lcd_refresh();

				//
				// React to selection.
				//

				if (usb_ms_ocr_state == USBMSOCRState_ready && rotary.transition == -1)
				{
					menu = Menu_main;
				}

			} break;
		}
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
