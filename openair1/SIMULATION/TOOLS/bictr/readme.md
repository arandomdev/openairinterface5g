# BICTR Channel Model
This document is used documentation and implementation notes for BICTR.

## Implementation Notes
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