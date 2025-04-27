#include "bictr.h"
#include <assert.h>
#include <math.h>
#include <complex.h>

#define SPEED_OF_LIGHT 299792458

unsigned int bictr_delay_samples(double fs, double delay)
{
  return (int)round(fs * delay);
}

int bictr_initialize(bictr_desc_t *desc,
                     bictr_body_e body,
                     const bictr_point_geo_t *region_min,
                     const bictr_point_geo_t *region_max,
                     uint32_t prng_seed)
{
  // Set safe default values for resources in case initialization fails
  desc->gmt_sess = NULL;
  desc->vf_grid[0] = '\0';

  desc->gmt_sess = GMT_Create_Session("BICTR", 2, 0, NULL);

  desc->region_min = *region_min;
  desc->region_max = *region_max;

  switch (body) {
    case BICTR_BODY_EARTH: {
      desc->body.dataset = BICTR_BODY_DATASET_EARTH;
      desc->body.radius = BICTR_BODY_RADIUS_EARTH;
      desc->body.grid_size = BICTR_BODY_GRID_SIZE_EARTH;
      break;
    }
    case BICTR_BODY_MOON: {
      desc->body.dataset = BICTR_BODY_DATASET_MOON;
      desc->body.radius = BICTR_BODY_RADIUS_MOON;
      desc->body.grid_size = BICTR_BODY_GRID_SIZE_MOON;
      break;
    }

    default: {
      return -1;
      break;
    }
  }

  int err = _bictr_load_region(desc);

  desc->prng_state = seedRand(prng_seed);
  return err;
}

void bictr_free(bictr_desc_t *desc)
{
  if (desc->vf_grid[0] != '\0') {
    GMT_Close_VirtualFile(desc->gmt_sess, desc->vf_grid);
  }

  if (desc->gmt_sess) {
    GMT_Destroy_Session(desc->gmt_sess);
  }
}

