#define _GNU_SOURCE
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>

#include "types.h"

#define YTIMER_OVERHEAD_ITERATIONS 10000000

const ascii logo[] =
  "      _   _\n"
  " _  _| |_(_)_ __  ___ _ _ ___\n"
  "| || |  _| | '  \\/ -_) '_(_-<\n"
  " \\_, |\\__|_|_|_|_\\___|_| /__/\n"
  " |__/\n";
  
__attribute__ ((noinline)) void spinner(u64 cycles)
{
  __asm__ volatile(
		   "loop:;\n"
		   "dec %[counter];\n"
		   "jnz loop;\n"
		   
		   : [counter] "+r" (cycles));
}

static inline f64 timer_nanos()
{
  struct timespec t;

  clock_gettime(CLOCK_MONOTONIC_RAW, &t);

  return t.tv_nsec;
}

static inline u64 rdtsc()
{
  u64 a = 0, d = 0;
  
  __asm__ volatile ("rdtsc" : "=a" (a), "=d" (d) : : "%rbx", "%rcx");

  return ((d << 32) | a);
}

static inline u64 rdtscp()
{
  u64 a = 0, d = 0;
  
  __asm__ volatile ("rdtscp" : "=a" (a), "=d" (d) : : "%rbx", "%rcx");

  return ((d << 32) | a);
}

static inline void timer_cycles_sync()
{
  __asm__ volatile ("cpuid" : : : "%rax", "%rbx", "%rcx", "%rdx");
}

static inline f64 timer_cycles_start()
{
  timer_cycles_sync();

  return (f64)rdtsc();
}

static inline f64 timer_cycles_stop()
{
  f64 t = (f64)rdtscp();
  
  timer_cycles_sync();
  
  return t;
}

f64 timer_nanos_overhead()
{
  f64 o = 0.0;
  f64 d = 0.0;
  f64 t0 = 0.0;
  f64 t1 = 0.0;
  f64 garbage __attribute__((unused)) = 0.0;
  
  for (u64 i = 0; i < YTIMER_OVERHEAD_ITERATIONS; i++)
    {
      do
	{
	  t0 = timer_nanos();
	  
	  garbage = timer_nanos();

	  t1 = timer_nanos();

	  d = t1 - t0;
	}
      while (d <= 0.0);

      if (i)
	{
	  if (d < o)
	  o = d;
	}
      else
	o = d;
    }

  return o;
}

f64 timer_cycles_overhead()
{
  f64 o = 0.0;
  f64 d = 0.0;
  f64 t0 = 0.0;
  f64 t1 = 0.0;

  for (u64 i = 0; i < YTIMER_OVERHEAD_ITERATIONS; i++)
    {
      t0 = timer_cycles_start();
      __asm__ volatile ("");
      t1 = timer_cycles_stop();

      d = t1 - t0;
      
      if (i)
	{
	  if (d < o)
	    o = d;
	}
      else
	o = d;
    }
  
  return o;
}

f64 measure_cycles(u64 cycles)
{
  f64 t0 = 0.0;
  f64 t1 = 0.0;
  
  t0 = timer_cycles_start();
  
  //Adjust cycles to 'dec' instruction throughput on the target architecture.
  //On Alderlake N95, 'dec' has a latency of 1 cycle, and a reciprocal throughput of 0.5.
  //This implies that the CPU can run 1 / 0.5 = 2 instructions in 1 cycle.
  //Therefore, for a requested 1000 cycles, the kernel needs to spin 2000 times to cover
  //for the throughput and allow for a valid measurement.
  spinner(cycles << 1);
  
  t1 = timer_cycles_stop();
  
  return (t1 - t0);
}

f64 measure_frequency_with_nanos()
{  
  f64 t0 = 0.0;
  f64 t1 = 0.0;
  f64 e1 = 0.0;
  f64 e2 = 0.0;
  
  t0 = timer_nanos();

  spinner(100000 << 1);

  t1 = timer_nanos();
  
  e1 = t1 - t0;

  t0 = timer_nanos();
  
  spinner(100000);
  
  t1 = timer_nanos();
  
  e2 = t1 - t0;

  f64 e = (e1 - e2);
  
  return (f64)(100000) / e;
}

i32 main(i32 argc, ascii **argv)
{
  printf("%s\tytimers - version 1.0 - 2016\n\n", logo);
  
  if (argc < 2)
    return printf("Usage: %s [cycles]\n", argv[0]), 1;

  printf("\n### WARNING: make sure you know which CPU architecture, frequency driver, and governor are set before interpreting the results!\n\n");
  
  u64 cycles = (u64)atoll(argv[1]);

  if (!cycles)
    return printf("ERROR: cycles must not be 0\n"), 1;
  else
    if (cycles < 1000)
      printf("### WARNING: bad TSC precision below 1000 cycles\n\n");
  
  f64 overhead_nanos  = timer_nanos_overhead();
  f64 overhead_cycles = timer_cycles_overhead();
  
  printf("# Overhead evaluation:\n");
  printf("nanos timer overhead: %.3lf nanos\n", overhead_nanos); 
  printf("TSC   timer overhead: %.3lf cycles\n", overhead_cycles);

  printf("\n# Cycles timer accuracy evaluation:\n");
  f64 measured_cycles = measure_cycles(cycles);
  f64 adjusted_measured_cycles = measured_cycles - (2.0 * overhead_cycles);
  f64 cycles_skew = fabs(adjusted_measured_cycles - cycles);
  
  printf("\tRequested\t: %llu cycles\n\tMeasured raw\t: %.3lf cycles\n\tMeasured adj.\t: %.3lf\n\tSkew\t\t: %.3lf (%.3lf %%)\n",
	 cycles,
	 measured_cycles,
	 adjusted_measured_cycles,
	 cycles_skew,
	 (cycles_skew * 100.0) / cycles);
  
  f64 average_frequency = 0.0;
  
  for (u64 i = 0; i < 10000; i++)
    average_frequency += measure_frequency_with_nanos();
  
  average_frequency /= 10000.0;
  
  printf("\n# Measured avg. CPU frequency: %.3lf GHz\n", average_frequency);
  
  return 0;
}
