// TODO Copied from CMD. Revamp it!

enum PinState
{
	PinState_false    = 0b00, // The corresponding booleans
	PinState_true     = 0b01, // can just be used instead.
	PinState_floating = 0b10,
	PinState_input    = 0b11
};
#define PIN_DEFS(X) \
	X( 0, D, 2) X( 1, D, 3) X( 2, D, 1) X( 3, D, 0) X( 4, D, 4) X( 5, C, 6) X(6, D, 7) X(7, E, 6) \
	X( 8, B, 4) X( 9, B, 5) X(10, B, 6) X(11, B, 7) X(12, D, 6) X(13, C, 7)

static void
pin_set(u8 pin_index, enum PinState state)
{
	switch (pin_index) // Sets the data direction (i.e. input/output).
	{
		#define CASE(PIN_NUMBER, LETTER, INDEX) \
			case PIN_NUMBER: \
			{ \
				DDR##LETTER ^= (DDR##LETTER & (1 << DD##LETTER##INDEX)) ^ (((~state >> 1) & 1) << DD##LETTER##INDEX); \
			} break;
		PIN_DEFS(CASE);
		#undef CASE
	}

	switch (pin_index) // Sets the port value (i.e. is driven high).
	{
		#define CASE(PIN_NUMBER, LETTER, INDEX) \
			case PIN_NUMBER: \
			{ \
				PORT##LETTER ^= (PORT##LETTER & (1 << PORT##LETTER##INDEX)) ^ ((state & 1) << PORT##LETTER##INDEX); \
			} break;
		PIN_DEFS(CASE);
		#undef CASE
	}
}

static b8
pin_read(u8 pin_index)
{
	switch (pin_index)
	{
		#define CASE(PIN_NUMBER, LETTER, INDEX) \
			case PIN_NUMBER: \
			{ \
				return (PIN##LETTER >> PIN##LETTER##INDEX) & 1; \
			} break;
		PIN_DEFS(CASE);
		#undef CASE

		default:
		{
			return false;
		} break;
	}
}

static void
pin_output(u8 byte)
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
