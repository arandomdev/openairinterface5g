# BICTR Channel Model
The Barren, Irregular, Chaotic Terrain Ring model is a purpose built lunar channel model.

This document is used documentation and implementation notes for BICTR.

## Table of Contents
- [BICTR Channel Model](#bictr-channel-model)
  - [Table of Contents](#table-of-contents)
  - [Building](#building)
  - [Running the model](#running-the-model)
  - [Implementation](#implementation)
  - [Ensuring Output Matches Python](#ensuring-output-matches-python)
  - [OAI Integration](#oai-integration)
    - [Assumptions](#assumptions)

## Building
BICTR was implemented to easily integrate into OAI. As such it uses the same build system.

The following instructions were tested on Ubuntu 22.04.

First additional library will need to be installed, mainly GMT and its development files. Additional work can be done to include this step into the `build_oai` script.
```sh
sudo apt install gmt gmt-dcw gmt-gshhg libgmt-dev
```

Then use the OAI build script can be used to build RFSimulator, which will include the BICTR model. Ensure the correct branch is active.
```sh
# Install dependencies and build
./build_oai -I -w SIMU --eNB --gNB --UE --nrUE --ninja
```
When building OAI the example program in `bictr.c` is also built. The executable is called `bictr_sim` and located in the same place as other executables.

Additionally there is a test suite at `./openair1/SIMULATION/TOOLS/bictr/TESTBENCH` with basic unit tests. These unit tests were created by running the equivalent function in the Python library and using the input and output as a test vector for the C library. These unit tests are not comprehensive and only shallowly verify that the C implementation matches the Python implementation. The following snippet demonstrates how to build and run it.
```sh
cd openairinterface5g/openair1/SIMULATION/TOOLS/bictr/TESTBENCH
make
./test_bictr
```

## Running the model
The BICTR library was designed to easily integrate into OAI to generate a channel. First the channel descriptor, `bictr_desc_t`, needs to be defined and initialized with `bictr_initialize()`.

`bictr_desc_t` is split into a user managed section and a library managed section.
```c
typedef struct {
  /* User Managed */
  /// Antenna params
  ...
  /// Ring search params
  ...
  /// Permittivity params
  ...
  /// Rayleigh fading params
  ...
  /// Other parameters
  ...

  /* BICTR library managed, Do not edit*/
  /// Runtime information
  ...
  /// Loaded body information
  ...
  /// Runtime resources
  ...
} bictr_desc_t;
```
Before calling the initialize function, all the user managed fields should be filled. The library managed fields should not be modified.

`bictr_initialize()`, asks for the region to load into memory, which is defined by the SW and NE points. This bounding box should contain both the transmitter and receiver. This function also downloads the required DEM data to `~/.gmt`. In a testing or production setting, additional work can be done to download and prepare that data before running BICTR. Refer to the command `gmt get`.

After initializing the BICTR descriptor, the user managed fields can be modified between runs without reinitializing. Though changes to the TX and RX coordinates should verify that it is still within the loaded region.

Next, the channel can be generated using `bictr_generate_channel()`.

Lastly, `bictr_free()` should be called to release any resources.

Refer to the unit tests as an example of using the library, specifically initialization and cleanup in `main()` and running the model in `test_bictr_generate_channel()`. Additionally, there is experimental integration into OAI and RFSimulator.

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

After the channel descriptor is initialized, the wireless channel can be generated in `random_channel.c:random_channel()`. The length of the impulse response can be reduced from the maximum to the correct length by modifying `channel_length`. This change will automatically be applied in `apply_channelmod.c:rxAddInput()`. This change should not cause a memory leak as `random_channel.c:free_channel_desc_scm()` does not depend on `channel_length`. Additionally, the `channel_offset` can be set to perform the LOS optimization.

Additionally, to ensure that memory is freed, `CHANMODEL_FREE_BICTR` should be set in `channel_desc_t::free_flags`.

### Assumptions
In the experimental integration with OAI, a few assumptions were made. These need to be checked.
* The IQ samples are modulated and sent to RFSimulator in passband.
  * **This has been disproven.** The IQ samples are in baseband and not modulated. Work is ongoing to adapt BICTR for this use case.
* RFSimulator applies the transmitter power to the IQ samples. BICTR then applies the pathloss to the IQ samples in the FIR filter. 
  * I'm not sure where RFSimulator applies and uses pathloss.