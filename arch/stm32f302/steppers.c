#define STM32F3

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/timer.h>

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "steppers.h"

#define FCPU 72000000UL
#define FTIMER 100000UL
#define PSC ((FCPU) / (FTIMER) - 1)
#define TIMEOUT_TIMER_STEP 1000UL

static bool going = false;

static void steppers_timer_setup(void)
{
    rcc_periph_reset_pulse(RST_TIM2);

    timer_set_prescaler(TIM2, PSC);
    timer_direction_up(TIM2);
    timer_disable_preload(TIM2);
    timer_update_on_overflow(TIM2);
    timer_enable_update_event(TIM2);
    timer_continuous_mode(TIM2);

    timer_set_oc_fast_mode(TIM2, TIM_OC1);
    timer_set_oc_value(TIM2, TIM_OC1, 1);

    nvic_set_priority(NVIC_TIM2_IRQ, 0x00);

    nvic_enable_irq(NVIC_TIM2_IRQ);
    timer_enable_irq(TIM2, TIM_DIER_UIE);
    timer_enable_irq(TIM2, TIM_DIER_CC1IE);

    nvic_set_priority(NVIC_TIM2_IRQ, 6 * 16);
}

void steppers_timer_irq_enable(bool en)
{
    if (en)
    {
        nvic_enable_irq(NVIC_TIM2_IRQ);
    }
    else
    {
        nvic_disable_irq(NVIC_TIM2_IRQ);
    }
}

void steppers_setup(void)
{
    steppers_timer_setup();

    // X - step
    gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO14);
    gpio_set_output_options(GPIOC, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO14);
    // X - dir
    gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO15);
    gpio_set_output_options(GPIOC, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO15);
    
    // Y - step
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO0);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO0);
    // Y - dir
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO1);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO1);
    
    // Z - step
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO2);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO2);
    // Z - dir
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO3);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO3);

    // Enable
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO0);
    gpio_set_output_options(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, GPIO0);
        
    // X - stop
    gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO7);

    // Y - stop
    gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO6);

    // Z - stop
    gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO5);

    // Probe
    gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO8);

    // GPIO Tool 0
    gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO1);
    gpio_set_output_options(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO1);
}

static volatile int moving;

static void set_dir(int i, bool dir)
{
    if (dir) {
        switch (i) {
        case 0:
            gpio_set(GPIOC, GPIO15);
            break;
        case 1:
            gpio_set(GPIOA, GPIO1);
            break;
        case 2:
            gpio_set(GPIOA, GPIO3);
            break;
        }
    } else {
        switch (i) {
        case 0:
            gpio_clear(GPIOC, GPIO15);
            break;
        case 1:
            gpio_clear(GPIOA, GPIO1);
            break;
        case 2:
            gpio_clear(GPIOA, GPIO3);
            break;
        }
    }
}

static void make_step(int i)
{
    switch (i)
    {
    case 0:
        gpio_clear(GPIOC, GPIO14);
        break;
    case 1:
        gpio_clear(GPIOA, GPIO0);
        break;
    case 2:
        gpio_clear(GPIOA, GPIO2);
        break;
    }
}

static void end_step(void)
{
    gpio_set(GPIOC, GPIO14);
    gpio_set(GPIOA, GPIO0);
    gpio_set(GPIOA, GPIO2);
}

static void make_tick(void)
{
    int delay_us = moves_step_tick();
    if (delay_us < 0)
    {
        return;
    }
    int delay = delay_us * FTIMER / 1000000UL;
    if (delay < 3)
        delay = 3;
    timer_set_counter(TIM2, 0);
    timer_set_period(TIM2, delay);
}

void tim2_isr(void)
{
    if (!going)
    {
        TIM_SR(TIM2) &= ~TIM_SR_UIF;
        TIM_SR(TIM2) &= ~TIM_SR_CC1IF;

        timer_disable_counter(TIM2);
        return;
    }
    if (TIM_SR(TIM2) & TIM_SR_UIF) {
        // next step of movement
        // it can set STEP pins active (low)
        TIM_SR(TIM2) &= ~TIM_SR_UIF;
        make_tick();
    }
    else if (TIM_SR(TIM2) & TIM_SR_CC1IF) {
        // set STEP pins not active (low) at the end of STEP
        //        ___
        // ______|   |_______
        //
        //           ^
        //           |
        //           here
        TIM_SR(TIM2) &= ~TIM_SR_CC1IF;
        end_step();
    }
}

static void line_started(void)
{
    // PC13 has LED. Enable it
    gpio_clear(GPIOC, GPIO13);

    // Set initial STEP state
    end_step();
    moving = 1;

    // first tick
    going = true;
    make_tick();
    if (going)
        timer_enable_counter(TIM2);
}

static void line_finished(void)
{
    timer_disable_counter(TIM2);
    going = false;
    
    // disable LED
    gpio_set(GPIOC, GPIO13);

    // initial STEP state
    end_step();
    moving = 0;
}

static void line_error(void)
{
    // temporary do same things as in finished case
    timer_disable_counter(TIM2);
    going = false;
    
    gpio_set(GPIOC, GPIO13);
    end_step();
    moving = 0;
}

static void set_gpio(int id, int on)
{
    switch (id)
    {
    case 0:
        if (on)
            gpio_clear(GPIOB, GPIO1);
        else
            gpio_set(GPIOB, GPIO1);
        break;
    }
}

static cnc_endstops get_stops(void)
{
    cnc_endstops stops = {
        .stop_x  = !(gpio_get(GPIOB, GPIO7)),
        .stop_y  = !(gpio_get(GPIOB, GPIO6)),
        .stop_z  = !(gpio_get(GPIOB, GPIO5)),
        .probe   = !(gpio_get(GPIOB, GPIO8)),
    };

    return stops;
}

static void reboot(void)
{
    SCB_AIRCR = (0x5FA<<16)|(1 << 2);
    for (;;)
        ;
}

void steppers_config(steppers_definition *sd, gpio_definition *gd)
{
    sd->reboot         = reboot;
    sd->set_dir        = set_dir;
    sd->make_step      = make_step;
    sd->get_endstops   = get_stops;
    sd->line_started   = line_started;
    sd->line_finished  = line_finished;
    sd->line_error     = line_error;
    gd->set_gpio       = set_gpio;
}

