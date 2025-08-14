#ifndef BLINK_API_H
#define BLINK_API_H

int kthread_blink_nodt_set_period(unsigned int ms);
int kthread_blink_nodt_get_period(unsigned int *ms);

int timer_blink_nodt_set_period(unsigned int ms);
int timer_blink_nodt_get_period(unsigned int *ms);

int hrtimer_blink_nodt_set_period(unsigned int ms);
int hrtimer_blink_nodt_get_period(unsigned int *ms);

#endif
