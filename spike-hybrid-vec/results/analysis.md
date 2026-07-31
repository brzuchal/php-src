## Q1: one append onto a 100k flat base (original retained, per elem)

| elem | repr | median/append | zval copies/op | allocs/op | bytes/op |
|---|---|---|---|---|---|
| int | A | 115.7us | 100001.0 | 1.00 | 1.5MB |
| int | B | 115.1us | 100001.0 | 1.00 | 2.0MB |
| int | C c16 | 11.4ns | 1.0 | 4.00 | 352B |
| int | C c64 | 10.6ns | 1.0 | 4.00 | 1.1KB |
| int | C c128 | 10.4ns | 1.0 | 4.00 | 2.1KB |
| string | A | 90.3us | 100001.0 | 1.00 | 1.5MB |
| string | B | 151.4us | 100001.0 | 1.00 | 2.0MB |
| string | C c16 | 11.6ns | 1.0 | 4.00 | 352B |
| string | C c64 | 11.1ns | 1.0 | 4.00 | 1.1KB |
| string | C c128 | 11.9ns | 1.0 | 4.00 | 2.1KB |

## Q1b: base scaling, appended=1, int (median ns/append)

| base | A | B | C c64 | A/C ratio |
|---|---|---|---|---|
| 100 | 44.2ns | 46.2ns | 10.7ns | 4x |
| 1000 | 481.7ns | 650.6ns | 10.6ns | 45x |
| 10000 | 7.2us | 10.0us | 11.3ns | 637x |
| 100000 | 115.7us | 115.1us | 10.6ns | 10922x |

## Q1c: amortization — base=100k int, varying appended (ns/append)

| appended | A | B | C c16 | C c64 | C c128 |
|---|---|---|---|---|---|
| 1 | 115.7us | 115.1us | 11.4ns | 10.6ns | 10.4ns |
| 4 | 194.5us | 29.2us | 4.4ns | 4.4ns | 4.3ns |
| 16 | 156.5us | 7.2us | 3.0ns | 2.8ns | 2.8ns |
| 32 | 146.2us | 3.6us | 3.1ns | 2.6ns | 2.6ns |
| 64 | 146.7us | 1.8us | 3.1ns | 2.6ns | 2.5ns |
| 1024 | 148.3us | 115.1ns | 3.1ns | 2.7ns | 2.7ns |

## Q2: branch cost by chunk and receiver position (int)

| chunk | lv_pos | lv | median | p95 | zcopies/op (bytes) | blk_addref/op | allocs/op |
|---|---|---|---|---|---|---|---|
| 16 | half | 8 | 21.5ns | 21.6ns | 9 (144B) | 4.0 | 4.0 |
| 16 | worst | 15 | 23.6ns | 25.5ns | 16 (256B) | 4.0 | 4.0 |
| 16 | boundary | 16 | 15.6ns | 16.3ns | 1 (16B) | 5.0 | 4.0 |
| 32 | half | 16 | 23.8ns | 24.1ns | 17 (272B) | 4.0 | 4.0 |
| 32 | worst | 31 | 31.9ns | 32.2ns | 32 (512B) | 4.0 | 4.0 |
| 32 | boundary | 32 | 16.0ns | 16.3ns | 1 (16B) | 5.0 | 4.0 |
| 64 | half | 32 | 29.5ns | 30.1ns | 33 (528B) | 4.0 | 4.0 |
| 64 | worst | 63 | 41.6ns | 41.7ns | 64 (1.0KB) | 4.0 | 4.0 |
| 64 | boundary | 64 | 15.5ns | 17.0ns | 1 (16B) | 5.0 | 4.0 |
| 128 | half | 64 | 61.3ns | 63.2ns | 65 (1.0KB) | 4.0 | 4.0 |
| 128 | worst | 127 | 84.1ns | 93.1ns | 128 (2.0KB) | 4.0 | 4.0 |
| 128 | boundary | 128 | 15.8ns | 15.9ns | 1 (16B) | 5.0 | 4.0 |

A/B comparison rows (append to shared older version = full copy):

| repr | chunk-context | elem | median | zcopies/op |
|---|---|---|---|---|
| A | shift 4 group | int | 452.1ns | 1097 |
| A | shift 4 group | int | 639.8ns | 1601 |
| A | shift 4 group | string | 1.4us | 1601 |
| A | shift 4 group | string | 617.1ns | 1097 |

## Q3: indexed read, base=10000 + 8 blocks (int), ns/read

