# Validation notes

## Catch-up session 1

Added a configurable speed and expanded the tests around encoder rounding, quiet-window resets, faults after arrival, and stop latching. A regression probe against the previous actuator found signed overflow when reversing from velocity 1,000 with the minimum integer command. The actuator now widens arithmetic before applying acceleration and position bounds.

The slow fault case needs care. At speed 100, the last encoder change is tick 5. A jam starting at tick 7 therefore reaches twelve quiet readings on tick 17. The controller has no access to the injected fault time.

## Catch-up session 2

Added paired 32-tick traces, a figure drawn from those traces, and a plotting tool that checks matching controller observations. The CLI integration test regenerates both committed examples and compares their bytes.

## Observed runs

Physical positions below are expressed in encoder counts. A dash means the fault flag was never set inside the recorded window.

| Scenario | Speed (subcounts/tick) | Fault tick | Recorded ticks | First stop tick | Final encoder | Final physical position |
| --- | --- | --- | --- | --- | --- | --- |
| jam | 1000 | 0 | 32 | 12 | 0 | 0 |
| frozen | 1000 | 0 | 32 | 12 | 0 | 12 |
| jam | 1000 | 7 | 32 | 19 | 7 | 7 |
| frozen | 1000 | 7 | 32 | 19 | 7 | 19 |
| jam | 1000 | 20 | 64 | 32 | 20 | 20 |
| frozen | 1000 | 20 | 64 | 32 | 20 | 32 |
| jam | 100 | 7 | 32 | 17 | 1 | 0.7 |
| frozen | 100 | 7 | 32 | 17 | 1 | 1.7 |
| jam | 1000 | 51 | 64 | 63 | 51 | 51 |
| jam | 1000 | 52 | 64 | — | 52 | 52 |
| frozen | 1000 | 63 | 64 | — | 63 | 63 |
| jam | 1000 | 90 | 128 | — | 80 | 80 |

The fault at tick 51 stops on the final recorded tick, 63. For a fault at tick 52, its default-speed deadline is tick 64 and lies beyond the recording. The tick-90 fault occurs after arrival at target 80, so no feedback fault is expected.

## Verification

- 30 GoogleTest cases plus one CLI integration check.
- 256 fault runs across both fault types, speeds 100 and 1,000, and start ticks 0 through 63.
- 18,020 combinations of speed, starting encoder phase and direction, with 120 controller updates per combination.
- Independent replay still rejects the one-tick-late fixture with expected tick 12 and observed tick 13.
- The plotting tool reports matching observations, physical divergence starting at tick 8, and a shared stop tick of 19 for the committed examples.

Run the complete build and validation with:

```sh
sh demo.sh
```

## Remaining release work

Run the update on macOS, review the demonstration output, record the terminal walkthrough, then create the reviewed release tag. The verifier checks the recorded window using the policy declared by the trace. Physical modeling and trace authenticity remain outside that check.
