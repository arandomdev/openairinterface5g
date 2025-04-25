#ifndef __SIMULATION_TOOLS_BICTR_H__
#define __SIMULATION_TOOLS_BICTR_H__

#include "PHY/TOOLS/tools_defs.h"
#include <gmt/gmt.h>
#include "mtwister.h"
#include <complex.h>

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

typedef struct {
  double x;
  double y;
  double z;
} bictr_point_3D_t;

/// @brief Structure for BICTR parameters and resources
typedef struct {
  /// Antenna params
  /// Transmitter coordinates
  bictr_point_geo_t tx_coord;
  /// Height in meters above the ground
  double tx_height;
  /// Receiver coordinates
  bictr_point_geo_t rx_coord;
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
  /// Carrier frequency
  double carr_freq;

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
  /// PRNG
  MTRand prng_state;
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
/// @param prng_seed Seed value for mtwister
/// @returns 0 if successful, non zero otherwise
int bictr_initialize(bictr_desc_t *desc,
                     bictr_body_e body,
                     const bictr_point_geo_t *region_min,
                     const bictr_point_geo_t *region_max,
                     uint32_t prng_seed);

/// @brief Free any resources used by bictr
/// @param desc Model descriptor
void bictr_free(bictr_desc_t *desc);

/// @brief Generate a new channel
/// @param desc Bictr descriptor
/// @param ch The channel array to write to
/// @param[out] channel_offset Additional offset the generated channel has
/// @param[out] channel_length The length of the generated channel
/// @return 0 if successful, non zero otherwise
int bictr_generate_channel(bictr_desc_t *desc, struct complexd *ch, unsigned int *channel_offset, unsigned int *channel_length);

/// @brief Generate the fast fading component of the channel
/// @param desc Bictr descriptor
/// @param ch The channel to write the fast fading channel
/// @param channel_length The number of samples to generate
void _bictr_generate_rayleigh(bictr_desc_t *desc, double complex *ch, unsigned int channel_length);

/// @brief Load the region into memory, will download the relief if needed
/// @param desc Bictr descriptor
/// @return 0 if successful, non zero otherwise
int _bictr_load_region(bictr_desc_t *desc);

/// @brief Sample heights at a list of points
/// @param desc Bictr descriptor
/// @param lons Longitude components of the list of coordinates to sample at
/// @param lans Latitude components of the list of coordinates to sample at
/// @param n_points The number of points in the list
/// @param vf_heights The VF to write the heights (GMT_IS_DATASET, GMT_IS_PLP, GMT_OUT)
/// @return 0 if successful, non-zero otherwise
int _bictr_get_heights(bictr_desc_t *desc, double *lons, double *lats, size_t n_points, char *vf_heights);

/// @brief Sample the elevation along a great-circle track
/// @param desc Bictr descriptor
/// @param start Starting point of the track
/// @param end End point of the track
/// @param vf_heights The VF to write the heights (GMT_IS_DATASET, GMT_IS_PLP, GMT_OUT)
/// @return 0 if successful, non-zero otherwise
int _bictr_get_track_heights(bictr_desc_t *desc, const bictr_point_geo_t *start, const bictr_point_geo_t *end, char *vf_heights);

/// @brief Convert a coordinate to a 3D point
/// @param desc Bictr descriptor
/// @param coord The coordinate to convert.
/// @param height_bias The relative height of the point from the radius of the body
/// @returns Converted point
bictr_point_3D_t _bictr_geo_to_3D(const bictr_desc_t *desc, const bictr_point_geo_t *coord, double height_bias);

/// @brief Compute the distance between two points
/// @param a The first point
/// @param b The second point
/// @return The distance between them
double _bictr_compute_distance(const bictr_point_3D_t *a, const bictr_point_3D_t *b);

/// @brief Computes the free space pathloss of the E field, i.e without the square.
/// @param freq The frequency of the signal in Hz.
/// @param dist The distance traveled in meters.
/// @return The amplitude gain due to fspl.
double _bictr_fspl(double freq, double dist);

/// @brief Check if two points have LOS
/// @param desc Bictr descriptor
/// @param start Starting coordinate
/// @param start_height_bias The height of the starting coordinate relative to the ground
/// @param end Ending coordinate
/// @param end_height_bias The height of the ending coordinate relative to the ground
/// @param[out] has_los If the two points have LOS
/// @return 0 if successful, non-zero otherwise
int _bictr_check_los(bictr_desc_t *desc,
                     const bictr_point_geo_t *start,
                     double start_height_bias,
                     const bictr_point_geo_t *end,
                     double end_height_bias,
                     bool *has_los);

/// @brief Find reflectors
/// @param desc Bictr descriptor
/// @param[out] reflectors An empty array to write reflectors to. Must allocate enough space for maximum number of reflectors.
/// @param[out] n_found The number of reflectors found.
/// @return 0 if successful, non-zero otherwise
int _bictr_generate_reflectors(bictr_desc_t *desc, bictr_point_3D_t *reflectors, size_t *n_found);

/// @brief Compute the destination coordinate with bearing and distance
/// @param desc Bictr descriptor
/// @param loc The starting coordinate
/// @param bearing Direction of travel, clockwise from north, in radians [0, 2pi]
/// @param distance Distance of travel in meters
/// @returns Resulting destination
bictr_point_geo_t _bictr_destination(const bictr_desc_t *desc, const bictr_point_geo_t *loc, double bearing, double distance);

/// @brief Generate a uniform random number on [a, b]
/// @param desc Bictr descriptor
/// @param a Low bound
/// @param b High bound
/// @return The random number
double _bictr_uniform_random(bictr_desc_t *desc, double a, double b);

/// @brief Generate a random number on a normal distribution
/// @param desc Bictr descriptor
/// @param mu mean
/// @param sigma standard deviation
/// @return The random number
double _bictr_normal_variate(bictr_desc_t *desc, double mu, double sigma);

#endif // __SIMULATION_TOOLS_BICTR_H__