# BICTR Channel Model
This document is used documentation and implementation notes for BICTR.

## Implementation
BICTR is defined similarly to a library. There is a main header file, `bictr.h`. All definitions, functions, and structures have `bictr` prepended to avoid namespace collisions. Public functions are prepended with `bictr` and are meant to be used in the OAI integration. Functions with `_bictr` prepended are private functions meant to be used by BICTR. They have also been included in the header file for consistency and documentation.

## Ensuring Output Matches Python
During implementation of BICTR testing was done to verify that the output matches that of the Python implementation.

Due to the use of random number generators in BICTR, ensuring that the output matches that of python is a little challenging. This was the main reason to use an additional library to implement the Mersenne Twister algorithm, the same one that Python uses. To set the seed of the PRNG, copy the state from python to BICTR.

```python
# Get the state array from Python
import random
random.seed(0)
print(random.getstate()[1][:-1])

> (2147483648, 766982754, 497961170, ...)
```

```c
// Set the state array in C
uint32_t mt[] = {2147483648, 766982754,  497961170, ...}
memcpy(desc.prng_state.mt, mt, sizeof(uint32_t) * STATE_VECTOR_LENGTH);
```

## OAI Integration
`random_channel.c:new_channel_desc_scm()` seems to be called whenever the channel parameters are changed, as suggested by looking at `simulator.c:rfsimu_setchanmod_cmd()`. Furthermore, `random_channel.c:random_channel()` is called right after. Based on these two functions, BICTR's parameters should be defined in the former, and the channel creation should be performed in the lather.

In `random_channel.c:new_channel_desc_scm()`, the `channel_desc_t` struct is populated. This operation should be quick it seems. This function also calls `random_channel.c:fill_channel_desc()`, which allocates the memory for the wireless channel. But this is problematic as the size of the FIR filter is unknown till the channel is generated in `random_channel.c:random_channel()`. To work around this, the FIR filter should be big enough to hold the longest theoretical situation. `channel_length` and `nb_taps` should be set at the maximum delay spread in the channel descriptor. 

The theoretical maximum delay spread is the time difference from the shortest path (LOS) to the longest path. This should be defined as the time it takes to travel to the furthest possible reflector.
$$
\mathrm{MaxDelaySpread=2(\mathrm{MaxRingRadius}+\mathrm{RingRadiusUncertainty})}
$$

After the channel descriptor is defined, the wireless channel can be defined in `random_channel.c:random_channel()`. The length of the impulse response can be reduced from the maximum to the correct length by modifying `channel_length`. This change will automatically be applied in `apply_channelmod.c:rxAddInput()`. This change should not cause a memory leak as `random_channel.c:free_channel_desc_scm()` does not depend on `channel_length`. Additionally, the `channel_offset` can be set to perform the LOS optimization.

## Building
Additional library will need to be installed, mainly GMT.
```sh
sudo apt install gmt gmt-dcw gmt-gshhg libgmt-dev
```