int bictr_generate_channel(bictr_desc_t *desc, struct complexd *ch, unsigned int *channel_offset, unsigned int *channel_length)
{
  int err = 0;
  char vf_heights[GMT_VF_LEN] = "";

  // defaults for out parameters
  *channel_offset = 0;
  *channel_length = 0;

  // Allocate pair of buffers to store delays and paths. Max number of paths is ref_count + los
  unsigned int *path_delay_samples = malloc((desc->ref_count + 1) * sizeof(*path_delay_samples)); // delay of a path in samples
  double complex *path_phasors = malloc((desc->ref_count + 1) * sizeof(*path_phasors));
  size_t n_paths = 0;

  // Allocate buffer to store reflectors
  bictr_point_3D_t *reflectors = malloc(desc->ref_count * sizeof(*reflectors));

  // Buffer for rayleigh fading channel
  struct complexd *rayleigh_ch = NULL;

  // Convert tx and rx to point locations
  double trx_lons[2] = {desc->tx_coord.lon, desc->rx_coord.lon};
  double trx_lats[2] = {desc->tx_coord.lat, desc->rx_coord.lat};

  if ((err = GMT_Open_VirtualFile(desc->gmt_sess, GMT_IS_DATASET, GMT_IS_PLP, GMT_OUT, NULL, vf_heights))) {
    goto cleanup;
  }

  if ((err = _bictr_get_heights(desc, trx_lons, trx_lats, 2, vf_heights))) {
    goto cleanup;
  }

  struct GMT_DATASET *ds_heights = (struct GMT_DATASET *)GMT_Read_VirtualFile(desc->gmt_sess, vf_heights);
  if (ds_heights->n_records != 2) {
    err = -1;
    goto cleanup;
  }

  double *trx_heights = ds_heights->table[0][0].segment[0][0].data[2];

  bictr_point_3D_t tx_loc = _bictr_geo_to_3D(desc, &desc->tx_coord, trx_heights[0] + desc->tx_height);
  bictr_point_3D_t rx_loc = _bictr_geo_to_3D(desc, &desc->rx_coord, trx_heights[1] + desc->rx_height);

  // Compute LOS delay path if it exists
  double los_dist = _bictr_compute_distance(&tx_loc, &rx_loc);
  double los_delay = los_dist / SPEED_OF_LIGHT;
  double los_pl = _bictr_fspl(desc->carr_freq, los_dist);

  bool has_los;
  if ((err = _bictr_check_los(desc, &desc->tx_coord, desc->tx_height, &desc->rx_coord, desc->rx_height, &has_los))) {
    goto cleanup;
  }
  if (has_los) {
    double complex los_phasor = cexp(I * (-2 * M_PI * desc->carr_freq * los_delay));
    unsigned int los_delay_samples = bictr_delay_samples(desc->sampling_freq, los_delay);
    path_phasors[n_paths] = los_phasor;
    path_delay_samples[n_paths] = los_delay_samples;
    n_paths += 1;
  }

  // Compute reflector paths
  size_t n_reflectors;
  if ((err = _bictr_generate_reflectors(desc, reflectors, &n_reflectors))) {
    goto cleanup;
  }
  if (n_reflectors) {
    // Generate random variables
    /// NOTE: Originally calls to generate random variables were called as needed, but in order to match the call order in Python,
    /// they need to be called in bulk.
    double complex *complex_rel_perms = malloc(n_reflectors * sizeof(*complex_rel_perms));
    double *phases = malloc(n_reflectors * sizeof(*phases));
    for (size_t i = 0; i < n_reflectors; i++) {
      double real = _bictr_normal_variate(desc, desc->complex_rel_permittivity_real, desc->complex_rel_permittivity_real_std);
      double imag = _bictr_normal_variate(desc, desc->complex_rel_permittivity_imag, desc->complex_rel_permittivity_imag_std);
      complex_rel_perms[i] = real + (I * imag);
    }
    for (size_t i = 0; i < n_reflectors; i++) {
      phases[i] = _bictr_uniform_random(desc, 0, 2 * M_PI);
    }

    // Process reflectors
    for (size_t i = 0; i < n_reflectors; i++) {
      bictr_point_3D_t *ref_loc = &reflectors[i];

      // Compute path distances
      double tx_ref_dist = _bictr_compute_distance(&tx_loc, ref_loc);
      double ref_rx_dist = _bictr_compute_distance(ref_loc, &rx_loc);
      double ref_dist = tx_ref_dist + ref_rx_dist;

      // Compute time delay and pathloss
      double ref_delay = ref_dist / SPEED_OF_LIGHT;
      double ref_pl = _bictr_fspl(desc->carr_freq, ref_dist);

      // Compute reflection angle with law of cosines
      double ref_angle = (M_PI
                          - acos(((tx_ref_dist * tx_ref_dist) + (ref_rx_dist * ref_rx_dist) - (los_dist * los_dist))
                                 / (2 * tx_ref_dist * ref_rx_dist)))
                         / 2;

      // Compute reflection coefficient
      double complex ref_polarization;
      if (desc->horizontal_polarization) {
        ref_polarization = csqrt(complex_rel_perms[i] - (pow(cos(ref_angle), 2)));
      } else {
        ref_polarization = csqrt(complex_rel_perms[i] - (pow(cos(ref_angle), 2))) / complex_rel_perms[i];
      }

      double complex ref_coeff = (sin(ref_angle) - ref_polarization) / (sin(ref_angle) + ref_polarization);

      // Compute phasor and delay samples
      double complex ref_phasor = ref_pl * ref_coeff * cexp(I * phases[i]);
      unsigned int ref_delay_samples = bictr_delay_samples(desc->sampling_freq, ref_delay);

      // Record
      path_phasors[n_paths] = ref_phasor;
      path_delay_samples[n_paths] = ref_delay_samples;
      n_paths += 1;
    }

    free(complex_rel_perms);
    free(phases);
  }

  if (n_paths == 0) {
    // Set channel to null any signal
    *channel_offset = 0;
    *channel_length = 1;
    ch[0].r = 0;
    ch[0].i = 0;

    goto cleanup;
  }

  // Get the minimum delay for LOS optimization
  *channel_offset = UINT_MAX;
  for (size_t i = 0; i < n_paths; i++) {
    if (path_delay_samples[i] < *channel_offset) {
      *channel_offset = path_delay_samples[i];
    }
  }

  // Zero out channel
  memset(ch, 0, desc->max_channel_length * sizeof(*ch));

  for (size_t i = 0; i < n_paths; i++) {
    // Subtract los offset
    unsigned int delay_samples = path_delay_samples[i] - *channel_offset;

    // Check delay does not overrun channel buffer
    /// TODO: Emit warning or error if it does
    if (delay_samples >= desc->max_channel_length) {
      continue;
    }

    // update max channel length
    if (delay_samples >= *channel_length) {
      *channel_length = delay_samples + 1; // delay_samples is an index, +1 for channel length
    }

    // Add phasor
    ch[delay_samples].r += creal(path_phasors[i]);
    ch[delay_samples].i += cimag(path_phasors[i]);
  }

  // generate rayleigh fading
  rayleigh_ch = malloc(*channel_length * sizeof(*rayleigh_ch));
  _bictr_generate_rayleigh(desc, rayleigh_ch, *channel_length);

  // Normalize rayleigh fading and add to channel
  for (size_t i = 0; i < *channel_length; i++) {
    rayleigh_ch[i].r *= los_pl / *channel_length;
    rayleigh_ch[i].i *= los_pl / *channel_length;

    ch[i].r += rayleigh_ch[i].r;
    ch[i].i += rayleigh_ch[i].i;
  }

  // Normalize
  for (size_t i = 0; i < *channel_length; i++) {
    ch[i].r /= desc->ref_count + 1;
    ch[i].i /= desc->ref_count + 1;
  }

cleanup:
  GMT_Close_VirtualFile(desc->gmt_sess, vf_heights);
  free(reflectors);
  free(path_phasors);
  free(path_delay_samples);

  if (rayleigh_ch) {
    free(rayleigh_ch);
  }
  return err;
}

