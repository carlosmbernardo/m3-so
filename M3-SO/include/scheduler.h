#pragma once

typedef int (*sched_algo_t)(void);

void scheduler_set_algorithm(sched_algo_t algo);
void scheduler_start(void);
void yield(void);