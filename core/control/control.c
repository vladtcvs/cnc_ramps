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

static void print_endstops(int nid)
{
	send_started(nid);
	int q = empty_slots();
	cnc_endstops stops = moves_get_endstops();
	shell_send_string("completed");
	shell_send_string(" N:");
	shell_print_dec(nid);
	shell_send_string(" Q:");
	shell_print_dec(q);
	shell_send_string(" X:");
	shell_send_char(stops.stop_x + '0');
	shell_send_string(" Y:");
	shell_send_char(stops.stop_y + '0');
	shell_send_string(" Z:");
	shell_send_char(stops.stop_z + '0');
	shell_send_string(" P:");
	shell_send_char(stops.probe_z + '0');
	shell_send_string("\r\n");
}

static void print_position(int nid)
{
	send_started(nid);
	int q = empty_slots();
	shell_send_string("completed");
	shell_send_string(" N:");
	shell_print_dec(nid);
	shell_send_string(" Q:");
	shell_print_dec(q);
	shell_send_string(" X:");
	shell_print_fixed_2(position.pos[0]);
	shell_send_string(" Y:");
	shell_print_fixed_2(position.pos[1]);
	shell_send_string(" Z:");
	shell_print_fixed_2(position.pos[2]);
	shell_send_string("\r\n");
}

void send_queued(int nid)
{
	int q = empty_slots();
	shell_send_string("queued");
	shell_send_string(" N:");
	shell_print_dec(nid);
	shell_send_string(" Q:");
	shell_print_dec(q);
	shell_send_string("\r\n");
}

void send_started(int nid)
{
	int q = empty_slots();
	shell_send_string("started");
	shell_send_string(" N:");
	shell_print_dec(nid);
	shell_send_string(" Q:");
	shell_print_dec(q);
	shell_send_string("\r\n");
}

void send_completed(int nid)
{
	int q = empty_slots();
	shell_send_string("completed");
	shell_send_string(" N:");
	shell_print_dec(nid);
	shell_send_string(" Q:");
	shell_print_dec(q);
	shell_send_string("\r\n");
}

void send_ok(int nid)
{
	send_queued(nid);
	send_started(nid);
	send_completed(nid);
}

void send_error(int nid, const char *err)
{
	int q = empty_slots();
	shell_send_string("error");
	shell_send_string(" N:");
	shell_print_dec(nid);
	shell_send_string(" ");
	shell_send_string(err);
	shell_send_string("\r\n");
}

static void next_cmd(void);

static int handle_g_command(gcode_frame_t *frame)
{
	gcode_cmd_t *cmds = frame->cmds;
	int ncmds = frame->num;
	int nid = -1;

	// skip line number(s)
	while (ncmds > 0 && cmds[0].type == 'N') {
		nid = cmds[0].val_i;
		ncmds--;
		cmds++;
	}

	if (nid == -1)
	{
		send_error(-1, "No command number specified");
		return -E_INCORRECT;
	}
	if (ncmds == 0)
	{
		send_ok(nid);
		return -E_OK;
	}
	// parse command line
	switch (cmds[0].type) {
	case 'G':
		switch (cmds[0].val_i) {
		case 0:
		case 1: {
			int i;
			int32_t f = 0, feed0 = 0, feed1 = 0, acc = def.acc_default;
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
				case 'T':
					acc = cmds[i].val_i;
					break;
				}
			}

			int res = planner_line_to(x, NULL, f, feed0, feed1, acc, nid);
			if (res >= 0)
			{
				return -E_OK;
			}
			else
			{
				send_error(nid, "problem with planning line");
				return res;
			}
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

			planner_find_begin(rx, ry, rz, nid);
			return -E_OK;
		}
		case 30:
			planner_z_probe(nid);
			return -E_OK;
		default:
			send_error(nid, "unknown command");
			return -E_INCORRECT;
		}
		break;
	case 'M':
		switch (cmds[0].val_i) {
		case 114:
			print_position(nid);
			return -E_OK;
		case 119:
			print_endstops(nid);
			return -E_OK;
		default:
			send_error(nid, "unknown command");
			return -E_INCORRECT;
		}
		break;
	default:
		send_error(nid, "unknown command");
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