void _bictr_generate_rayleigh(bictr_desc_t *desc, struct complexd *ch, unsigned int channel_length)
{
  unsigned int m = desc->fading_paths / 4;
  double wd = 2 * M_PI * desc->fading_doppler_spread * desc->carr_freq / SPEED_OF_LIGHT;

  double complex *temp_ch = malloc(channel_length * sizeof(*temp_ch));

  // Generate real component
  for (size_t n = 1; n < m + 1; n++) {
    double theta = _bictr_uniform_random(desc, -M_PI, M_PI);
    double phi = _bictr_uniform_random(desc, -M_PI, M_PI);
    double psi = _bictr_uniform_random(desc, -M_PI, M_PI);
    double angle = (2 * M_PI * n - M_PI + theta) / (4 * m);

    for (size_t i = 0; i < channel_length; i++) {
      temp_ch[i] += cos(psi) * cos(wd * (i / desc->sampling_freq) * cos(angle) + phi);
    }
  }

  // Generate imaginary component
  for (size_t n = 1; n < m + 1; n++) {
    double theta = _bictr_uniform_random(desc, -M_PI, M_PI);
    double phi = _bictr_uniform_random(desc, -M_PI, M_PI);
    double psi = _bictr_uniform_random(desc, -M_PI, M_PI);
    double angle = (2 * M_PI * n - M_PI + theta) / (4 * m);

    for (size_t i = 0; i < channel_length; i++) {
      temp_ch[i] += I * sin(psi) * cos(wd * (i / desc->sampling_freq) * cos(angle) + phi);
    }
  }

  // Normalize
  for (size_t i = 0; i < channel_length; i++) {
    temp_ch[i] *= 2 / sqrt(m);
  }

  double sum = 0;
  for (size_t i = 0; i < channel_length; i++) {
    sum += pow(cabs(temp_ch[i]), 2);
  }
  double avgPower = sum / channel_length;

  for (size_t i = 0; i < channel_length; i++) {
    temp_ch[i] *= sqrt(1 / avgPower);
  }

  // Zero out channel and copy
  memset(ch, 0, channel_length * sizeof(*ch));
  for (size_t i = 0; i < channel_length; i++) {
    ch[i].r = creal(temp_ch[i]);
    ch[i].i = cimag(temp_ch[i]);
  }

  free(temp_ch);
}

int _bictr_load_region(bictr_desc_t *desc)
{
  // Create VF for grid
  int err;
  if ((err = GMT_Open_VirtualFile(desc->gmt_sess, GMT_IS_GRID, GMT_IS_SURFACE, GMT_OUT, NULL, desc->vf_grid))) {
    return err;
  }

  // Format region string
  char region[96] = "-R";
  snprintf(&region[2],
           94,
           "%.10f/%.10f/%.10f/%.10f",
           desc->region_min.lon,
           desc->region_max.lon,
           desc->region_min.lat,
           desc->region_max.lat);

  // Call
  char *args[] = {
      desc->body.dataset, // dataset to load
      desc->vf_grid, // Output file
      "-Tg", // Datatype: grid
      &region[0] // Region
  };
  err = GMT_Call_Module(desc->gmt_sess,
                        "read",
                        4, // 4 args
                        args);
  return err;
}

