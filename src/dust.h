#include <raylib.h>

#define MAX_TOUCH_POINTS 10

typedef struct TouchPoint {
	int id;
	Vector2 pos;
	bool active;
} TouchPoint;
