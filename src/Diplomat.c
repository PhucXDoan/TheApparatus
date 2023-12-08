#define F_CPU                   16'000'000
#define PROGRAM_DIPLOMAT        true
#define BOARD_PRO_MICRO         true
#define PIN_LCD_ENABLE          6
#define PIN_LCD_REGISTER_SELECT 7
#define PIN_LCD_DATA_4          5
#define PIN_LCD_DATA_5          4
#define PIN_LCD_DATA_6          3
#define PIN_LCD_DATA_7          2
#define PIN_SD_SS               8
#define PIN_BTN_LEFT            A1
#define PIN_BTN_MID             A0
#define PIN_BTN_RIGHT           10
#define PIN_USB_BUSY            9
#define PIN_NERD_RESET          A2
#define SPI_PRESCALER           SPIPrescaler_2
#define USART_N                 1
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/crc16.h>
#include <stdint.h>
#include <string.h>
#include "defs.h"
#include "W:/data/masks.h" // Generated by Microservices.c.
#include "pin.c"
#include "misc.c"
#include "spi.c"
#include "sd.c"
#include "lcd.c"
#include "usart.c"
#include "Diplomat_usb.c"

#if DEBUG
	static void
	debug_dump(char* file_path, u16 line_number)
	{
		lcd_init(); // Reinitialize LCD in case the dump occurred mid communication or we haven't had it initialized yet.
		lcd_reset();
		lcd_strlit("DUMPED\n");
		for (u16 i = 0; file_path[i]; i += 1)
		{
			if (file_path[i] == '\\')
			{
				lcd_char('/');
			}
			else
			{
				lcd_char(file_path[i]);
			}
		}
		lcd_strlit("\nLine ");
		lcd_u64   (line_number);
		lcd_refresh();
	}
#endif

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

