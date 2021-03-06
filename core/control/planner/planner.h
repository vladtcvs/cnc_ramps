#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <control/moves/moves.h>
#include <control/tools/tools.h>
#include <control/moves/moves_line/line.h>
#include <control/moves/moves_arc/arc.h>

int empty_slots(void);

void init_planner(steppers_definition *pd,
		  gpio_definition *gd,
                  void (*arg_send_queued)(int nid),
                  void (*arg_send_started)(int nid),
                  void (*arg_send_completed)(int nid),
                  void (*arg_send_completed_with_pos)(int nid, const int32_t *pos),
                  void (*arg_send_dropped)(int nid),
		  void (*arg_send_failed)(int nid));

int planner_line_to(int32_t x[3], double feed, double f0, double f1, int32_t acc, int nid);

int planner_arc_to(int32_t x1[2], int32_t x2[2], int32_t H, double len, double a, double b, arc_plane plane, int cw,
		   double feed, double f0, double f1, int32_t acc, int nid);

int planner_tool(int id, bool on, int nid);

void planner_pre_calculate(void);

void enable_break_on_probe(bool en);

void planner_unlock(void);

void planner_lock(void);

int planner_is_locked(void);

void planner_fail_on_endstops(bool fail);

void planner_report_states(void);