| repr/chunk | base region | full blocks | last block | mixed |
|---|---|---|---|---|
| A | 0.51 | - | - | 0.51 |
| B | 0.49 | - | - | 0.47 |
| C c16 | 0.57 | 0.65 | 0.65 | 0.62 |
| C c32 | 0.56 | 0.65 | 0.65 | 0.67 |
| C c64 | 0.58 | 0.65 | 0.61 | 0.72 |
| C c128 | 0.61 | 0.65 | 0.61 | 0.69 |

## Q4: foreach ns/element, base=10000 (int)

| blocks | C c16 | C c32 | C c64 | C c128 | A same-total (c64 sizes) |
|---|---|---|---|---|---|
| 0 | 0.531 | 0.495 | 0.495 | 0.495 | 0.494 |
| 1 | 0.494 | 0.531 | 0.496 | 0.532 | 0.494 |
| 8 | 0.532 | 0.497 | 0.500 | 0.540 | 0.494 |
| 64 | 0.503 | 0.542 | 0.559 | 0.521 | 0.530 |
| 1024 | 0.552 | 0.568 | 0.605 | 0.582 | 0.529 |

A flat baselines by total: 18192=0.494 10128=0.494 10256=0.495 75536=0.529 10064=0.494 11024=0.494 26384=0.494 10512=0.494 10016=0.494 141072=0.529 14096=0.530 42768=0.494 12048=0.530 10032=0.495 10000=0.494 

## Q5: invisible retention (string elems, base=1000, tip=40 blocks, snapshot at ~10.5 blocks)

| chunk | snap_at | visible | invisible slots | bound | blocks live | spine arr bytes | live bytes @snap | live bytes @tip |
|---|---|---|---|---|---|---|---|---|
| 128 | 1281 | 2281 | 127 | 128 | 11 | 512B | 38.3KB | 96.5KB |
| 32 | 336 | 1336 | 16 | 32 | 11 | 512B | 21.8KB | 36.5KB |
| 16 | 161 | 1161 | 15 | 16 | 11 | 512B | 19.0KB | 26.5KB |
| 16 | 168 | 1168 | 8 | 16 | 11 | 512B | 19.0KB | 26.5KB |
| 32 | 321 | 1321 | 31 | 32 | 11 | 512B | 21.8KB | 36.5KB |
| 64 | 641 | 1641 | 63 | 64 | 11 | 512B | 27.3KB | 56.5KB |
| 64 | 672 | 1672 | 32 | 64 | 11 | 512B | 27.3KB | 56.5KB |
| 128 | 1344 | 2344 | 64 | 128 | 11 | 512B | 38.3KB | 96.5KB |

## Q6/Q8: version materialization + destroy vs visible blocks (boundary branch, int)

| vb | c16 create | c16 destroy | c64 create | c64 destroy | c128 create | c128 destroy |
|---|---|---|---|---|---|---|
| 1 | 13.5ns | 12.5ns | 14.9ns | 14.3ns | 13.5ns | 12.6ns |
| 8 | 17.8ns | 16.3ns | 17.7ns | 16.2ns | 17.6ns | 16.2ns |
| 64 | 41.9ns | 49.8ns | 48.5ns | 51.7ns | 47.7ns | 52.5ns |
| 1024 | 567.7ns | 698.3ns | 1.2us | 1.1us | 2.3us | 1.2us |

## Q7: linear append, intermediates dead (ns/append)

| elem | n | A | B | C c16 | C c32 | C c64 | C c128 |
|---|---|---|---|---|---|---|---|
| int | 1024 | 746.8ns | 2.0ns | 2.5ns | 2.6ns | 2.4ns | 2.4ns |
| int | 4096 | 2.9us | 2.1ns | 2.5ns | 2.7ns | 2.4ns | 2.3ns |
| int | 65536 | - | 2.1ns | 3.0ns | 3.0ns | 2.7ns | 2.6ns |
| string | 1024 | 841.5ns | 2.2ns | 2.7ns | 2.8ns | 2.6ns | 2.6ns |
| string | 4096 | 4.0us | 2.4ns | 3.1ns | 3.2ns | 3.1ns | 3.3ns |
| string | 65536 | - | 2.5ns | 3.6ns | 4.1ns | 4.2ns | 3.8ns |
| object | 1024 | 1.3us | 2.2ns | 2.7ns | 2.8ns | 2.6ns | 2.6ns |
| object | 4096 | 6.2us | 2.4ns | 3.0ns | 3.3ns | 2.9ns | 3.6ns |
| object | 65536 | - | 2.4ns | 3.5ns | 4.3ns | 4.2ns | 3.3ns |
| array | 1024 | 1.4us | 2.3ns | 2.8ns | 2.8ns | 2.7ns | 2.6ns |
| array | 4096 | 6.1us | 2.5ns | 3.0ns | 3.2ns | 3.0ns | 3.1ns |
| array | 65536 | - | 2.7ns | 3.1ns | 3.5ns | 3.8ns | 3.1ns |
| nested | 1024 | 1.3us | 2.5ns | 3.0ns | 3.0ns | 2.9ns | 2.9ns |
| nested | 4096 | 6.9us | 2.9ns | 3.2ns | 3.2ns | 3.2ns | 3.4ns |
| nested | 65536 | - | 3.0ns | 3.3ns | 3.5ns | 3.4ns | 3.4ns |

