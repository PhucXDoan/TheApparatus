#define pin_input(P)  concat(pin_input_ , P)()
#define pin_output(P) concat(pin_output_, P)()
#define pin_low(P)    concat(pin_low_   , P)()
#define pin_high(P)   concat(pin_high_  , P)()
#define pin_read(P)   concat(pin_read_  , P)()

// The X-macro data-table maps pin numbers to corresponding data-direction ports and bit-index. See: Source(3) & Source(18).
#define COMMON(NAME, BODY) __attribute__((always_inline)) static inline void NAME(void) { BODY; }
	#define PIN_INPUT(P, X, N)  COMMON(pin_input_##P , DDR##X  &= ~(1 << DD##X##N  ))
	#define PIN_OUTPUT(P, X, N) COMMON(pin_output_##P, DDR##X  |=  (1 << DD##X##N  ))
	#define PIN_LOW(P, X, N)    COMMON(pin_low_##P   , PORT##X &= ~(1 << PORT##X##N))
	#define PIN_HIGH(P, X, N)   COMMON(pin_high_##P  , PORT##X |=  (1 << PORT##X##N))
		PIN_XMDT(PIN_INPUT)
		PIN_XMDT(PIN_OUTPUT)
		PIN_XMDT(PIN_LOW)
		PIN_XMDT(PIN_HIGH)
	#undef PIN_INPUT
	#undef PIN_OUTPUT
	#undef PIN_LOW
	#undef PIN_HIGH
#undef COMMON
#define PIN_READ(P, X, N) \
	__attribute__((always_inline)) \
	static inline b8 \
	pin_read_##P(void) \
	{ \
		return PIN##X & (1 << PIN##X##N); \
	}
PIN_XMDT(PIN_READ)
#undef PIN_READ

#if PROGRAM_DIPLOMAT
__attribute__((noreturn))
static void
error(enum PinHaltSource source) // TODO
{
	cli();

	DDRD |= (1 << DDD5);
	for (;;)
	{
		for (u8 i = 0; i < 8; i += 1)
		{
			PORTD |= (1 << PORTD5);
			_delay_ms(25.0);
			PORTD &= ~(1 << PORTD5);
			_delay_ms(25.0);
		}

		_delay_ms(250.0);

		for (u8 i = 0; i < source; i += 1)
		{
			PORTD |= (1 << PORTD5);
			_delay_ms(150.0);
			PORTD &= ~(1 << PORTD5);
			_delay_ms(150.0);
		}

		_delay_ms(250.0);
	}
}
#endif

#if PROGRAM_NERD
__attribute__((noreturn))
static void
error(enum PinHaltSource source) // TODO
{
	cli();

	pin_output(13);
	for (;;)
	{
		pin_high(13);
		_delay_ms(25.0);
		pin_low(13);
		_delay_ms(25.0);
	}
}
#endif

#define error error(PIN_HALT_SOURCE)

#if DEBUG // TODO
// Long pulse followed by some amount of pairs of flashes.
__attribute__((noreturn))
static void
debug_halt(u8 amount)
{
	cli();

	DDRB |= (1 << DDB0);
	for (;;)
	{
		PORTB |= (1 << PORTB0);
		_delay_ms(2000.0);
		PORTB &= ~(1 << PORTB0);
		_delay_ms(1000.0);

		for (u8 i = 0; i < amount; i += 1)
		{
			PORTB |= (1 << PORTB0);
			_delay_ms(25.0);
			PORTB &= ~(1 << PORTB0);
			_delay_ms(25.0);
			PORTB |= (1 << PORTB0);
			_delay_ms(25.0);
			PORTB &= ~(1 << PORTB0);
			_delay_ms(1000.0);
		}
	}
}
#endif

#if DEBUG
static void
debug_pin_set(u8 pin, enum PinState state)
{
	if (state & 0b10)
	{
		switch (pin)
		{
			#define PIN_INPUT(P, X, N) case P: pin_input_##P(); break;
			PIN_XMDT(PIN_INPUT);
			#undef PIN_INPUT
		}
	}
	else
	{
		switch (pin)
		{
			#define PIN_OUTPUT(P, X, N) case P: pin_output_##P(); break;
			PIN_XMDT(PIN_OUTPUT);
			#undef PIN_OUTPUT
		}
	}

	if (state & 0b01)
	{
		switch (pin)
		{
			#define PIN_HIGH(P, X, N) case P: pin_high_##P(); break;
			PIN_XMDT(PIN_HIGH);
			#undef PIN_HIGH
		}
	}
	else
	{
		switch (pin)
		{
			#define PIN_LOW(P, X, N) case P: pin_low_##P(); break;
			PIN_XMDT(PIN_LOW);
			#undef PIN_LOW
		}
	}
}
#endif

