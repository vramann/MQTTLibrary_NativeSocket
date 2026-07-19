/*
 * blink.c — minimal libgpiod v2 example for Raspberry Pi 5.
 *
 * Requires libgpiod >= 2.0 (Raspberry Pi OS based on Debian Trixie or later).
 * Bookworm ships libgpiod 1.6.x, whose API is entirely different — check with
 * `pkg-config --modversion libgpiod` before debugging "wrong" compile errors.
 *
 * Usage: ./blink [gpiochip-path] [line-offset]
 * Defaults: /dev/gpiochip0, line 17 (physical pin 11).
 */
#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	const char *chip_path = argc > 1 ? argv[1] : "/dev/gpiochip0";
	unsigned int offset = argc > 2 ? (unsigned int)atoi(argv[2]) : 17;
	int ret = EXIT_FAILURE;

	struct gpiod_chip *chip = gpiod_chip_open(chip_path);
	if (!chip) {
		perror("gpiod_chip_open");
		return EXIT_FAILURE;
	}

	struct gpiod_line_settings *settings = gpiod_line_settings_new();
	gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
	gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

	struct gpiod_line_config *line_cfg = gpiod_line_config_new();
	gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings);

	struct gpiod_request_config *req_cfg = gpiod_request_config_new();
	gpiod_request_config_set_consumer(req_cfg, "blink");

	struct gpiod_line_request *request =
		gpiod_chip_request_lines(chip, req_cfg, line_cfg);
	if (!request) {
		perror("gpiod_chip_request_lines");
		goto out;
	}

	printf("Blinking %s line %u — Ctrl-C to stop\n", chip_path, offset);
	for (;;) {
		gpiod_line_request_set_value(request, offset,
					     GPIOD_LINE_VALUE_ACTIVE);
		usleep(500 * 1000);
		gpiod_line_request_set_value(request, offset,
					     GPIOD_LINE_VALUE_INACTIVE);
		usleep(500 * 1000);
	}

	ret = EXIT_SUCCESS;
out:
	gpiod_request_config_free(req_cfg);
	gpiod_line_config_free(line_cfg);
	gpiod_line_settings_free(settings);
	gpiod_chip_close(chip);
	return ret;
}
