#define dary_push_n(DARY_PTR, SRC_PTR, SRC_COUNT) \
	( \
		(false ? (DARY_PTR)->data : (SRC_PTR)), \
		dary_push_(&(DARY_PTR)->dary_void, (SRC_PTR), sizeof(*(SRC_PTR)), (SRC_COUNT)) \
	)
#define dary_push(DARY_PTR, ...) dary_push_n((DARY_PTR), (__VA_ARGS__), 1)
static void
dary_push_(struct Dary_void* dary, void* src, i64 src_elem_size, i64 src_count)
{
	if (dary->size < dary->length + src_count)
	{
		dary->size = (dary->length + src_count + 1) * 2;
		dary->data = realloc(dary->data, dary->size * src_elem_size);
		if (!dary->data)
		{
			error("Reallocation failed.");
		}
	}

	memmove(((u8*) dary->data) + dary->length * src_elem_size, src, src_elem_size * src_count);
	dary->length += src_count;
}
