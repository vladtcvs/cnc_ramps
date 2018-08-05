#include <stddef.h>
#include <stdio.h>

#include "moves.h"
#include "control.h"
#include "planner.h"

#include <gcode/gcodes.h>
#include <err.h>

#include <shell/shell.h>
#include <shell/print.h>

#define ANSWER_OK(str)  shell_print_answer(0, (str))
#define ANSWER_ERR(err, str)  shell_print_answer(err, (str))

static void print_endstops(void)
{
	int q = empty_slots();
	cnc_endstops stops = moves_get_endstops();
	shell_send_string("ok X: ");
	shell_send_char(stops.stop_x + '0');
	shell_send_string(" Y: ");
	shell_send_char(stops.stop_y + '0');
	shell_send_string(" Z: ");
	shell_send_char(stops.stop_z + '0');
	shell_send_string(" Q: ");
	shell_print_dec(q);
	shell_send_string("\r\n");
}

static void print_position(void)
{	
	int q = empty_slots();
	shell_send_string("ok X: ");
	shell_print_fixed_2(position.pos[0]);
	shell_send_string(" Y: ");
	shell_print_fixed_2(position.pos[1]);
	shell_send_string(" Z: ");
	shell_print_fixed_2(position.pos[2]);
	shell_send_string(" Q: ");
	shell_print_dec(q);
	shell_send_string("\r\n");
}

static void send_ok(void)
{
	int q = empty_slots();
	shell_send_string("ok Q:");
	shell_print_dec(q);
	shell_send_string("\r\n");
}

static void next_cmd(void);

static int handle_g_command(gcode_frame_t *frame)
{
	gcode_cmd_t *cmds = frame->cmds;
	int ncmds = frame->num;

	// skip line number(s)
	while (ncmds > 0 && cmds[0].type == 'N') {
		ncmds--;
		cmds++;
	}
	if (ncmds == 0)
	{
		send_ok();
		return -E_OK;
	}
	// parse command line
	switch (cmds[0].type) {
	case 'G':
		switch (cmds[0].val_i) {
		case 0:
		case 1: {
			int i;
			int32_t f = 0, feed0 = 0, feed1 = 0;
			int32_t x[3] = {0, 0, 0};
			for (i = 1; i < ncmds; i++) {
				switch (cmds[i].type) {
				case 'X':
					x[0] = cmds[i].val_f;
					break;
				case 'Y':
					x[1] = cmds[i].val_f;
					break;
				case 'Z':
					x[2] = cmds[i].val_f;
					break;
				case 'F':
					f = cmds[i].val_i;
					break;
				case 'P':
					feed0 = cmds[i].val_i;
					break;
				case 'L':
					feed1 = cmds[i].val_i;
					break;
				}
			}

			planner_line_to(x, f, feed0, feed1);
			send_ok();
			return -E_OK;
		}
		case 28: {
			int i;
			int rx = 0, ry = 0, rz = 0;
			for (i = 1; i < ncmds; i++) {
				switch (cmds[i].type) {
				case 'X':
					rx = 1;
					break;
				case 'Y':
					ry = 1;
					break;
				case 'Z':
					rz = 1;
					break;
				}
			}
			if (!rx && !ry && !rz) {
				rx = 1;
				ry = 1;
				rz = 1;
			}

			planner_find_begin(rx, ry, rz);
			send_ok();
			return -E_OK;
		}
		default:
 			shell_print_answer(-1, "unknown command number");
			return -E_INCORRECT;
		}
		break;
	case 'M':
		switch (cmds[0].val_i) {
		case 114:
			print_position();
			return -E_OK;
		case 119:
			print_endstops();
			return -E_OK;
		case 204:
			if (cmds[1].type != 'T') {
				shell_print_answer(-1, "expected: M204 Txx");
			}
			set_acceleration(cmds[1].val_i);
			send_ok();
			return -E_OK;
		default:
 			shell_print_answer(-1, "unknown command number");
			return -E_INCORRECT;
		}
		break;
	default:
 		shell_print_answer(-1, "unknown command");
		return -E_INCORRECT;
	}
	return -E_INCORRECT;
}

int execute_g_command(const char *command)
{
	gcode_frame_t frame;
	int rc;

	if (empty_slots() == 0) {
		ANSWER_ERR(-1, "no slots for frame");
		return -E_INCORRECT;
	}

	if ((rc = parse_cmdline(command, &frame)) < 0) {
		ANSWER_ERR(-1, "parse error");
		return rc;
	}

	return handle_g_command(&frame);
}

