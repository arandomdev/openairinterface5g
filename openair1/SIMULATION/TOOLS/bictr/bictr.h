#ifndef __SIMULATION_TOOLS_BICTR_H__
#define __SIMULATION_TOOLS_BICTR_H__

#include <stdbool.h>

typedef struct {
  /// Antenna params
  double tx_lat;
  double tx_lon;
  /// Height in meters above the ground
  double tx_height;
  double rx_lat;
  double rx_lon;
  /// Height in meters above the ground
  double rx_height;
  /// If the antenna is horizontally polarized
  bool horizontal_polarization;

  /// Ring search params
  /// Number of reflectors to search for
  unsigned int ref_count;
  /// Number of attempts per ring to find a reflector
  unsigned int ref_attempt_per_ring;
  /// Minimum ring radius
  double ring_radius_min;
  /// Maximum ring radius
  double ring_radius_max;
  /// Extra radius to define a band around a ring to place the reflector
  double ring_radius_uncertainty;
  /// Maximum number of rings to generate
  unsigned int ring_count;

  /// Permittivity params
  /// Average of the real component of permittivity
  double complex_rel_permittivity_real;
  /// Standard deviation of the real component of permittivity
  double complex_rel_permittivity_real_std;
  /// Average of the imaginary component of permittivity
  double complex_rel_permittivity_imag;
  /// Standard deviation of the imaginary component of permittivity
  double complex_rel_permittivity_imag_std;

  /// Rayleigh fading params
  /// Number of fading paths, must be divisible by 4
  unsigned int fading_paths;
  /// The relative velocity between the TX and RX
  double fading_doppler_spread;

  /// BICTR resources
  /// GMT Session
  void *gmtAPI;
} bictr_desc_t;

/// @brief Compute the number of samples needed to represent a delay. Note conversion to base SI units does not need to be done if
/// the sampling frequency and the delay are the same magnitude, i.e. MHz and microseconds.
/// @param fs Sampling frequency in Hz
/// @param delay Time delay in seconds
/// @return The number of samples
unsigned int delay_samples(double fs, double delay);

/// @brief Initialize resources for bictr
/// @param desc The descriptor to store runtime 
void bictr_initialize(bictr_desc_t *desc);

/// @brief Free any resources used by bictr
/// @param desc Model descriptor
void bictr_free(bictr_desc_t *desc);


#endif // __SIMULATION_TOOLS_BICTR_H__