int _bictr_get_heights(bictr_desc_t *desc, double *lons, double *lats, size_t n_points, char *vf_heights)
{
  int err;

  // Create table VF for input points
  uint64_t table_data_dims[] = {
      2, // 2 cols
      n_points, // rows
      0, // NA
      0 // NA
  };
  void *table_data = GMT_Create_Data(desc->gmt_sess,
                                     GMT_IS_DATASET | GMT_VIA_VECTOR,
                                     GMT_IS_POINT,
                                     GMT_CONTAINER_ONLY,
                                     table_data_dims,
                                     NULL,
                                     NULL,
                                     0,
                                     GMT_PAD_DEFAULT,
                                     NULL);
  if ((err = GMT_Put_Vector(desc->gmt_sess, table_data, 0, GMT_DOUBLE, lons))) {
    return err;
  }
  if ((err = GMT_Put_Vector(desc->gmt_sess, table_data, 1, GMT_DOUBLE, lats))) {
    return err;
  }
  char vf_table[GMT_VF_LEN];
  if ((err = GMT_Open_VirtualFile(desc->gmt_sess,
                                  GMT_IS_DATASET | GMT_VIA_VECTOR,
                                  GMT_IS_POINT,
                                  GMT_IN | GMT_IS_REFERENCE,
                                  table_data,
                                  vf_table))) {
    return err;
  }

  // Grid option
  char grid[GMT_VF_LEN + 2] = "-G";
  memcpy(&grid[2], desc->vf_grid, GMT_VF_LEN);

  // output option
  char output[GMT_VF_LEN + 2] = "->";
  memcpy(&output[2], vf_heights, GMT_VF_LEN);

  // Call
  char *args[] = {vf_table, grid, output};
  err = GMT_Call_Module(desc->gmt_sess, "grdtrack", 3, args);

  // Cleanup
  /// NOTE: Since GMT_Create_Data is an empty container, no need to destroy it.
  err |= GMT_Close_VirtualFile(desc->gmt_sess, vf_table);
  return err;
}

int _bictr_get_track_heights(bictr_desc_t *desc, const bictr_point_geo_t *start, const bictr_point_geo_t *end, char *vf_heights)
{
  // Prepare call to project
  char center_arg[64] = "-C";
  snprintf(&center_arg[2], 62, "%.10f/%.10f", start->lon, start->lat);

  char end_arg[64] = "-E";
  snprintf(&end_arg[2], 62, "%.10f/%.10f", end->lon, end->lat);

  char generate_arg[32] = "-G";
  snprintf(&generate_arg[2], 30, "%.16f", desc->body.grid_size);

  int err;
  char vf_track[GMT_VF_LEN];
  if ((err = GMT_Open_VirtualFile(desc->gmt_sess, GMT_IS_DATASET, GMT_IS_PLP, GMT_OUT, NULL, vf_track))) {
    return err;
  }

  char vf_track_arg[GMT_VF_LEN + 2] = "->";
  memcpy(&vf_track_arg[2], vf_track, GMT_VF_LEN);

  // Call project
  char *project_args[] = {&center_arg[0],
                          &end_arg[0],
                          &generate_arg[0], // generate equal points
                          &vf_track_arg[0]};
  if ((err = GMT_Call_Module(desc->gmt_sess, "project", 4, project_args))) {
    GMT_Close_VirtualFile(desc->gmt_sess, vf_track);
    return err;
  }

  // Forward track to get heights
  struct GMT_DATASET *ds_track = (struct GMT_DATASET *)GMT_Read_VirtualFile(desc->gmt_sess, vf_track);
  if (ds_track == NULL) {
    GMT_Close_VirtualFile(desc->gmt_sess, vf_track);
    return -1;
  }
  assert(ds_track->n_tables == 1);
  assert(ds_track->n_segments == 1);
  assert(ds_track->n_columns == 3);
  size_t n_points = ds_track->n_records; // Since there is only one table and segment, this is equal to the number of rows
  double *lons = ds_track->table[0][0].segment[0][0].data[0]; // First column
  double *lats = ds_track->table[0][0].segment[0][0].data[1]; // Second column

  err = _bictr_get_heights(desc, lons, lats, n_points, vf_heights);

  // Clean up
  GMT_Close_VirtualFile(desc->gmt_sess, vf_track);
  return err;
}

bictr_point_3D_t _bictr_geo_to_3D(const bictr_desc_t *desc, const bictr_point_geo_t *coord, double height_bias)
{
  double inc = (90 - coord->lat) * M_PI / 180.0;
  double azi = coord->lon * M_PI / 180.0;
  double h = desc->body.radius + height_bias;

  bictr_point_3D_t point;
  point.x = h * sin(inc) * cos(azi);
  point.y = h * sin(inc) * sin(azi);
  point.z = h * cos(inc);
  return point;
}

double _bictr_compute_distance(const bictr_point_3D_t *a, const bictr_point_3D_t *b)
{
  double dist_x = a->x - b->x;
  double dist_y = a->y - b->y;
  double dist_z = a->z - b->z;
  return sqrt(pow(dist_x, 2) + pow(dist_y, 2) + pow(dist_z, 2));
}

