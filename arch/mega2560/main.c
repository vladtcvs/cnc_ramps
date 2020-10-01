#include "config.h"

#ifdef CONFIG_LIBCORE
#include <control/moves/moves.h>
#include <control/planner/planner.h>
#include <control/control.h>
#include <output/output.h>
#include "steppers.h"
#endif

#include <shell.h>

#include "platform.h"

#ifdef CONFIG_LIBCORE
static void init_steppers(void)
{
    gpio_definition gd;

    steppers_definition sd = {
        .steps_per_unit = {
            STEPS_PER_MM,
            STEPS_PER_MM,
            STEPS_PER_MM
        },
        .feed_base = FEED_BASE/60.0,
        .feed_max = FEED_MAX/60.0,
        .es_travel = FEED_ES_TRAVEL/60.0,
        .probe_travel = FEED_PROBE_TRAVEL/60.0,
        .es_precise = FEED_ES_PRECISE/60.0,
        .probe_precise = FEED_PROBE_PRECISE/60.0,
        .size = {
            SIZE_X,
            SIZE_Y,
            SIZE_Z,
        },
        .acc_default = ACC,
    };
    steppers_config(&sd, &gd);
    init_control(&sd, &gd);
}
#endif

/* main */


int main(void)
{
    hardware_setup();

#ifdef CONFIG_LIBCORE
    init_steppers();
    planner_lock();
    moves_reset();
#endif

    shell_add_message("Hello", -1);

    while (true)
    {
#ifdef CONFIG_LIBCORE
        planner_pre_calculate();
#endif
	hardware_loop();
    }

    return 0;
}
