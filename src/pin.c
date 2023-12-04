#define pin_pullup(P) concat(pin_pullup_, P)()
#define pin_output(P) concat(pin_output_ , P)()
#define pin_low(P)    concat(pin_low_    , P)()
#define pin_high(P)   concat(pin_high_   , P)()
#define pin_read(P)   concat(pin_read_   , P)()

#define COMMON(NAME, BODY) __attribute__((always_inline)) static inline void NAME(void) { BODY }
	#define MAKE_PULLUP(P, X, N) COMMON(pin_pullup_##P, DDR##X  &= ~(1 << DD##X##N  ); PORT##X |=  (1 << PORT##X##N);)
	#define MAKE_OUTPUT(P, X, N) COMMON(pin_output_##P, DDR##X  |=  (1 << DD##X##N  );                               )
	#define MAKE_LOW(P, X, N)    COMMON(pin_low_##P   , PORT##X &= ~(1 << PORT##X##N);                               )
	#define MAKE_HIGH(P, X, N)   COMMON(pin_high_##P  , PORT##X |=  (1 << PORT##X##N);                               )
		PIN_XMDT(MAKE_PULLUP)
		PIN_XMDT(MAKE_OUTPUT)
		PIN_XMDT(MAKE_LOW)
		PIN_XMDT(MAKE_HIGH)
	#undef MAKE_PULLUP
	#undef MAKE_OUTPUT
	#undef MAKE_LOW
	#undef MAKE_HIGH
#undef COMMON

#define MAKE(P, X, N) \
	[[nodiscard]] \
	__attribute__((always_inline)) \
	static inline b8 \
	pin_read_##P(void) \
	{ \
		return (PIN##X >> PIN##X##N) & 1; \
	}
PIN_XMDT(MAKE)
#undef MAKE

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
