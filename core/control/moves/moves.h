#pragma once

#include <stdint.h>

#include "steppers.h"
#include "line.h"
#include "arc.h"

void moves_init(steppers_definition *definition);
void moves_reset(void);
void moves_break(void);

int moves_line_to(line_plan *plan);
int moves_arc_to(arc_plan *plan);
int moves_step_tick(void);

cnc_endstops moves_get_endstops(void);