double _bictr_fspl(double freq, double dist)
{
  return (double)SPEED_OF_LIGHT / (4 * M_PI * dist * freq);
}

int _bictr_check_los(bictr_desc_t *desc,
                     const bictr_point_geo_t *start,
                     double start_height_bias,
                     const bictr_point_geo_t *end,
                     double end_height_bias,
                     bool *has_los)
{
  // get heights
  int err;
  char vf_heights[GMT_VF_LEN];
  if ((err = GMT_Open_VirtualFile(desc->gmt_sess, GMT_IS_DATASET, GMT_IS_PLP, GMT_OUT, NULL, vf_heights))) {
    return err;
  }

  if ((err = _bictr_get_track_heights(desc, start, end, vf_heights))) {
    GMT_Close_VirtualFile(desc->gmt_sess, vf_heights);
    return err;
  }

  // Check if there is LOS
  struct GMT_DATASET *ds_heights = (struct GMT_DATASET *)GMT_Read_VirtualFile(desc->gmt_sess, vf_heights);
  size_t n_points = ds_heights->n_records;
  if (n_points == 0) {
    GMT_Close_VirtualFile(desc->gmt_sess, vf_heights);
    return -1;
  }

  double *heights = ds_heights->table[0][0].segment[0][0].data[2];
  double slope = ((heights[n_points - 1] + end_height_bias) - (heights[0] + start_height_bias)) / (n_points - 1);

  // Skip first and last points, only want to check the path in between
  double line_height = heights[0] + start_height_bias + slope;
  *has_los = true;
  for (size_t i = 1; i < n_points - 1; i++) {
    if (line_height < heights[i]) {
      *has_los = false;
      break;
    }

    line_height += slope;
  }

  // Clean up
  return GMT_Close_VirtualFile(desc->gmt_sess, vf_heights);
}

int _bictr_generate_reflectors(bictr_desc_t *desc, bictr_point_3D_t *reflectors, size_t *n_found)
{
  unsigned int ref_count = desc->ref_count;
  unsigned int ref_attempt_per_ring = desc->ref_attempt_per_ring;
  double ring_radius_min = desc->ring_radius_min;
  double ring_radius_max = desc->ring_radius_max;
  double ring_radius_uncertainty = desc->ring_radius_uncertainty;
  unsigned int ring_count = desc->ring_count;

  *n_found = 0;
  double curr_radius = ring_radius_min;
  double radius_step = (ring_radius_max - ring_radius_min) / ring_count;

  // Allocate mem for temp coord buffers
  double *ref_lons = malloc(ref_count * sizeof(*ref_lons));
  double *ref_lats = malloc(ref_count * sizeof(*ref_lats));

  int err = 0;
  char vf_heights[GMT_VF_LEN] = "";

  while ((curr_radius <= ring_radius_max) && (*n_found < ref_count)) {
    for (int i = 0; i < ref_attempt_per_ring; i++) {
      double radius = _bictr_uniform_random(desc, curr_radius - ring_radius_uncertainty, curr_radius + ring_radius_uncertainty);
      double theta = _bictr_uniform_random(desc, 0, 2 * M_PI);
      bictr_point_geo_t ref_coord = _bictr_destination(desc, &desc->rx_coord, theta, radius);

      // Check LOS from tx to ref and from ref to rx
      bool has_los1 = false;
      bool has_los2 = false;
      if ((err = _bictr_check_los(desc, &desc->tx_coord, desc->tx_height, &ref_coord, 0, &has_los1))) {
        goto cleanup;
      }
      if (has_los1 && (err = _bictr_check_los(desc, &ref_coord, 0, &desc->rx_coord, desc->rx_height, &has_los2))) {
        goto cleanup;
      }

      if (has_los1 && has_los2) {
        ref_lons[*n_found] = ref_coord.lon;
        ref_lats[*n_found] = ref_coord.lat;
        *n_found += 1;

        if (*n_found == ref_count) {
          break;
        }
      }
    }

    // Exhausted ring, increase radius
    curr_radius += radius_step;
  }

  if (*n_found) {
    // Convert coordinates to points
    if ((err = GMT_Open_VirtualFile(desc->gmt_sess, GMT_IS_DATASET, GMT_IS_PLP, GMT_OUT, NULL, vf_heights))) {
      goto cleanup;
    }

    if ((err = _bictr_get_heights(desc, ref_lons, ref_lats, *n_found, vf_heights))) {
      goto cleanup;
    }

    struct GMT_DATASET *ds_heights = (struct GMT_DATASET *)GMT_Read_VirtualFile(desc->gmt_sess, vf_heights);
    if (ds_heights->n_records != *n_found) {
      err = -1;
      goto cleanup;
    }

    double *heights = ds_heights->table[0][0].segment[0][0].data[2];
    for (size_t i = 0; i < *n_found; i++) {
      bictr_point_geo_t ref_coord = {ref_lons[i], ref_lats[i]};
      bictr_point_3D_t ref_loc = _bictr_geo_to_3D(desc, &ref_coord, heights[i]);
      memcpy(&reflectors[i], &ref_loc, sizeof(bictr_point_3D_t));
    }
  }

  // Clean up
cleanup:
  GMT_Close_VirtualFile(desc->gmt_sess, vf_heights);
  free(ref_lons);
  free(ref_lats);
  return err;
}

