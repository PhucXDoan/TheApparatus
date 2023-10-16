#define pin_input(P)  concat(pin_input_ , P)()
#define pin_output(P) concat(pin_output_, P)()
#define pin_low(P)    concat(pin_low_   , P)()
#define pin_high(P)   concat(pin_high_  , P)()
#define pin_read(P)   concat(pin_read_  , P)()

#define COMMON(NAME, BODY) __attribute__((always_inline)) static inline void NAME(void) { BODY; }
	#define MAKE_INPUT(P, X, N)  COMMON(pin_input_##P , DDR##X  &= ~(1 << DD##X##N  ))
	#define MAKE_OUTPUT(P, X, N) COMMON(pin_output_##P, DDR##X  |=  (1 << DD##X##N  ))
	#define MAKE_LOW(P, X, N)    COMMON(pin_low_##P   , PORT##X &= ~(1 << PORT##X##N))
	#define MAKE_HIGH(P, X, N)   COMMON(pin_high_##P  , PORT##X |=  (1 << PORT##X##N))
		PIN_XMDT(MAKE_INPUT)
		PIN_XMDT(MAKE_OUTPUT)
		PIN_XMDT(MAKE_LOW)
		PIN_XMDT(MAKE_HIGH)
	#undef MAKE_INPUT
	#undef MAKE_OUTPUT
	#undef MAKE_LOW
	#undef MAKE_HIGH
#undef COMMON

#define MAKE(P, X, N) \
	__attribute__((always_inline)) \
	static inline b8 \
	pin_read_##P(void) \
	{ \
		return PIN##X & (1 << PIN##X##N); \
	}
PIN_XMDT(MAKE)
#undef MAKE

// Rapid flashes follwed by pulses indicating the source of error.
__attribute__((noreturn))
static void
error(enum HaltSource source)
{
	cli();

	pin_output(HALT);
	for (;;)
	{
		#if BOARD_PRO_MICRO || BOARD_LEONARDO // These boards has the LEDs active low.
			#define ON()  pin_low(HALT)
			#define OFF() pin_high(HALT)
		#endif

		#if BOARD_MEGA2560
			#define ON()  pin_high(HALT)
			#define OFF() pin_low(HALT)
		#endif

		for (u8 i = 0; i < 8; i += 1)
		{
			ON();
			_delay_ms(25.0);
			OFF();
			_delay_ms(25.0);
		}

		_delay_ms(250.0);

		for (u8 i = 0; i < source; i += 1)
		{
			ON();
			_delay_ms(150.0);
			OFF();
			_delay_ms(150.0);
		}

		_delay_ms(250.0);

		#undef ON
		#undef OFF
	}
}
#define error error(PIN_HALT_SOURCE)

#if DEBUG
// Long pulse followed by pairs of flashes.
__attribute__((noreturn))
static void
debug_halt(u8 amount)
{
	cli();

	pin_output(HALT);
	for (;;)
	{
		pin_high(HALT);
		_delay_ms(2000.0);
		pin_low(HALT);
		_delay_ms(1000.0);

		for (u8 i = 0; i < amount; i += 1)
		{
			pin_high(HALT);
			_delay_ms(25.0);
			pin_low(HALT);
			_delay_ms(25.0);
			pin_high(HALT);
			_delay_ms(25.0);
			pin_low(HALT);
			_delay_ms(1000.0);
		}
	}
}
#endif

#if DEBUG
	#if PROGRAM_DIPLOMAT
		static void
		debug_u16(u16 value)
		{
			pin_output(PIN_U16_CLK);
			pin_output(PIN_U16_DATA);
			pin_low(PIN_U16_CLK);
			pin_low(PIN_U16_DATA);

			for (u8 i = 0; i < sizeof(value) * 8; i += 1)
			{
				if ((value >> (sizeof(value) * 8 - 1 - i)) & 1)
				{
					pin_high(PIN_U16_DATA);
				}
				else
				{
					pin_low(PIN_U16_DATA);
				}

				pin_high(PIN_U16_CLK);
				pin_low(PIN_U16_CLK);
			}
		}
	#endif
#endif

//
// Documentation.
//

/* [Overview].
	This file just defines stuff related to the MCU's pins.
	The pin numberings are determined by pin-out diagrams such as (2).

	Pins can be in four states (configured with pin_input/pin_output and pin_low/pin_high
	respectively):

		- Input, Low : Also called "floating", "tri-stated", "Hi-Z". This is the default
		state of an IO pin. The pin wouldn't be set to any particular voltage level, and
		reading from this pin with pin_read is unreliable due to noise.

		- Input, High : In this state, the pin's pull-up resistor is activated. This
		essentially now makes the voltage level be at VCC (~5 volts), but if the pin
		is connected to something that sinks current strong enough, the voltage level
		will drop quite a bit. This is makes reading from the pin much more reliable,
		as most environmental noise will not be able to affect the pin enough to get a
		faulty reading.

		- Output, Low : The pin will act as a sink; that is, GND of 0 volts. Reading from this pin
		will result in a falsy value.

		- Output, High : The pin will act as a source; that is, VCC of ~5 volts. Reading from this
		pin will result in a truthy value.

	The pin_input, pin_output, pin_low, pin_high, and pin_read "procedures" are defined
	in such a way that the compiler is pretty much guaranteed to emit a single instruction. There
	may be multiple aliases for a single pin. For example, the Mega2560 board has digital pin 53
	that could be used for anything your heart desires, but it also has the special ability of
	being the SPI's slave-select pin. Thus, pin_output(53) and pin_output(SPI_SS) are equivalent.
	The PIN_XMDTs in defs.h simply defines the register and bit index needed to configure the
	specific pin, as described in (1).

	(1) "Configuring the Pin" @ Source(1) @ Section(10.2.1) @ Page(68).
	(2) Arduino Leonardo Pinout Diagram @ Source(3).
*/

/* TODO[Power Consumption?]
	Apparently floating pins comsume more power than pins that are tied to 5v.
	How much though? Is it that important?
	* "Unconnected Pins" @ Source(1) @ Section(10.2.6) @ Page(71-72).
*/
