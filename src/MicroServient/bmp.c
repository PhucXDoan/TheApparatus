static b32 // Error.
bmp_malloc_read_file(struct BMP* dst, str file_path)
{
	return true;
}

static void
bmp_free_read_file(struct BMP* bmp)
{
	*bmp = (struct BMP) {0};
}
