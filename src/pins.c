/* TODO(Setting Pin Optimizations)
	There is potentially be a lot of garbage code that could be generated from
	changing pin configurations, particularly changing output pins from low to high
	or vice-versa. Each time we do that, we have to set DDRx, but really we only have to
	do this once. If we're doing it all over the codebase though, then there's very little
	chance for the compiler to realize that pin X will always be an output pin, so we don't
	need to write that one bit in DDRx every single time.
*/

void
pin_set(u8 pin, enum PinState state)
{
	/* Configuring Pin.
		We set the pin's output direction (determined by the 2nd LSB of the values in enum PinState)
		and then the pin's driven state (LSB of enum PinState).

		If the 2nd LSB is 0, then the pin is set to output some voltage level.
		In this case, the voltage level is determined by the LSB of enum PinState (5 volts when 1, 0 volts when 0).

		If the 2nd LSB is 1, then the pin will not be necessarily used for outputting a voltage signal.
		In this case, if the LSB is 0, then the pin is "floating" or "tri-state" or "Hi-Z" ("High-Impedence").
		This just means that reading from the pin will be noisy, perhaps because neighboring pins are
		influence it or some external signal is affecting its voltage level.
		This is the default state of a pin after the chip is reset.

		If we want to avoid noise, the pin's pull-up resistor can be activated (this will be where LSB is 1).
		The pull-up resistor pulls the pin's voltage level to ~5v, thus resisting any external noise
		from influencing it. The pin's voltage level can still be pulled down to 0 volts by some strong,
		outside force on purpose, like another chip. This is how we will read pin inputs reliably.

		TODO Power Consumption?
			Apparently floating pins comsume more power than pins that are tied to 5v.
			How much though? Is it that important?
			* "Unconnected Pins" @ Source(1) @ Section(10.2.6) @ Page(71-72).

		* "Configuring the Pin" @ Source(1) @ Section(10.2.1) @ Page(68).
		* PORTx, DDRx @ Source(1) @ Section(10.4.1) @ Page(84-86).
	*/

	switch (pin)
	{
		#define MAKE_CASE(P, X, N) case P: DDR##X ^= (DDR##X & (1 << DD##X##N)) ^ (((~state >> 1) & 1) << DD##X##N); break;
		PIN_XMDT(MAKE_CASE);
		#undef MAKE_CASE
	}

	switch (pin)
	{
		#define MAKE_CASE(P, X, N) case P: PORT##X ^= (PORT##X & (1 << PORT##X##N)) ^ ((state & 1) << PORT##X##N); break;
		PIN_XMDT(MAKE_CASE);
		#undef MAKE_CASE
	}
}

static b8
pin_read(u8 pin)
{
	/* Read Digital Input.
		We use PINx to simply read the binary voltage level of the specified pin!
		If the pin is configured as an output, then the reading will return whatever
		we have set the voltage level to. If it's in a floating state, when there is
		no reliable reading that can be gathered here. If it's in the input state
		(pull-up resistors enabled), then readings are reliable.

		* "Reading the Pin Value" @ Source(1) @ Section(10.2.4) @ Page(69).
		* PINx @ Source(1) @ Section(10.4.1) @ Page(84-86).
	*/

	switch (pin)
	{
		#define MAKE_CASE(P, X, N) case P: return (PIN##X >> PIN##X##N) & 1;
		PIN_XMDT(MAKE_CASE);
		#undef MAKE_CASE

		default: return false;
	}
}

static void
error_halt(void)
{
	cli();
	while (true)
	{
		pin_set(13, !pin_read(13));
		_delay_ms(50.0);
	}
}

static void
debug_halt(u8 flashes)
{
	while (true)
	{
		for (u8 i = 0; i < flashes; i += 1)
		{
			pin_set(13, true);
			_delay_ms(100.0);
			pin_set(13, false);
			_delay_ms(100.0);
		}
		pin_set(13, false);
		_delay_ms(1000.0);
	}
}

static void
debug_u8(u8 byte)
{
	pin_set(9, (byte >> 0) & 1);
	pin_set(8, (byte >> 1) & 1);
	pin_set(7, (byte >> 2) & 1);
	pin_set(6, (byte >> 3) & 1);
	pin_set(5, (byte >> 4) & 1);
	pin_set(4, (byte >> 5) & 1);
	pin_set(3, (byte >> 6) & 1);
	pin_set(2, (byte >> 7) & 1);
}
