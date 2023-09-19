void
pin_set(u8 pin, enum PinState state)
{ // [Configuring Pin].

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
{ // [Read Digital Input].

	switch (pin)
	{
		#define MAKE_CASE(P, X, N) case P: return (PIN##X >> PIN##X##N) & 1;
		PIN_XMDT(MAKE_CASE);
		#undef MAKE_CASE

		default: return false;
	}
}

static void
debug_halt(u8 flashes)
{
	while (true)
	{
		for (u8 j = 0; j < 8; j += 1)
		{
			for (u8 i = 0; i < flashes; i += 1)
			{
				pin_set(2 + j, !pin_read(2 + j));
				_delay_ms(200.0);
				pin_set(2 + j, !pin_read(2 + j));
				_delay_ms(200.0);
			}
			_delay_ms(1000.0);
		}
	}
}

static void
debug_u8(u8 byte)
{
	pin_set(2, (byte >> 0) & 1);
	pin_set(3, (byte >> 1) & 1);
	pin_set(4, (byte >> 2) & 1);
	pin_set(5, (byte >> 3) & 1);
	pin_set(6, (byte >> 4) & 1);
	pin_set(7, (byte >> 5) & 1);
	pin_set(8, (byte >> 6) & 1);
	pin_set(9, (byte >> 7) & 1);
}

static void
error_halt(void) // TODO Figure out a good error signaling method that isn't potentially bloated.
{
	cli();
	while (true)
	{
		debug_u8(0);
		_delay_ms(50.0);
		debug_u8(-1);
		_delay_ms(50.0);
	}
}

//
// Internal Documentation.
//

/* [Configuring Pin].
	We set the pin's output direction (determined by the 2nd LSB of the values in
	enum PinState) and then the pin's driven state (LSB of enum PinState).

	* "Configuring the Pin" @ Source(1) @ Section(10.2.1) @ Page(68).
	* PORTx, DDRx @ Source(1) @ Section(10.4.1) @ Page(84-86).
*/

/* [Read Digital Input].
	We use PINx to simply read the binary voltage level of the specified pin.
	If the pin is configured as an output, then the reading will return whatever
	we have set the voltage level to. If it's in a floating state, then there is
	no the readings that are gathered here are unreliable. If the pin is in
	PinState_input (pull-up resistors enabled), then readings will be reliable, where
	a truthy value is returned by default if the pin is unconnected to any sink.

	* "Reading the Pin Value" @ Source(1) @ Section(10.2.4) @ Page(69).
	* PINx @ Source(1) @ Section(10.4.1) @ Page(84-86).
*/


/* TODO[Setting Pin Optimizations].
	There is potentially be a lot of garbage code that could be generated from
	changing pin configurations, particularly changing output pins from low to high
	or vice-versa. Each time we do that, we have to set DDRx, but really we only have to
	do this once. If we're doing it all over the codebase though, then there's very little
	chance for the compiler to realize that pin X will always be an output pin, so we don't
	need to write that one bit in DDRx every single time.
*/

/* TODO[Power Consumption?]
		Apparently floating pins comsume more power than pins that are tied to 5v.
		How much though? Is it that important?
		* "Unconnected Pins" @ Source(1) @ Section(10.2.6) @ Page(71-72).
*/
