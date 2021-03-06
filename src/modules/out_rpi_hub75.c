// Raspberry Pi HUB75 LED matrix output.
// Uses https://github.com/hzeller/rpi-rgb-led-matrix
// Place it in a directory next to sled's.
// Or, well, set the directory to the include path and object.

#include <types.h>
#include <timers.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <led-matrix-c.h>

struct RGBLedMatrix *matrix;
struct LedCanvas *offscreen_canvas;

static int width;
static int height;

static RGB *buffer;
#define PPOS(x, y) (x + (y * width))

int init(int modno, char* argstr) {
	struct RGBLedMatrixOptions options;
	memset(&options, 0, sizeof(options));

	// Options for two 64x32 1:8 scan matrices plugged together in a U-shape.
	// (The Adafruit ones.)
	options.rows = 32;
	options.cols = 64;
	options.chain_length = 2;
	options.pixel_mapper_config = "U-mapper";

	// Split argstr into fake argc/argv.
	int argc = 1;
	char* *argv = malloc(1 * sizeof(char*));
	if (argv == NULL)
		return 1;
	argv[0] = "out_rpi_hub75";

	if (argstr != NULL) {
		char* tok;
		while ((tok = strsep(&argstr, " ")) != NULL) {
			argv = realloc(argv, sizeof(char*) * (argc + 1));

			if (argv == NULL) // realloc failed
				return 1;

			argv[argc++] = tok;
		}
	}

	// Create the matrix with our options and args, if any.
	matrix = led_matrix_create_from_options(&options, &argc, &argv);
	if (matrix == NULL) {
		led_matrix_print_flags(stderr);
		return 2;
	}

	// Create an offscreen canvas so we can double buffer.
	// Also set the width and height vars so getx/gety work.
	offscreen_canvas = led_matrix_create_offscreen_canvas(matrix);
	led_canvas_get_size(offscreen_canvas, &width, &height);

	// Allocate our internal buffer.
	buffer = malloc((width * height * sizeof(RGB)));
	if (buffer == NULL)
		return 1;

	// Free stuff if argparsing happened.
	if (argstr != NULL) {
		free(argv);
		free(argstr);
	}

	return 0;
}

int getx(void) {
	return width;
}
int gety(void) {
	return height;
}

int set(int x, int y, RGB *color) {
	if (x < 0 || y < 0)
		return 1;
	if (x >= width || y >= height)
		return 2;

	buffer[PPOS(x, y)] = RGB(color->red, color->green, color->blue);
	return 0;
}

int clear(void) {
	// clear this canvas, make sure we clean it on the double buffer too.
	// problem with this is that it needs a render call.
	// so, instead, we simply don't, we just maintain our own buffer and copy our state over.
	memset(buffer, 0, (width * height * sizeof(RGB)));
	return 0;
};

int render(void) {
	// swap buffers on next vsync, so we don't get any tearing.
	// this is fine, because it'll be running at a very high refresh rate, hopefully.
	// however, in order to don't get any differential update issues, we need to syncronize state.
	int x;
	int y;
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++) {
			RGB color = buffer[PPOS(x, y)];
			led_canvas_set_pixel(offscreen_canvas, x, y, color.red, color.green, color.blue);
		}
	offscreen_canvas = led_matrix_swap_on_vsync(matrix, offscreen_canvas);
	return 0;
}

ulong wait_until(ulong desired_usec) {
	// Hey, we can just delegate work to someone else. Yay!
	return wait_until_core(desired_usec);
}

void wait_until_break(void) {
	return wait_until_break_core();
}

int deinit(void) {
	free(buffer);
	led_matrix_delete(matrix);
	return 0;
}
