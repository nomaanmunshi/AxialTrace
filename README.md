# AxialTrace

A fixed-step C++17 simulation of a single linear axis. Command a move, jam the actuator or freeze its encoder, then inspect the controller decision on each tick.

## Build and run

From the project directory on macOS or Linux, with a C++17 compiler, CMake 3.16+ and Make installed:

```sh
sh demo.sh
```

The first build downloads checksum-pinned GoogleTest 1.15.2. The script runs the tests and writes six CSV files under `build/traces/`. To inspect a single run, use `build/axialtrace jam`.

## Day 1 baseline

Each tick represents 10 ms. Position uses integer encoder counts, velocity uses counts per tick. A CSV row records position and velocity **before** applying that row's command. A zero command requests deceleration. The simulated hard stops constrain position to [0, 100].

| Scenario | Observed result |
| --- | --- |
| Forward | Position reaches 31 on tick 31 while moving toward 80. |
| Reversal | Target switches to 0 on tick 12. Velocity falls from +1 through 0 to −1. |
| Arrival | Position reaches 12 and holds. |
| Travel limits | From 95, the axis reaches 100. A separate actuator test drives into both hard stops. |
| Jam | Position and encoder hold at 7. Controller latches stop on tick 11. |
| Frozen encoder | Encoder holds at 7. Controller latches stop on tick 11. Physical position reaches 11. |

The actuator jams during interval 7. In the frozen case the encoder holds its tick-7 sample for subsequent reads. The controller sees the same four unchanged readings following nonzero commands, starting on tick 8. Its stop flag reports loss of measured motion. The CSV position column is available to the observer only.

## Scope

This baseline uses a unit-speed position controller and an acceleration-limited actuator. The velocity model has no load torque, backlash or sub-count motion. Independent CSV replay, a deliberately late-stop fixture and slow-motion encoder quantization are the next sprint steps. The two fault traces alone cannot establish that the controller met its stop deadline independently.
