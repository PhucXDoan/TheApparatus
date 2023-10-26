#define dary_push(DARY_PTR, SRC_PTR) \
	( \
		(false ? (DARY_PTR)->data : (SRC_PTR)), \
		dary_push_(&(DARY_PTR)->dary_void, (SRC_PTR), sizeof(*(SRC_PTR))) \
	)

static void
dary_push_(struct Dary_void* dary, void* src, i64 src_size)
{
	if (dary->length == dary->size)
	{
		dary->size *= 2;
		dary->size += 1;
		dary->data  = realloc(dary->data, dary->size * src_size);
		if (!dary->data)
		{
			error("Reallocation failed.");
		}
	}

	memmove(((u8*) dary->data) + dary->length * src_size, src, src_size);
	dary->length += 1;
}
