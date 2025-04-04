#ifndef __SIMULATION_TOOLS_BICTR_H__
#define __SIMULATION_TOOLS_BICTR_H__

#include "PHY/TOOLS/tools_defs.h"
#include <gmt/gmt.h>

#define BICTR_BODY_DATASET_EARTH "@earth_relief_01s_g"
#define BICTR_BODY_DATASET_MOON "@moon_relief_01m_g"
#define BICTR_BODY_RADIUS_EARTH 6378137.0
#define BICTR_BODY_RADIUS_MOON 1737.4e3
#define BICTR_BODY_GRID_SIZE_EARTH (1.0 / 3600.0)
#define BICTR_BODY_GRID_SIZE_MOON (1.0 / 60.0)

/// @brief Structure used to specify parameters of the loaded body
typedef struct {
  /// GMT dataset name
  char *dataset;
  /// Radius of the body to serve as the zero point heights
  double radius;
  /// Size of each cell in the gridded dataset
  double grid_size;
} bictr_body_info_t;

/// @brief Structure to encode a coordinate point
typedef struct {
  double lon;
  double lat;
} bictr_point_geo_t;


/// @brief Structure for BICTR parameters and resources
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

  /// Other parameters
  /// Max channel length to generate
  unsigned int max_channel_length;
  /// Sampling rate of the system
  double sampling_freq;

  /// Runtime information
  /// Coordinate region that is loaded
  bictr_point_geo_t region_min;
  bictr_point_geo_t region_max;
  
  /// Loaded body information
  bictr_body_info_t body;

  /// Runtime resources
  /// GMT Session for API calls
  void *gmt_sess;
  /// DEM VF reference name
  char vf_grid[GMT_VF_LEN];
} bictr_desc_t;


/// @brief Supported celestial bodies
typedef enum { BICTR_BODY_EARTH, BICTR_BODY_MOON } bictr_body_e;

/// @brief Compute the number of samples needed to represent a delay. Note conversion to base SI units does not need to be done if
/// the sampling frequency and the delay are the same magnitude, i.e. MHz and microseconds.
/// @param fs Sampling frequency in Hz
/// @param delay Time delay in seconds
/// @return The number of samples
unsigned int bictr_delay_samples(double fs, double delay);

/// @brief Initialize resources for bictr
/// @param desc The descriptor to store runtime
/// @param body The body to load the region for
/// @param region_min Minimum coordinate of the boundary box to load
/// @param region_max Maximum coordinate of the boundary box to load
/// @returns 0 if successful, non zero otherwise
int bictr_initialize(bictr_desc_t *desc, bictr_body_e body, bictr_point_geo_t region_min, bictr_point_geo_t region_max);

/// @brief Free any resources used by bictr
/// @param desc Model descriptor
void bictr_free(bictr_desc_t *desc);

/// @brief Generate a new channel
/// @param desc Bictr descriptor
/// @param ch The channel array to write to
/// @param [out] channel_offset Additional offset the generated channel has
/// @return The length of the channel
unsigned int bictr_generate_channel(bictr_desc_t *desc, struct complexd *ch, unsigned int *channel_offset);

/// @brief Load the region into memory, will download the relief if needed
/// @param desc Bictr descriptor
/// @return 0 if successful, non zero otherwise
int _bictr_load_region(bictr_desc_t *desc);

/// @brief Sample heights at a list of points
/// @param desc Bictr descriptor
/// @param points The list of coordinate points to sample
/// @param n_points The number of points in the list
/// @param vf_heights The VF to write the heights (GMT_IS_DATASET, GMT_IS_PLP, GMT_OUT)
/// @return 0 if successful, non-zero otherwise
int _bictr_get_heights(bictr_desc_t *desc, double *lons, double *lats, size_t n_points, char *vf_heights);
#endif // __SIMULATION_TOOLS_BICTR_H__