static i8 // -1 released, 0 no change, 1 pressed.
update_btn(u8* btn_bias, b8 btn_released)
{
	i8 transition = 0;

	if (btn_released)
	{
		if (*btn_bias >= 128) // Currently still recognize button as being pressed?
		{
			*btn_bias -= 1;

			if (*btn_bias < 128) // Enough ticks have passed to the point where we're confident the button is released?
			{
				transition = -1;
				*btn_bias  = 0;
			}
		}
		else // Button is still released.
		{
			*btn_bias = 0;
		}
	}
	else if (*btn_bias < 128) // Currently still recognize button as being released?
	{
		*btn_bias += 1;

		if (*btn_bias >= 128) // Enough ticks have passed to the point where we're confident the button is pressed?
		{
			transition = 1;
			*btn_bias  = 255;
		}
	}
	else // Button is still pressed.
	{
		*btn_bias = 255;
	}

	return transition;
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

	sei();

	pin_output(PIN_NERD_RESET);
	pin_low(PIN_NERD_RESET);
	_delay_ms(1.0);
	pin_high(PIN_NERD_RESET);

	usart_init();
	lcd_init();
	spi_init();
	sd_init();
	usb_init();

	pin_pullup(PIN_BTN_LEFT);
	pin_pullup(PIN_BTN_MID);
	pin_pullup(PIN_BTN_RIGHT);
	u8                         btn_left_bias                          = 0;
	u8                         btn_mid_bias                           = 0;
	u8                         btn_right_bias                         = 0;
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
		b8 clicked  = update_btn(&btn_mid_bias, pin_read(PIN_BTN_MID)) == -1;
		i8 rotation = 0;
		if (update_btn(&btn_left_bias, pin_read(PIN_BTN_LEFT)) == -1)
		{
			rotation -= 1;
		}
		if (update_btn(&btn_right_bias, pin_read(PIN_BTN_RIGHT)) == -1)
		{
			rotation += 1;
		}

		switch (menu)
		{
			case Menu_main:
			{
				//
				// Update selection.
				//

				menu_main_selected_option += MenuMainOption_COUNT + rotation;
				menu_main_selected_option %= MenuMainOption_COUNT;
				if ((menu_main_selected_option - menu_main_first_displayed_option + MenuMainOption_COUNT) % MenuMainOption_COUNT >= LCD_DIM_Y)
				{
					menu_main_first_displayed_option += MenuMainOption_COUNT + rotation;
					menu_main_first_displayed_option %= MenuMainOption_COUNT;
				}

				if (rotation) // TEMP
				{
					usart_tx(0xFF); // Dummy byte to signal that we're ready for command from Nerd.
					u8 nerd_command = usart_rx();
					lcd_reset();
					lcd_u64(nerd_command);
					lcd_refresh();
					_delay_ms(1000.0);
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
							case MenuMainOption_reset_filesystem : lcd_strlit("Reset file system"); break;
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

				if (clicked)
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
										lcd_strlit
										(
											"Clearing sectors...\n"
											"\n"
											"     "
										);
										lcd_u64(sector_address);
										lcd_strlit("/");
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
				//
				// Update selection.
				//

				menu_chosen_map_selected_option += MenuChosenMapOption_COUNT + rotation;
				menu_chosen_map_selected_option %= MenuChosenMapOption_COUNT;
				if ((menu_chosen_map_selected_option - menu_chosen_map_first_displayed_option + MenuChosenMapOption_COUNT) % MenuChosenMapOption_COUNT >= LCD_DIM_Y - 1)
				{
					menu_chosen_map_first_displayed_option += MenuChosenMapOption_COUNT + rotation;
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
					lcd_strlit("  ");
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

				if (clicked)
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
							pin_low(PIN_NERD_RESET);
							_delay_ms(1.0);
							pin_high(PIN_NERD_RESET);

							usb_ms_ocr_state         = USBMSOCRState_set;
							diplomat_packet.wordgame = (enum WordGame) menu_main_selected_option;
							menu                     = Menu_displaying;
							scroll_y                 = 0;
						} break;

						case MenuChosenMapOption_datamine:
						{
							#define WAIT(MS) \
								do \
								{ \
									for (u16 i = 0; i < (MS); i += 1) \
									{ \
										if (update_btn(&btn_mid_bias, pin_read(PIN_BTN_MID)) == 1) \
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

							usb_mouse_command(false, 0, 0);

							CLICK(6, 163); // Show iMessage apps.
							WAIT(350);

							CLICK(14, 167); // Game Pigeon icon on iMessage bar.
							WAIT(350);

							u16 screenshot_count = 0;
							while (true)
							{
								lcd_reset();
								lcd_pgm_cstr(WORDGAME_INFO[menu_main_selected_option].print_name_cstr);
								lcd_strlit
								(
									"\n"
									"  Datamining #"
								);
								lcd_u64(screenshot_count);
								lcd_strlit
								(
									"...\n"
									"  Hold to cancel  "
								);
								lcd_refresh();

								CLICK(14, 245); // Word Games category.
								WAIT(375);

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
										CLICK(30, 227); // Anagrams game.
										WAIT(375);

										if (!screenshot_count)
										{
											// TODO Explain.
											CLICK(8, 240); // Language menu.
											WAIT(375);

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

											CLICK(22 + language_index * 17, 240);
											WAIT(375);

											if ((enum WordGame) menu_main_selected_option == WordGame_anagrams_english_6)
											{
												CLICK(67, 240);
											}
											else if ((enum WordGame) menu_main_selected_option == WordGame_anagrams_english_7)
											{
												CLICK(103, 240);
											}
										}

										play_button_y = 190;
									} break;

									case WordGame_wordhunt_4x4:
									case WordGame_wordhunt_o:
									case WordGame_wordhunt_x:
									case WordGame_wordhunt_5x5:
									{
										CLICK(64, 227); // Wordhunt games.
										WAIT(375);

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

											CLICK(24 + 27 * layout_index, 240);
											WAIT(375);
										}

										play_button_y = 202;
									} break;

									case WordGame_wordbites:
									{
										CLICK(99, 227); // WordBites game.
										WAIT(375);

										play_button_y = 202;
									} break;

									case WordGame_COUNT:
									{
										error();
									} break;
								}

								CLICK(121, 164); // Send.
								WAIT(1000);

								CLICK(64, 120); // Open game.
								WAIT(1250);

								CLICK(64, play_button_y); // Start.
								WAIT(750);

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
								WAIT(1000);

								// Close game.
								usb_mouse_command(false, 64, 20);
								WAIT(500);
								usb_mouse_command(true, 64, 100);
								WAIT(300);
								usb_mouse_command(false, 64, 100);
								WAIT(300);

								// Swipe away screenshot.
								usb_mouse_command(false, 25, 255);
								WAIT(500);
								usb_mouse_command(true, 0, 255);
								WAIT(200);
								usb_mouse_command(false, 0, 255);
								WAIT(200);
							}
							BREAK_DATAMINING:;
							lcd_reset();
							lcd_pgm_cstr(WORDGAME_INFO[menu_main_selected_option].print_name_cstr);
							lcd_strlit
							(
								"\n"
								"  Datamined "
							);
							lcd_u64(screenshot_count);
							lcd_strlit
							(
								"!\n"
								"  Release to finish"
							);
							lcd_refresh();
							while (update_btn(&btn_mid_bias, pin_read(PIN_BTN_MID)) != -1);

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
						switch (diplomat_packet.wordgame)
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
									u8 y = pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.y);
									y--;
								)
								{
									for (u8 x = 0; x < pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.x); x += 1)
									{
										lcd_char(pgm_u8(LETTER_INFO[diplomat_packet.board[y][x]].lcd_character_code));
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
										pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.x),
										pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.y),
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
										for (u8 x = 0; x < pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.x); x += 1)
										{
											if (is_slot_excluded(diplomat_packet.wordgame, x, y))
											{
												lcd_char(pgm_u8(LETTER_INFO[Letter_null].lcd_character_code));
											}
											else
											{
												lcd_char(pgm_u8(LETTER_INFO[diplomat_packet.board[y][x]].lcd_character_code));
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
						lcd_strlit("Waiting for BMP...");
					} break;

					case USBMSOCRState_processing:
					{
						lcd_strlit("Processing BMP...");
					} break;
				}
				lcd_refresh();

				//
				// React to selection.
				//

				if (usb_ms_ocr_state == USBMSOCRState_ready && clicked)
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