#if DEBUG
static b8
debug_pin_read(u8 pin)
{
	switch (pin)
	{
		#define MAKE_CASE(P, X, N) case P: return pin_read_##P();
		PIN_XMDT(MAKE_CASE);
		#undef MAKE_CASE

		default: return false;
	}
}
#endif

#if DEBUG && PROGRAM_DIPLOMAT
static void
debug_u16(u16 value)
{
	pin_high(PIN_LEDS_SS);
	_delay_ms(1.0);
	SPDR = (value >> 8) & 0xFF;
	while (!(SPSR & (1 << SPIF)));
	SPDR = (value >> 0) & 0xFF;
	while (!(SPSR & (1 << SPIF)));
	pin_low(PIN_LEDS_SS);
	_delay_ms(1.0);
}
#endif

#if DEBUG && PROGRAM_DIPLOMAT
static void
debug_dump(u8* data, u16 length)
{
	pin_low(PIN_DUMP_SS);
	_delay_ms(1.0);
	for (u16 i = 0; i < length; i += 1)
	{
		SPDR = data[i];
		while (!(SPSR & (1 << SPIF)));
		_delay_ms(1.0);
	}
	pin_high(PIN_DUMP_SS);
}
#endif

#if DEBUG && PROGRAM_DIPLOMAT
__attribute__((noreturn))
static void
debug_unhandled(u16 line_number, enum PinHaltSource source)
{
	cli();

	debug_u16(line_number);

	DDRB |= (1 << DDB0);
	for (;;)
	{
		for (u8 i = 0; i < 8; i += 1)
		{
			PORTB |= (1 << PORTB0);
			_delay_ms(25.0);
			PORTB &= ~(1 << PORTB0);
			_delay_ms(25.0);
		}

		_delay_ms(250.0);

		for (u8 i = 0; i < source; i += 1)
		{
			PORTB |= (1 << PORTB0);
			_delay_ms(150.0);
			PORTB &= ~(1 << PORTB0);
			_delay_ms(150.0);
		}

		_delay_ms(250.0);
	}
}
#define debug_unhandled debug_unhandled(__LINE__, PIN_HALT_SOURCE)
#endif

//
// Documentation.
//

/* [Overview].
	This file just defines stuff related to the general-purpose input-output pins (GPIO).
	The pin numberings are determined by pin-out diagrams such as (2).

	In summary, GPIOs can be in four states (configured with pin_input/pin_output and
	pin_low/pin_high respectively):

		- Input, Low : Also called "floating", "tri-stated", "Hi-Z". This is the default
		state of an IO pin. The pin wouldn't be set to any particular voltage level, and
		reading from this pin with pin_read is unreliable due to noise.

		- Input, High : In this state, the pin's pull-up resistor is activated. This
		essentially now makes the voltage level be at VCC (~5 volts), but if the pin
		is connected to something that sinks current strong enough, the voltage level
		will drop quite a bit. This is makes reading from the pin much more reliable,
		as most environmental noise will not be able to affect the pin enough to get a
		faulty reading.

		- Output, Low : The GPIO pin will act as a sink; that is, GND of 0 volts.
		Reading from this pin will result in a falsy value.

		- Output, High : The GPIO pin will act as a source; that is, VCC of ~5 volts.
		Reading from this pin will result in a truthy value.

	The error procedure is defined here since making the program blink IO pins
	indefinitely is the most error-free of displaying the error condition. I'd put the
	procedure somewhere else, but C is stuck in the land of in-order compilation, and I
	don't like the idea of having forward declarations.

	The pin_input, pin_output, pin_low, pin_high, and pin_read "procedures" are defined
	in such a way that the compiler is pretty much guaranteed to emit a single
	instruction. Nonetheless, many debug procedures are defined for quick and sloppy
	debugging whenever necessary.

	(1) "Configuring the Pin" @ Source(1) @ Section(10.2.1) @ Page(68).
	(2) Arduino Leonardo Pinout Diagram @ Source(3).
*/

/* TODO[Power Consumption?]
	Apparently floating pins comsume more power than pins that are tied to 5v.
	How much though? Is it that important?
	* "Unconnected Pins" @ Source(1) @ Section(10.2.6) @ Page(71-72).
*/
