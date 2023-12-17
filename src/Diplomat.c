#define F_CPU                   16'000'000
#define PROGRAM_DIPLOMAT        true
#define SPI_PRESCALER           SPIPrescaler_2
#define USART_N                 1
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
#define PIN_ERROR               A3
#define PIN_XMDT(X) /* Similar to Arduino Leonardo pinouts. See: Source(3). */ \
	X(2     , D, 1) X(3      , D, 0) X(4       , D, 4) X(5       , C, 6) \
	X(6     , D, 7) X(7      , E, 6) X(8       , B, 4) X(9       , B, 5) \
	X(10    , B, 6) X(14     , B, 3) X(15      , B, 1) X(16      , B, 2) \
	X(A0    , F, 7) X(A1     , F, 6) X(A2      , F, 5) X(A3      , F, 4) \
	X(SPI_SS, B, 0) X(SPI_CLK, B, 1) X(SPI_MOSI, B, 2) X(SPI_MISO, B, 3)
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
		lcd_u64(line_number);
		lcd_refresh();
	}
#endif

static b8
update_inputs(void)
{
	b8 input_occured = false;

	input_clicked  = false;
	input_rotation = false;

	if (update_btn(&input_btn_mid_bias, pin_read(PIN_BTN_MID)) == -1)
	{
		input_clicked = true;
		input_occured = true;
	}

	if (update_btn(&input_btn_left_bias, pin_read(PIN_BTN_LEFT)) == -1)
	{
		input_rotation -= 1;
		input_occured   = true;
	}
	if (update_btn(&input_btn_right_bias, pin_read(PIN_BTN_RIGHT)) == -1)
	{
		input_rotation += 1;
		input_occured   = true;
	}

	return input_occured;
}

static void
apply_input_rotation_to_fst_and_selected_options(u8* fst_displayed_option, u8* selected_option, u8 option_count, u8 max_visible_options)
{
	*selected_option += option_count + input_rotation;
	*selected_option %= option_count;
	if ((*selected_option - *fst_displayed_option + option_count) % option_count >= max_visible_options)
	{
		*fst_displayed_option += option_count + input_rotation;
		*fst_displayed_option %= option_count;
	}
}

