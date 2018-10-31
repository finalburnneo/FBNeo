
void draw_vector(UINT32 *palette);
void vector_init();
void vector_reset();
INT32 vector_scan(INT32 nAction);
void vector_exit();
void vector_add_point(INT32 x, INT32 y, INT32 color, INT32 intensity);
void vector_set_scale(INT32 x, INT32 y);
void vector_set_offsets(INT32 x, INT32 y);
void vector_set_clip(INT32 xmin, INT32 xmax, INT32 ymin, INT32 ymax);
void vector_rescale(INT32 x, INT32 y);
