// Uses Timer0 to count amount of milliseconds elapsed since timer initialization.
//
//     - The firmware must not be interrupt-heavy, else this slows down the measurement.
//     - timer_ms is best executed as infrequently as possible.
//     - The timer will not overflow for over 49 days.
//     - Clock frequency is assumed to be 16MHz.

static_assert(F_CPU == 16'000'000);

static void
timer_init(void) // Uses the 8-bit Timer0.
{
	TCNT0  = TIMER_INITIAL_COUNTER;       // Timer0's initial counter value. See: [Overview].
	TCCR0B = (TimerPrescaler_64 << CS00); // Timer0's counter will be incrementing at a rate of F_CPU/TimerPrescaler.
	TIMSK0 = (1 << TOIE0);                // When Timer0's 8-bit counter overflows, the TIMER0_OVF_vect interrupt routine is triggered.
}

[[nodiscard]]
static u32 // Milliseconds since initialization of Timer0.
timer_ms(void)
{
	cli();
	u32 ms = _timer_ms;
	sei();
	return ms;
}

ISR(TIMER0_OVF_vect) // When Timer0's 8-bit counter overflows. See: [Overview].
{
	_timer_ms += 1;
	TCNT0      = TIMER_INITIAL_COUNTER;
}

//
// Documentation.
//

/* [Overview].
	A 16MHz clock would increment Timer0's 8-bit counter after some amount of cycles.
	The amount of cycles depends on the configured prescaler (e.g. prescaler of 64 means the 8-bit
	counter is incremented by 1 after 64 cycles). Since the counter is only 8 bits, it would
	overflow back to 0 after incrementing from 255. When this overflow occurs, an interrupt is
	also triggered, and we can use this interrupt to increment our own counter to signify that
	some amount of time has passed.

	For example, the amount of time it takes to overflow using 16MHz clock with prescaler of 64
	can be calculated as so:

		2^CLOCK_BIT_WIDTH / (F_CPU / PRESCALER)
			= 256 / (16,000,000Hz / 64)
			= 1.024ms.

	If we want exactly 1ms per overflow, the counter will need to begin at a specific value.
	Having TIMER_INITIAL_COUNTER = 6 does the trick.

		(2^CLOCK_BIT_WIDTH - INITIAL_VALUE) / (F_CPU / PRESCALER)
			= (256 - 6) / (16,000,000Hz / 64)
			= 1ms.

	So after 1ms, Timer0's counter will overflow and we increment our accumulator _timer_ms to
	keep measure. Since the accumulator is not a single byte, reading from it in the main program
	will potentially lead to a teared load where the interrupt is triggered right in the middle of
	the read. Thus, timer_ms() is best used to prevent this as it'll disable the interrupt system
	briefly to read it without trouble.

	Since the accumulator is incremented every 1ms, then the following table indicate when the
	accumulator itself will overflow:

		-----------------------------------------------------------------------
		| sizeof(_timer_ms) | Time elapsed to overflow.                       |
		-----------------------------------------------------------------------
		|                 8 | 2^8  * 0.001s =                         0.256s. |
		|                16 | 2^16 * 0.001s =                  1 min  5.536s. |
		|                32 | 2^32 * 0.001s = 49 days 17 hours 2 min 47.296s. |
		|                64 | 2^64 * 0.001s = End of human life.              |
		-----------------------------------------------------------------------

	1ms is a particularly nice number to increment from since it's a reasonable level of
	granularity for most applications. It can't really be reduced any further into the microsecond
	time range since the interrupt routine will end up being called too often for the MCU to do
	any useful work quickly.

	For performance measurements, it's best to call timer_ms() outside of interrupt routines and
	as little as possible inside of a tight loop.
*/