## W2: linear append, all versions retained (ns/append)

| elem | n | A | B | C c16 | C c32 | C c64 | C c128 |
|---|---|---|---|---|---|---|---|
| int | 1024 | 631.1ns | 801.2ns | 15.1ns | 10.2ns | 7.6ns | 6.3ns |
| int | 4096 | 2.7us | 3.4us | 48.6ns | 24.6ns | 20.7ns | 11.9ns |
| int | 16384 | - | - | 163.3ns | 89.5ns | 74.9ns | 39.8ns |
| int | 65536 | - | - | 1.2us | 588.7ns | 462.7ns | 265.5ns |
| string | 1024 | 713.2ns | 943.3ns | 15.2ns | 10.1ns | 8.0ns | 6.6ns |
| string | 4096 | 3.6us | 4.2us | 46.0ns | 26.8ns | 21.1ns | 12.3ns |
| string | 16384 | - | - | 164.1ns | 86.0ns | 75.3ns | 40.0ns |
| object | 1024 | 732.4ns | 896.9ns | 15.4ns | 10.2ns | 8.1ns | 6.6ns |
| object | 4096 | 3.4us | 4.2us | 49.0ns | 25.1ns | 21.1ns | 12.3ns |
| object | 16384 | - | - | 163.9ns | 88.9ns | 75.7ns | 40.0ns |

Retained memory at build (mem_build - mem_start), int:

| n | A | B | C c16 | C c64 | C c128 |
|---|---|---|---|---|---|
| 1024 | 9.6MB | 14.2MB | 76.6KB | 76.2KB | 76.1KB |
| 4096 | 135.7MB | 186.3MB | 306.1KB | 304.6KB | 304.3KB |
| 16384 | - | - | 1.2MB | 1.2MB | 1.2MB |
| 65536 | - | - | 4.8MB | 4.8MB | 4.8MB |

## W8: destruction (ns/element; retained = ns per element incl. version chain)

| mode | elem | A | B | C c16 | C c128 |
|---|---|---|---|---|---|
| single | int | 1.0ns | 1.0ns | 1.1ns | 1.0ns |
| single | string | 1.1ns | 1.0ns | 1.2ns | 1.1ns |
| single | object | 2.1ns | 2.0ns | 2.1ns | 2.0ns |
| single | array | 2.0ns | 2.0ns | 2.1ns | 2.0ns |
| single | nested | 2.0ns | 2.0ns | 2.2ns | 2.0ns |
| chain | int | 1.1us | 1.2us | 289.4ns | 41.1ns |
| chain | string | 1.2us | 1.3us | 263.4ns | 47.0ns |

## W10: forks (base=10000, per=8 appends per fork, int; ns/fork create, destroy)

| forks | C create | C destroy | C live bytes | A create | A live bytes |
|---|---|---|---|---|---|
| 1 | 42.0ns | 42.0ns | 158.4KB | 110.1us | 313.6KB |
| 4 | 52.0ns | 52.0ns | 161.7KB | - | - |
| 16 | 44.2ns | 52.1ns | 174.9KB | 109.2us | 2.6MB |
| 64 | 44.9ns | 51.4ns | 227.4KB | - | - |
| 256 | 45.4ns | 51.8ns | 437.4KB | - | - |

## W10b: branch depth (chained branches, int, c64; ns/level)

| depth | keep_all | median/level | live bytes at depth |
|---|---|---|---|
| 256 | 0 | 94.9ns | 21.2KB |
| 256 | 1 | 46.1ns | 297.0KB |
| 1024 | 0 | 107.2ns | 33.4KB |
| 16 | 0 | 98.9ns | 17.1KB |

## Sanity: peak memory, linear dead n=65536 int

B: peak-start=2.5MB build-start=2.0MB
C c16: peak-start=2.3MB build-start=2.3MB
C c128: peak-start=2.3MB build-start=2.3MB
