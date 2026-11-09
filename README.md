# AxialTrace

A deterministic C++17 simulator for a single robotic feed axis. Inject a mechanical jam or frozen encoder, export a CSV trace, then check the controller's stop deadline with an independent replay verifier.

## Run

Requires CMake 3.16+, a C++17 compiler and Make on macOS or Linux. The first configuration downloads checksum-pinned GoogleTest 1.15.2.

```sh
sh demo.sh
```

This builds the simulator, runs GoogleTest, generates seven scenarios under `build/traces/`, verifies each trace and rejects a deliberately faulty fixture. The final rejection message is expected. The script exits successfully only when every check behaves as specified.

```sh
build/axialtrace jam --fault-tick 20 --ticks 64 > build/traces/jam20.csv
build/axialtrace verify build/traces/jam20.csv
build/axialtrace verify tests/fixtures/delayed_controller.csv
```

The last command exits 1 and includes:

```text
tick=12 expected_stop=12 observed_stop=13 stop flag missing
tick=12 expected_stop=12 observed_stop=13 nonzero command after stop deadline
```

Exit codes are 0 for a passing replay, 1 for a requirement violation and 2 for invalid arguments, malformed input or an I/O failure. An observed stop of -1 means no stop flag appeared within the recorded window.

## Timing and units

The simulation advances exactly 10 ms per tick. Position and velocity use integer subcounts, with 1,000 subcounts per encoder count. Velocity and command units are subcounts per tick. Target and encoder columns use whole counts.

The actuator changes velocity by at most 1,000 subcounts per tick and clamps position to [0, 100,000]. The encoder rounds to the nearest count. The controller receives the target and encoder reading only. The runner records physical position separately for analysis.

Each row describes the state at the start of tick k. Its command applies over interval [k, k+1). The next row contains the resulting measurement. A jam beginning at tick F prevents movement over interval F. A frozen encoder keeps the sample read at F and holds it on later reads. A zero command requests deceleration. The stop flag latches a feedback fault.

## Stop requirement

For every measurement, inspect the preceding interval. If its command was nonzero and the encoder reading has stayed unchanged, count that interval as quiet. Any measured movement or preceding zero command resets the quiet interval.

After 12 consecutive quiet intervals, the controller must set its stop flag and command zero on that same tick. The flag must stay set for the remainder of the run. For the supplied jam and freeze runs, the deadline is F + 12.

The slowest supported speed is 100 subcounts per tick. One full encoder count takes 10 ticks at that speed. A 12-tick observation window allows rounded slow movement to produce a new reading before a feedback fault is declared. This window is 120 ms. It requires reassessment if the minimum speed, encoder resolution, tick period or acceleration changes.

## Independent replay

The replay library has no dependency on the motion model or controller. It retains only tick, target, encoder, command and stop flag. Physical columns are checked for integer syntax, then discarded.

The verifier derives the deadline from command history and encoder changes using an elapsed interval calculation. It compares that deadline with each recorded command and flag, reporting the violating tick and the first observed stop tick.

Version 2 CSV files declare the tick period and quiet limit in a metadata line. Original Day 1 files without metadata remain readable with their original four-tick rule. Policy metadata is trusted input. The verifier cannot establish that a truncated trace includes a later fault deadline, prove the physical cause of lost feedback, or detect tampering with the declared policy.

Tests cover both saved faulty traces and a live experiment that delays the current controller's published stop flag and zero command by one tick. Production controller behavior stays unchanged during that experiment.

## Scenarios

Default runs contain 64 ticks and inject faults at tick 7.

| Scenario | Result |
| --- | --- |
| forward | Encoder reaches 63 at tick 63 while moving toward 80. |
| reversal | Target switches to 0 at tick 12. Velocity passes through zero before becoming negative. |
| arrival | Encoder and physical position reach 12 counts and hold. |
| limits | Starting at 95 counts, reaches the upper hard stop. Unit coverage drives into both stops. |
| jam | Position stays at 7 counts. Stop requested at tick 19. |
| frozen | Encoder stays at 7 counts. Physical position reaches 19 counts. Stop requested at tick 19. |
| slow | Moves at 0.1 count per tick with rounded readings. Reaches encoder count 2 without a feedback fault. |

Jam and frozen cases produce identical controller observations. Ground truth explains the difference to an observer. The controller reports loss of measured motion and cannot distinguish the physical cause.

## Engineering milestone

The agreed 75% milestone includes the build, separate components, seven reproducible scenarios, independent replay, a one-tick-late regression fixture, slow-motion coverage and configurable fault timing. The percentage is a scope checkpoint.

Remaining work is the jam/freeze comparison figure, broader edge-case review, a recorded terminal walkthrough and a reviewed v0.1.0 release. Tomorrow's session can begin by changing fault timing and explaining why the replay deadline moves.

## Modeling limits

This is a kinematic model with bounded integer velocity. It has no mass, torque, backlash or communication delay. Targets are whole counts in [0, 100], and the controller's speed range is [100, 1,000] subcounts per tick. Arrival is judged by the rounded encoder, so slow movement may stop within half a count of the physical target. A latched fault requires a new controller instance to reset.

Identical arguments produce identical CSV bytes within this integer model. Tests exercise every scenario twice. These checks establish behavior for this simulator and its declared rule.
