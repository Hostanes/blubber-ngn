
```
  A mech fps shooter written in C and using raylib for rendering.
  Built to showcase data oriented design in a standard gaming context
```

# Blubber ngn

A work in progress game engine and demonstration FPS game. Built to test the effectiveness of data oriented style programming in a gaming context.
The focus of this work is the Entity Component System architecture. Which prioritizes contiguous memory arrays and predictable iterations.

## Installation

Prerequisites:

- GCC
- CMake 3.16+
- Raylib 4.0+ installed on your system


```Bash
git clone https://github.com/Hostanes/blubber-ngn.git
cd blubber-ngn

mkdir build
cd build
```

configure using Cmake

```Bash
cmake ..
```

if raylib isnt found automatically you can specify its path manually:

```Bash
cmake -DRAYLIB_INCLUDE_DIR=/path/to/raylib/include \
      -DRAYLIB_LIBRARY=/path/to/libraylib.a ..
```

```Bash
cmake --build .
```

the executable will be in the bin/ directory




# Benchmarks

Both of these Benchmarks were ran on this CPU:
```
AMD Ryzen 7 PRO 5850U with Radeon Graphics 
Clock speed Capped at 3.4 GHz 
16 cores, but in testing limited to 8

CPU cache information:
L1d cache:                               256 KiB (8 instances)
L1i cache:                               256 KiB (8 instances)
L2 cache:                                4 MiB (8 instances)
L3 cache:                                16 MiB (1 instance)
```

---

### Benchmark A

This benchmark evaluates the performance impact of **data-level parallelism (SIMD)** versus **thread-level parallelism (OpenMP)**.

The test scene consists of two archetypes containing entities with `Position`, `Velocity`, both inline. and `Timer` component using an external pool with inline handles. 

- A **Movement System**, which updates positions based on velocity.
- A **Timer System**, which decrements timer values each frame and resets them when reaching zero.

Two implementations are compared:

1. **SIMD Implementation*
    - Uses manual SSE intrinsics (`__m128`)
    - Processes four entities per iteration
    - Timer pool is explicitly memory-aligned
    - Single-threaded execution
2. **OpenMP Implementation**
    - Uses scalar operations
    - Parallelized using `#pragma omp parallel for`
    - Multi-threaded execution
    - No explicit SIMD intrinsics

Both versions use identical entity layouts, archetypes, and spawn logic. The only difference lies in the execution strategy.

The benchmark is executed with the following total entity counts:
- 100,000
- 250,000
- 300,000
- 400,000
- 600,000
- 1,000,000

We run the simulation for 200 warmup iterations followed by 10000 measured iterations. The results are obtained as:
- Mean updates/sec (how many entities' values are updated every second)
- Standard deviation of mean
- Min
- Max
Additionally using `perf` we measure:
- Time elapsed for 1000 iterations
- Cache references
- Cache misses
- L1 cache misses

**Results**

![[Pasted image 20260224033844.png]]
- SIMD provides strong speedups at lower counts (around 2.8 to 3x at 100k to 300k).
- Peak SIMD performance occurs around 300k to 400k entities.
- After 400k, SIMD performance drops sharply.    
- At 1M entities, SIMD becomes slower than scalar.

---

![[Pasted image 20260224033847.png]]
- SIMD consistently outperforms scalar.
- Speedup is strongest between 300k–400k (around 4x).
- After 400k, SIMD performance degrades significantly but remains faster than scalar.

---

![[Pasted image 20260224033852.png]]

- Scalar cache miss rate stays low and stable (around 3 to 4%).
- SIMD shows much higher miss rates (10 to 28%).  
- Cache misses spike sharply at 600k and 1M, corresponding to the large SIMD performance drop.

---

At ~400k entities, total working set size is roughly 15 mB, which fits within the CPUs 16 mB L3 cache. This explains the peak SIMD performance around that point. But this does not explain why performance is still slightly worse when the size is less than 15 mBs or ~400k entities. I have been reading up on how caches and SIMD work in more detail trying to understand but I have not found a clear answer. My current assumptions are that some CPU Instruction or C Compiler optimization works better when L3 is filled exactly.

Overall, SIMD provides substantial acceleration while the workload fits in cache. Once the working set exceeds L3 capacity, memory bandwidth becomes the bottleneck and performance collapses, demonstrating that cache friendly memory is the dominant factor in ECS system scalability.

---

### Benchmark B

This benchmark evaluates the performance tradeoff between two component storage strategies for the `Timer` component:
1. **Pooled Timer Layout** All `Timer` components are stored in a single global component pool and accessed through archetype handles.
2. **Inline Timer Layout** Each archetype stores its own `Timer` array inline, alongside its `Position` and `Velocity` arrays.
The total number of entities remained constant at `1,000,000` entities. What varied is the number of archetypes used. The archetypes are all identical and the entities are equally distributed across each.
The point of this is to isolate the effect of Global Contiguous Storage (External Pool) and Fragmented per archetype storage (Inline smaller array for each archetype)

This Benchmark measures multiple things:
1. Timer system: (Timer decrement every frame)

Every frame this runs as a single contiguous loop over all timers
```
for (i in 0..pool.count)  
	timers[i] -= dt
```

In the inline layout, it runs once per layout
```
for (each archetype)
    for (i in 0..arch.count)
        arch.timers[i] -= dt
```

2. Movement System
for each archetype we run `Position += Velocity * dt`
Position and Velocity are stored inline in both configurations.

We run the simulation for 200 warmup iterations followed by 1000 measured iterations. The results are obtained as:
- Mean updates/sec (how many entities' values are updated every second)
- Standard deviation of mean
- Min
- Max
Additionally using `perf` we measure:
- Time elapsed for 1000 iterastions
- Cache references
- Cache misses
- L1 cache misses

**Results:**

This table shows the ratio of time taken between Pooled and Inline implementations, Pooled is consistently ~2x faster across all archetype counts

| Archetypes | Speedup(Pooled/Inline) |
| ---------- | ---------------------- |
| 2          | 2.38                   |
| 8          | 2.34                   |
| 32         | 2.24                   |
| 256        | 2.13                   |
| 1024       | 2.04                   |

---

This table has the mean Updates/sec in Million Entities/sec as Archetype count increases

| Archetypes | Inline Mean | Pooled Mean |
| ---------- | ----------- | ----------- |
| 2          | 284.62      | 677.34      |
| 8          | 282.16      | 659.22      |
| 32         | 262.43      | 588.49      |
| 256        | 148.01      | 315.34      |
| 1024       | 59.83       | 121.83      |
![[Pasted image 20260224025035.png]]
Both degrade over time (due to the inline velocity and position for both cases). But the Pooled version remains consistently faster.

---

This table shows the total L1 cache misses for each configuration in Million

| Archetypes | Inline_L1_Misses_Millions | Pooled_L1_Misses_Millions |
| ---------- | ------------------------- | ------------------------- |
| 2          | 938.74                    | 627.66                    |
| 8          | 941.99                    | 629.67                    |
| 32         | 953.8                     | 636.35                    |
| 256        | 1067.27                   | 696.51                    |
| 1024       | 1417.78                   | 880.81                    |
![[Pasted image 20260224025743.png]]

Both degrade over time, however Pooled is consistently lower.

