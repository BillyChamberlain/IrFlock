# #118 Use a median of 5 for the sonar

Issue: https://github.com/BillyChamberlain/IrFlock/issues/118

## Why

Median handles spikes better than an average.

## Done when

- [ ] Sonar distance is the median of the last 5 readings.

## Start here

- `bench/sensor_eyes/sensor_eyes.ino`