bictr_point_geo_t _bictr_destination(const bictr_desc_t *desc, const bictr_point_geo_t *loc, double bearing, double distance)
{
  double dist_rad = distance / desc->body.radius;
  double lon1 = loc->lon * M_PI / 180.0;
  double lat1 = loc->lat * M_PI / 180.0;

  double lat2;
  double lon2;
  if (loc->lat == 90) {
    lon2 = bearing - M_PI;
    lat2 = (M_PI / 2) - dist_rad;
  } else if (loc->lat == -90) {
    lon2 = bearing - M_PI;
    lat2 = (-M_PI / 2) + dist_rad;
  } else {
    lat2 = asin(sin(lat1) * cos(dist_rad) + cos(lat1) * sin(dist_rad) * cos(bearing));
    lon2 = lon1 + atan2(sin(bearing) * sin(dist_rad) * cos(lat1), cos(dist_rad) - sin(lat1) * sin(lat2));
  }

  bictr_point_geo_t dest;
  dest.lat = lat2 * 180 / M_PI;
  dest.lon = lon2 * 180 / M_PI;
  return dest;
}

double _bictr_uniform_random(bictr_desc_t *desc, double a, double b)
{
  return a + (b - a) * genRand(&desc->prng_state);
}

double _bictr_normal_variate(bictr_desc_t *desc, double mu, double sigma)
{
  // Adapted from python's implementation
  // https://github.com/python/cpython/blob/a94c7528b596e9ec234f12ebeeb45fc731412b18/Lib/random.py#L536

  const double NV_MAGICCONST = 4.0 * exp(-0.5) / sqrt(2.0);

  double z;
  while (true) {
    double u1 = genRand(&desc->prng_state);
    double u2 = 1.0 - genRand(&desc->prng_state);
    z = NV_MAGICCONST * (u1 - 0.5) / u2;
    double zz = z * z / 4.0;
    if (zz <= -log(u2)) {
      break;
    }
  }

  return mu + z * sigma;
}