static void
enter_menu_of_selected_wordgame(enum WordGame wordgame)
{
	#define OPTION_XMDT(X) \
		X(back    , "Back"    ) \
		X(play    , "Play"    ) \
		X(datamine, "Datamine")

	enum Option
	{
		#define MAKE(NAME, TEXT) Option_##NAME,
		OPTION_XMDT(MAKE)
		#undef MAKE
		Option_COUNT,
	};

	static const char OPTION_TEXTS[][LCD_DIM_X] PROGMEM =
		{
			#define MAKE(NAME, TEXT) TEXT,
			OPTION_XMDT(MAKE)
			#undef MAKE
		};
	#define MAKE(NAME, TEXT) static_assert(sizeof(TEXT) < countof(OPTION_TEXTS[0]));
	OPTION_XMDT(MAKE)
	#undef MAKE

	enum Option fst_displayed_option = {0};
	enum Option selected_option      = {0};
	b8          done                 = false;
	while (!done)
	{
		//
		// Render.
		//

		lcd_reset();
		lcd_pgm_cstr(WORDGAME_INFO[wordgame].print_name_cstr);
		lcd_char('\n');
		for (u8 row = 0; row < u8_min(LCD_DIM_Y - 1, Option_COUNT); row += 1)
		{
			enum Option row_option = (fst_displayed_option + row) % Option_COUNT;

			lcd_strlit("  ");
			lcd_pgm_cstr(OPTION_TEXTS[row_option]);
			if (row_option == selected_option)
			{
				lcd_char(' ');
				lcd_char(LCD_LEFT_ARROW);
			}
			lcd_char('\n');
		}
		lcd_refresh();

		//
		// React to inputs.
		//

		while (!update_inputs());

		if (input_clicked)
		{
			switch (selected_option)
			{
				case Option_back:
				{
					done = true;
				} break;

				case Option_play:
				{
					lcd_reset();
					lcd_strlit("Beginning to play...");
					lcd_refresh();

					pin_low(PIN_NERD_RESET);
					_delay_ms(4.0);
					pin_high(PIN_NERD_RESET); // By the time OCR is finished, Nerd should all be set up and ready to go.

					u8 shortcuts_option_index = 0;
					switch (wordgame)
					{
						case WordGame_anagrams_english_6 :
						case WordGame_anagrams_russian   :
						case WordGame_anagrams_french    :
						case WordGame_anagrams_german    :
						case WordGame_anagrams_spanish   :
						case WordGame_anagrams_italian   : shortcuts_option_index = 0; break;
						case WordGame_anagrams_english_7 : shortcuts_option_index = 1; break;
						case WordGame_wordhunt_4x4       : shortcuts_option_index = 2; break;
						case WordGame_wordhunt_o         :
						case WordGame_wordhunt_x         :
						case WordGame_wordhunt_5x5       : shortcuts_option_index = 3; break;
						case WordGame_wordbites          : shortcuts_option_index = 4; break;
						case WordGame_COUNT              : error(); break; // Impossible.
					}

					u8 play_button_y = 0;
					switch (wordgame)
					{
						case WordGame_anagrams_english_6 :
						case WordGame_anagrams_english_7 :
						case WordGame_anagrams_russian   :
						case WordGame_anagrams_french    :
						case WordGame_anagrams_german    :
						case WordGame_anagrams_spanish   :
						case WordGame_anagrams_italian   : play_button_y = ANAGRAMS_PLAY_BUTTON_Y; break;
						case WordGame_wordhunt_4x4       :
						case WordGame_wordhunt_o         :
						case WordGame_wordhunt_x         :
						case WordGame_wordhunt_5x5       : play_button_y = WORDHUNT_PLAY_BUTTON_Y; break;
						case WordGame_wordbites          : play_button_y = WORDBITES_PLAY_BUTTON_Y; break;
						case WordGame_COUNT              : error(); break; // Impossible.
					}

					//
					// Open Peripheral-Offsite-Optimizer.
					//

					usb_mouse_command(false, 0, 0);
					_delay_ms(128.0);
					usb_mouse_command(false, ASSISTIVE_TOUCH_X, ASSISTIVE_TOUCH_Y);
					_delay_ms(64.0);
					usb_mouse_command(true, ASSISTIVE_TOUCH_X, ASSISTIVE_TOUCH_Y);
					_delay_ms(64.0);
					usb_mouse_command(false, ASSISTIVE_TOUCH_X, ASSISTIVE_TOUCH_Y);
					_delay_ms(2750.0);

					//
					// Select the option from drop-down menu.
					//

					usb_mouse_command(false, 64, 49 + shortcuts_option_index * 19);
					_delay_ms(256.0);
					usb_mouse_command(true, 64, 49 + shortcuts_option_index * 19);
					_delay_ms(64.0);
					usb_mouse_command(false, 64, 49 + shortcuts_option_index * 19);
					_delay_ms(64.0);

					//
					// Begin game!
					//

					usb_mouse_command(false, 64, play_button_y);
					_delay_ms(1000.0);
					usb_mouse_command(true, 64, play_button_y);
					_delay_ms(64.0);
					usb_mouse_command(false, 64, play_button_y);
					_delay_ms(64.0);
					usb_mouse_command(false, 0, 0);

					//
					// Set OCR to intercept the BMP from Peripheral-Offsite-Optimizer.
					//

					diplomat_packet =
						(struct DiplomatPacket)
						{
							.wordgame = wordgame,
						};
					usb_ms_ocr_state = USBMSOCRState_set;

					//
					// Indicate OCR has been set and is waiting to be triggered.
					//

					lcd_reset();
					lcd_strlit("Waiting for BMP...");
					lcd_refresh();
					while (usb_ms_ocr_state == USBMSOCRState_set);

					//
					// Indicate OCR has received BMP and is currently processing it.
					//

					lcd_reset();
					lcd_strlit("Processing BMP...");
					lcd_refresh();
					while (usb_ms_ocr_state == USBMSOCRState_processing);

					//
					// Display the OCR results and handle commands from Nerd!
					//

					u16  scroll_tick                          = 0;
					u8   scroll_y                             = 0;
					b8   began_word_in_wordhunt               = false;
					b8   currently_holding_piece_in_wordbites = false;
					b8   done                                 = false;
					u8_2 wordgame_dim_slots                   =
						{
							pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.x),
							pgm_u8(WORDGAME_INFO[diplomat_packet.wordgame].dim_slots.y),
						};

					while (true)
					{
						//
						// Scroll board if it's too big to fit entirely on the display.
						//

						if (wordgame_dim_slots.y > LCD_DIM_Y)
						{
							scroll_tick += 1;
							if (!(scroll_tick & ((1 << 11) - 1)))
							{
								scroll_y += 1;
								scroll_y %= wordgame_dim_slots.y + 1;
							}
						}

						//
						// Display board.
						//

						lcd_reset();
						for (u8 row = 0; row < u8_min(LCD_DIM_Y, wordgame_dim_slots.y); row += 1)
						{
							if ((scroll_y + row) % (wordgame_dim_slots.y + 1) < wordgame_dim_slots.y)
							{
								u8 y = wordgame_dim_slots.y - 1 - (scroll_y + row) % (wordgame_dim_slots.y + 1);
								for (u8 x = 0; x < wordgame_dim_slots.x; x += 1)
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

						if (done)
						{
							lcd_cursor_pos = (u8_2) { WORDGAME_MAX_DIM_SLOTS_X + 1, 0 };
							lcd_strlit("FINISHED!");
						}

						lcd_refresh();

						//
						// Handle Nerd command.
						//

						if (!done && usart_rx_available() && usb_mouse_command_finished)
						{
							u8 command = usart_rx();

							if (command == NERD_COMMAND_COMPLETE)
							{
								done = true;
							}
							else
							{
								usart_tx(0xFF); // Let Nerd know we have received the command.

								switch (wordgame)
								{
									{
										u8_2 init_pos;
										u8   delta;
										u8_2 submit_pos;

										case WordGame_anagrams_english_6:
										case WordGame_anagrams_russian:
										case WordGame_anagrams_french:
										case WordGame_anagrams_german:
										case WordGame_anagrams_spanish:
										case WordGame_anagrams_italian:
										{
											init_pos   = (u8_2) { ANAGRAMS_6_INIT_X, ANAGRAMS_6_INIT_Y };
											delta      = ANAGRAMS_6_DELTA;
											submit_pos = (u8_2) { ANAGRAMS_6_SUBMIT_X, ANAGRAMS_6_SUBMIT_Y };
										} goto ANAGRAMS;

										case WordGame_anagrams_english_7:
										{
											init_pos   = (u8_2) { ANAGRAMS_7_INIT_X, ANAGRAMS_7_INIT_Y };
											delta      = ANAGRAMS_7_DELTA;
											submit_pos = (u8_2) { ANAGRAMS_7_SUBMIT_X, ANAGRAMS_7_SUBMIT_Y };
										} goto ANAGRAMS;

										ANAGRAMS:;

										usb_mouse_command(false, init_pos.x + delta * NERD_COMMAND_X(command), init_pos.y);
										_delay_ms(48.0);
										usb_mouse_command(true , init_pos.x + delta * NERD_COMMAND_X(command), init_pos.y);
										_delay_ms(48.0);

										if (command & NERD_COMMAND_SUBMIT_BIT)
										{
											usb_mouse_command(false, submit_pos.x, submit_pos.y);
											_delay_ms(48.0);
											usb_mouse_command(true , submit_pos.x, submit_pos.y);
											_delay_ms(48.0);
											usb_mouse_command(false, submit_pos.x, submit_pos.y);
											_delay_ms(48.0);
										}
									} break;

									{
										u8_2 init_pos;
										u8   delta;

										case WordGame_wordhunt_4x4:
										{
											init_pos   = (u8_2) { WORDHUNT_4x4_INIT_X, WORDHUNT_4x4_INIT_Y };
											delta      = WORDHUNT_4x4_DELTA;
										} goto WORDHUNT;

										case WordGame_wordhunt_o:
										case WordGame_wordhunt_x:
										case WordGame_wordhunt_5x5:
										{
											init_pos   = (u8_2) { WORDHUNT_5x5_INIT_X, WORDHUNT_5x5_INIT_Y };
											delta      = WORDHUNT_5x5_DELTA;
										} goto WORDHUNT;

										WORDHUNT:;

										if (!began_word_in_wordhunt)
										{
											began_word_in_wordhunt = true;
											usb_mouse_command(false, init_pos.x + delta * NERD_COMMAND_X(command), init_pos.y - delta * NERD_COMMAND_Y(command));
										}

										usb_mouse_command(true, init_pos.x + delta * NERD_COMMAND_X(command), init_pos.y - delta * NERD_COMMAND_Y(command));

										if (command & NERD_COMMAND_SUBMIT_BIT)
										{
											began_word_in_wordhunt = false;
											usb_mouse_command(false, init_pos.x + delta * NERD_COMMAND_X(command), init_pos.y - delta * NERD_COMMAND_Y(command));
										}
									} break;

									case WordGame_wordbites:
									{
										u8 x = WORDBITES_INIT_X + WORDBITES_DELTA * NERD_COMMAND_X(command);
										u8 y = WORDBITES_INIT_Y - WORDBITES_DELTA * NERD_COMMAND_Y(command);

										if (currently_holding_piece_in_wordbites)
										{
											usb_mouse_command(true, x, y);
											_delay_ms(300.0);
											usb_mouse_command(false, x, y);
											_delay_ms(64.0);
										}
										else
										{
											usb_mouse_command(false, x, y);
											_delay_ms(128.0);
											usb_mouse_command(true, x, y);
											_delay_ms(64.0);
										}
										currently_holding_piece_in_wordbites = !currently_holding_piece_in_wordbites;
									} break;

									case WordGame_COUNT:
									{
										error(); // Impossible.
									} break;
								}
							}
						}

						//
						// Stop early by the user if needed.
						//

						if (update_btn(&input_btn_mid_bias, pin_read(PIN_BTN_MID)) == 1)
						{
							lcd_cursor_pos = (u8_2) { WORDGAME_MAX_DIM_SLOTS_X + 1, 0 };
							lcd_strlit("STOPPED");
							lcd_refresh();

							while (update_btn(&input_btn_mid_bias, pin_read(PIN_BTN_MID)) != -1);

							if (done)
							{
								break;
							}
							else
							{
								done = true;
								usb_mouse_command(false, 0, 0); // In case we're currently pressed.
							}
						}
					}
				} break;

				case Option_datamine:
				{
					//
					// Continuously send and take screenshots of wordgames until we are prompted to break out.
					//

					u16 screenshot_count = 0;
					while (true)
					{
						#define WAIT(MS) \
							do \
							{ \
								for (u16 i = 0; i < (MS); i += 1) \
								{ \
									if (update_btn(&input_btn_mid_bias, pin_read(PIN_BTN_MID)) == 1) \
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
						#define CLICK(X, Y, MS) \
							do \
							{ \
								usb_mouse_command(false, (X), (Y)); \
								WAIT(512); \
								usb_mouse_command(true, (X), (Y)); \
								WAIT(64); \
								usb_mouse_command(false, (X), (Y)); \
								WAIT(MS); \
							} \
							while (false)

						//
						// Display current screenshot.
						//

						lcd_reset();
						lcd_pgm_cstr(WORDGAME_INFO[wordgame].print_name_cstr);
						lcd_strlit
						(
							"\n"
							"  Screenshot #"
						);
						lcd_u64(screenshot_count);
						lcd_strlit
						(
							"...\n"
							"  Hold to cancel  "
						);
						lcd_refresh();

						//
						// Open Game Pigeon menu first time around.
						//

						if (!screenshot_count)
						{
							usb_mouse_command(false, 0, 0);

							CLICK( 6, 163, 500); // Show iMessage apps.
							CLICK(14, 167, 500); // Game Pigeon icon on iMessage bar.
						}

						//
						// Select the desired wordgame.
						//

						CLICK(14, 245, 375); // Open "Word Games" category.

						u8 play_button_y = {0};
						switch (wordgame)
						{
							case WordGame_anagrams_english_6:
							case WordGame_anagrams_english_7:
							case WordGame_anagrams_russian:
							case WordGame_anagrams_french:
							case WordGame_anagrams_german:
							case WordGame_anagrams_spanish:
							case WordGame_anagrams_italian:
							{
								play_button_y = ANAGRAMS_PLAY_BUTTON_Y;

								CLICK(30, 227, 375); // "Anagrams" group.

								//
								// Configure language and letter count first time around (configuration remains from then on).
								//

								if (!screenshot_count)
								{
									//
									// Click in such a way that we open up the lanugage options but not click
									// on "English" if the language menu happens to be already opened.
									//

									CLICK(8, 240, 375);

									//
									// Select the language.
									//

									CLICK(22 + pgm_u8(WORDGAME_INFO[wordgame].language) * 17, 240, 375);

									//
									// Pick letter count.
									//

									if (wordgame == WordGame_anagrams_english_6)
									{
										CLICK(67, 240, 32);
									}
									else if (wordgame == WordGame_anagrams_english_7)
									{
										CLICK(103, 240, 32);
									}
								}
							} break;

							case WordGame_wordhunt_4x4:
							case WordGame_wordhunt_o:
							case WordGame_wordhunt_x:
							case WordGame_wordhunt_5x5:
							{
								play_button_y = WORDHUNT_PLAY_BUTTON_Y;

								CLICK(64, 227, 375); // Click "Word Hunt" group.

								//
								// Configure the map first time around (configuration remains from then on).
								//

								if (!screenshot_count)
								{
									u8 map_index = {0};
									switch (wordgame)
									{
										case WordGame_wordhunt_4x4       : map_index = 0; break;
										case WordGame_wordhunt_o         : map_index = 1; break;
										case WordGame_wordhunt_x         : map_index = 2; break;
										case WordGame_wordhunt_5x5       : map_index = 3; break;
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

									CLICK(24 + 27 * map_index, 240, 375);
								}
							} break;

							case WordGame_wordbites:
							{
								play_button_y = WORDBITES_PLAY_BUTTON_Y;

								CLICK(99, 227, 375); // Click the sole "Word Bites" game.
							} break;

							case WordGame_COUNT:
							{
								error();
							} break;
						}

						//
						// Get to the game.
						//

						CLICK(121, 164, 1000);         // Send the game.
						CLICK(64, 120, 1250);          // Open the sent game.
						CLICK(64, play_button_y, 750); // Press the play button of the game.

						//
						// Double tap AssistiveTouch to take screenshot.
						// Make sure the double-tap timeout is long enough for this!
						//

						CLICK(ASSISTIVE_TOUCH_X, ASSISTIVE_TOUCH_Y, 1);
						CLICK(ASSISTIVE_TOUCH_X, ASSISTIVE_TOUCH_Y, 1);
						screenshot_count += 1;
						WAIT(1000);

						//
						// Drag Game Pigeon window down to close the game.
						//

						usb_mouse_command(false, 64, 20);
						WAIT(500);
						usb_mouse_command(true, 64, 100);
						WAIT(300);
						usb_mouse_command(false, 64, 100);
						WAIT(300);

						//
						// Swipe away screenshot that was just taken.
						//

						usb_mouse_command(false, 25, 255);
						WAIT(500);
						usb_mouse_command(true, 0, 255);
						WAIT(200);
						usb_mouse_command(false, 0, 255);
						WAIT(200);

						#undef WAIT
						#undef CLICK
					}
					BREAK_DATAMINING:;

					//
					// Display amount of screenshots taken.
					//

					lcd_reset();
					lcd_pgm_cstr(WORDGAME_INFO[wordgame].print_name_cstr);
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

					while (update_btn(&input_btn_mid_bias, pin_read(PIN_BTN_MID)) != -1);
				} break;

				case Option_COUNT:
				{
					error(); // Impossible.
				} break;
			}
		}
		else
		{
			apply_input_rotation_to_fst_and_selected_options(&fst_displayed_option, &selected_option, Option_COUNT, LCD_DIM_Y - 1);
		}
	}

	#undef OPTION_XMDT
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

	pin_pullup(PIN_BTN_LEFT);
	pin_pullup(PIN_BTN_MID);
	pin_pullup(PIN_BTN_RIGHT);
	pin_output(PIN_NERD_RESET);
	pin_high(PIN_NERD_RESET);

	usart_init();
	lcd_init();
	spi_init();
	sd_init();
	usb_init();

	//
	// Enter main menu.
	//

	#define BASE_OPTION_XMDT(X) \
		X(wipe_file_system, "Wipe file system")
	#define TOTAL_OPTION_COUNT (BaseOption_COUNT + WordGame_COUNT)

	enum BaseOption
	{
		#define MAKE(NAME, TEXT) BaseOption_##NAME,
		BASE_OPTION_XMDT(MAKE)
		#undef MAKE
		BaseOption_COUNT,
	};

	static const char BASE_OPTION_TEXTS[][LCD_DIM_X] PROGMEM =
		{
			#define MAKE(NAME, TEXT) TEXT,
			BASE_OPTION_XMDT(MAKE)
			#undef MAKE
		};
	#define MAKE(NAME, TEXT) static_assert(sizeof(TEXT) < countof(BASE_OPTION_TEXTS[0]));
	BASE_OPTION_XMDT(MAKE)
	#undef MAKE

	u8 fst_displayed_option = {0};
	u8 selected_option      = {0};
	while (true)
	{
		//
		// Render.
		//

		lcd_reset();
		for (u8 row = 0; row < u8_min(LCD_DIM_Y, TOTAL_OPTION_COUNT); row += 1)
		{
			u8 row_option = (fst_displayed_option + row) % TOTAL_OPTION_COUNT;

			if (row_option < BaseOption_COUNT)
			{
				lcd_pgm_cstr(BASE_OPTION_TEXTS[row_option]);
			}
			else
			{
				lcd_pgm_cstr(WORDGAME_INFO[row_option - BaseOption_COUNT].print_name_cstr);
			}

			if (row_option == selected_option)
			{
				lcd_char(' ');
				lcd_char(LCD_LEFT_ARROW);
			}

			lcd_char('\n');
		}
		lcd_refresh();

		//
		// React to inputs.
		//

		while (!update_inputs());

		if (!input_clicked)
		{
			apply_input_rotation_to_fst_and_selected_options(&fst_displayed_option, &selected_option, TOTAL_OPTION_COUNT, LCD_DIM_Y);
		}
		else if (selected_option < BaseOption_COUNT)
		{
			switch (selected_option)
			{
				case BaseOption_wipe_file_system:
				{
					//
					// Disconnect.
					//

					cli();
					USBCON &= ~(1 << USBE);

					//
					// Clear sectors.
					//

					memset(sd_sector, 0, sizeof(sd_sector));
					for (u32 sector_address = 0; sector_address < FAT32_WIPE_SECTOR_COUNT; sector_address += 1)
					{
						if (!(sector_address & ((1 << 5) - 1)))
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

					//
					// Rewrite file system.
					//

					#define MAKE(SECTOR_DATA, SECTOR_ADDRESS) \
						{ \
							static_assert(sizeof(sd_sector) == sizeof(SECTOR_DATA)); \
							memcpy_P(sd_sector, &(SECTOR_DATA), sizeof(SECTOR_DATA)); \
							sd_write(SECTOR_ADDRESS); \
						}
					FAT32_SECTOR_XMDT(MAKE);
					#undef MAKE

					//
					// Restart the Diplomat!
					//

					restart();
				} break;

				case BaseOption_COUNT:
				{
					error(); // Impossible.
				} break;
			}
		}
		else
		{
			enter_menu_of_selected_wordgame((enum WordGame) selected_option - BaseOption_COUNT);
		}
	}

	#undef BASE_OPTION_XMDT
	#undef TOTAL_OPTION_COUNT
}

//
// Documentation.
//

/* [Overview].
	TODO
*/

/* [Bootloader-Tampered State].
	Caterina is a common bootloader used on ATmega32U4 boards, including the Arduino Leonardo and
	Pro Micro. It works by having its own USB stack that opens a COM to be programmed through. If
	no commands are sent, the bootloader times out and the flow of execution is given to the main
	program. Consequently however, the USB registers at the beginning of the main program will be
	tampered with. To ensure that the MCU have all of its registers at its expected default bits,
	we just simply enable the watchdog timer and wait for it to trigger. This induces a hardware
	reset that won't have the MCU start with the bootloader messing around with our registers.
	AVR-GCC pretty much handles all for us, so don't have to worry about the minute details like
	the timed-writing sequences.

	Obviously, if there's no bootloader, everything will always be as expected at the beginning
	of main.

	See: "Watchdog Reset" @ Source(1) @ Section(8.6) @ Page(53-54).
	See: "Watchdog Timer" @ Source(1) @ Section(8.9) @ Page(55-56).
*/
