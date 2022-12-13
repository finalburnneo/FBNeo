// some compilers are idiots so let's avoid having different struct for this

struct rectangle
{
	// mame-compatible rectangle object
	INT32 min_x;
	INT32 max_x;
	INT32 min_y;
	INT32 max_y;

	rectangle(INT32 minx = 0, INT32 maxx = 0, INT32 miny = 0, INT32 maxy = 0) {
		set(minx, maxx, miny, maxy);
	}

	void set(INT32 minx, INT32 maxx, INT32 miny, INT32 maxy) {
		min_x = minx; max_x = maxx;
		min_y = miny; max_y = maxy;
	}

	rectangle operator &= (const rectangle &other) {
		if (min_x < other.min_x) min_x = other.min_x;
		if (min_y < other.min_y) min_y = other.min_y;
		if (max_x > other.max_x) max_x = other.max_x;
		if (max_y > other.max_y) max_y = other.max_y;
		if (min_y > max_y) min_y = max_y;
		if (min_x > max_x) min_x = max_x;
		return *this;
	}
};