#ifdef BICTR_TEST_EXECUTABLE
int main(int argc, char const *argv[])
{
  bictr_desc_t desc = {.tx_coord = {-111.633156, 35.590627},
                       .tx_height = 10,
                       .rx_coord = {-111.625833, 35.596667},
                       .rx_height = 2,
                       .horizontal_polarization = false,
                       .ref_count = 5,
                       .ref_attempt_per_ring = 3,
                       .ring_radius_min = 5,
                       .ring_radius_max = 300,
                       .ring_radius_uncertainty = 15,
                       .ring_count = 100,
                       .complex_rel_permittivity_real = 7.058396,
                       .complex_rel_permittivity_real_std = 0.007131,
                       .complex_rel_permittivity_imag = -0.862227,
                       .complex_rel_permittivity_imag_std = 0.001397,
                       .fading_paths = 1024,
                       .fading_doppler_spread = 1,
                       .max_channel_length = 3000,
                       .sampling_freq = 3e9,
                       .carr_freq = 913e6};

  bictr_point_geo_t region_min = {-111.655615, 35.568169};
  bictr_point_geo_t region_max = {-111.610698, 35.613086};

  int err;
  if ((err = bictr_initialize(&desc, BICTR_BODY_EARTH, &region_min, &region_max, 0x00000001))) {
    printf("Failed to initialize BICTR\n");
    return -1;
  }

  // Force set state to match python. random.seed(0)
  uint32_t mt[] = {
      2147483648, 766982754,  497961170,  3952298588, 2331775348, 1811986599, 3100132149, 3188119873, 3937547222, 215718963,
      3315684082, 2978012849, 2428261856, 1298227695, 1704729580, 54668373,   3285201915, 3285178464, 1552935063, 988471319,
      3135387943, 1691402966, 2757551880, 416056905,  907387413,  1072924981, 33903495,   2168419592, 2429050353, 831159753,
      430343641,  3315943586, 1761671042, 864453023,  334804929,  1627478028, 2596811275, 3468733638, 3994375553, 1457139722,
      3139722021, 1334790738, 2656639915, 3535811098, 1464315470, 2397423927, 885719490,  1140895889, 3284299483, 2854516462,
      2734973817, 147484763,  792049954,  114360641,  3345458839, 1159898878, 1410498733, 2242989638, 453922141,  1344019764,
      413870456,  3089405849, 1494382840, 470157779,  4266372830, 2831181573, 1361928602, 1589253513, 1381373062, 753045124,
      987032420,  781978839,  2953638767, 3258570111, 3006718191, 1675218601, 1854232715, 3655829819, 1731242722, 2192104666,
      1736665161, 740150002,  1195833394, 1610203160, 159492766,  4041488705, 3128952632, 2867295744, 3272632449, 886824304,
      1791482600, 221114776,  3867175393, 4020804062, 1077871826, 1298953503, 996366221,  4149754679, 2483052703, 2615558283,
      274318093,  1716359450, 4099129961, 1026774175, 288240973,  1459347562, 2365566296, 3690105224, 3065780221, 2050634722,
      2652606621, 3185241207, 3026457375, 3456165734, 1880121515, 3398461093, 1795638629, 2379692076, 608668379,  1261955525,
      84456522,   1913485156, 106878280,  757183891,  2913957588, 160418091,  2025664758, 141497907,  1657818026, 3053760160,
      672193054,  4157546743, 223046484,  1623470498, 1201972930, 675008814,  684162366,  1738776330, 3025656654, 159760723,
      1908867305, 3933381342, 2545706671, 467196949,  1427819885, 842150314,  4032903454, 2140851898, 3269883445, 975813755,
      4177392955, 1556690684, 2535611513, 462962732,  67591358,   1729610528, 2025206740, 3153739740, 3255032049, 4186226368,
      1070144624, 3107867195, 1621006038, 63742485,   835629717,  3189842019, 3950227584, 3184714559, 841836938,  1685394870,
      657939920,  766156242,  1412314179, 1048281639, 4037161120, 2044490307, 1923947830, 3900790422, 907554295,  276417304,
      860658646,  3574201134, 3508771399, 2110232300, 1636296241, 1405006077, 1093408401, 3243057343, 1519791182, 1994660136,
      3829840937, 2644974199, 957955566,  3487641161, 1646922510, 1907939989, 3836029453, 3429168778, 201307778,  72550089,
      2464394982, 1695794191, 3344785682, 996786130,  3589457196, 1241754792, 1291082245, 4224603667, 1194379475, 2693491244,
      881186965,  2705535111, 445306946,  440274268,  1980827733, 2482488861, 3205215943, 2119332222, 2928713046, 1418736938,
      652581136,  2474070665, 2208621536, 4171251876, 2303664214, 443762656,  2981912989, 2199228311, 2652261633, 3166738494,
      3443009210, 3498764432, 424010848,  4065487566, 2262993542, 1756076712, 1477098233, 2742171915, 306185806,  3610666541,
      923091830,  1034267993, 2336668648, 1880719718, 676878038,  3788797208, 3763351494, 3985428106, 1101865631, 1130501258,
      3672967388, 3432003530, 4124438011, 1660392285, 4025484827, 2108074566, 3815409682, 42955331,   3248965569, 1643835718,
      1246665668, 1071162194, 3814069229, 115491158,  985096811,  3311029186, 2990827378, 3101633320, 1648574497, 1470117052,
      174145027,  2019894819, 2035501481, 459104123,  3507464599, 2093352659, 3369174406, 618767835,  4009895756, 935587447,
      3956987426, 33753995,   307782427,  2473424805, 1440371818, 2382619594, 2138695812, 3164510238, 1318650933, 2910086616,
      3886677510, 566832801,  3718063320, 1559818704, 183047272,  1142362855, 26306548,   645536402,  3875596208, 2272778168,
      3512733409, 1897046338, 38248886,   2570759766, 1806313150, 860304898,  2433450338, 4124013408, 1216634590, 1275388896,
      1169566669, 652504502,  761221427,  1448403764, 3129135949, 2513214949, 1269533687, 2413509541, 1226750363, 2450740925,
      4094137910, 945759293,  3636927736, 3178020081, 2509964157, 3878869300, 1848504895, 2018369720, 1579755740, 1023627943,
      924838836,  2653160914, 1812804174, 1521323076, 4012390528, 1338763317, 2608655937, 16022784,   1672945066, 2177189646,
      2944458483, 2213810972, 1369873847, 1224017670, 130901785,  3595066712, 2259115284, 3316038259, 455873927,  2917250465,
      3599550610, 1502173758, 684943436,  3079863840, 3144992244, 942855823,  1771140188, 2118780653, 3411494225, 2711180217,
      4239611184, 1371891067, 3398566397, 3105518599, 1310665701, 3345178451, 2959821156, 242241789,  2148966880, 3192740583,
      404401893,  3605380577, 1446464038, 3920522056, 2577523013, 1079274576, 286634372,  1752710796, 2351075979, 981312309,
      3410516352, 3468455736, 1938779182, 1592494371, 1533303080, 88045436,   438252489,  1220512168, 3487004938, 3724852871,
      1073434882, 3728218947, 2977555283, 4105408406, 3553772656, 1462006821, 3917158017, 119003006,  3470530198, 3439192457,
      2829375771, 3555715155, 32324691,   588735808,  1459221702, 803072782,  2699519868, 1530797005, 79738580,   671990400,
      4289511388, 3207115447, 2584684068, 832698998,  760958416,  1217440464, 2517898131, 2418819938, 3629956222, 3445024962,
      206619378,  365007395,  522114139,  1707954431, 540423623,  1786750801, 369253262,  4239016754, 147889201,  1637777773,
      236798285,  2806120188, 586972608,  2201782716, 1323327827, 819485723,  406078680,  3407345698, 1537169369, 1821691865,
      527271655,  3751827102, 1465426495, 3321682429, 2179672664, 401355478,  1068871880, 24609462,   1403522408, 2311580015,
      1532058170, 3877815340, 1768430711, 1619755157, 2832904331, 475102697,  354987331,  3295386430, 2816873951, 1039415736,
      363972779,  1499307670, 2895506264, 3746345349, 2678027234, 3251899088, 955392878,  2329157295, 1343358773, 309573887,
      2410178377, 2843173466, 361132917,  1755816798, 1319204283, 609284796,  1998842567, 1892325921, 223190385,  1483015769,
      2876023365, 3876009312, 3199738344, 491524099,  160383137,  1219178873, 3870310498, 1114580266, 4279604166, 855339774,
      1983818547, 2297848784, 4118592947, 4084409863, 2225095054, 4215601993, 946447434,  4205503762, 146088676,  778046685,
      1876936928, 3157333726, 2173097090, 3215738813, 4135448234, 1219619643, 1936128689, 2897130162, 3336043946, 3779039524,
      4200886837, 1359380925, 3402593091, 3140713935, 50855190,   3122065768, 1501584468, 2512255124, 687125154,  2666013386,
      837819715,  3057258172, 3653455791, 2868624990, 322131992,  42534870,   4036564806, 798099710,  3533853670, 190914037,
      3726947981, 2601169403, 602059656,  1365668439, 1918780004, 394790500,  277566007,  3891847777, 3365421094, 3139612253,
      1380519090, 1183088424, 4203794803, 3049949521, 4214159484, 3446206962, 1875544460, 3207220027, 3288287026, 913535288,
      178159620,  1410694581, 4190575040, 880731713,  1427805121, 404869072,  3413191414, 2865934056, 2899472677, 4239222733,
      688404529,  3923323887, 933651074,  1199453686, 642723732,  2850614853, 3104368451, 3054041024, 3129913503, 2805843726,
      1829781129, 3479062313, 650272704,  4224852052, 4085038685, 2616580676, 1793860711, 585126334,  2995262791, 520446536,
      3855655015, 1571815563, 2240778227, 2051010344, 1694977983, 788402852,  1988089041, 2035558649, 1800063056, 1234412692,
      2490862867, 417320514,  2415019489, 3374117797, 136034611,  898704236,  1247106941, 3923519397, 3563607190, 2454738671,
      3522360389, 2672645476, 146828884,  3985140042, 4233949333, 1184742586, 860278824,  2815489967, 983483427,  3190081845,
      3288865305, 3575181235, 1292151129, 4007823805, 4049420597, 3499391972, 1611182906, 1721268432, 2944249577, 2487212557,
      789127738,  4027610014, 1057334138, 2902720905};
  memcpy(desc.prng_state.mt, mt, sizeof(uint32_t) * STATE_VECTOR_LENGTH);

  for (size_t i = 0; i < 10; i++) {
    printf("%.15f\n", _bictr_normal_variate(&desc, 1, 1));
  }
}

#endif