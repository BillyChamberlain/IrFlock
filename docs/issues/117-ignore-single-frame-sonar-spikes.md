# #117 Ignore single-frame sonar spikes

Issue: https://github.com/BillyChamberlain/IrFlock/issues/117

## Why

One wild reading jumps the state for a frame.

## Done when

- [ ] A single bad reading doesn't change the state.

## Start here

- `bench/sensor_eyes/sensor_eyes.ino`
