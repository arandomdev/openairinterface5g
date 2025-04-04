#include "bictr.h"
#include <math.h>

unsigned int bictr_delay_samples(double fs, double delay)
{
  return (int)round(fs * delay);
}

int bictr_initialize(bictr_desc_t *desc, bictr_body_e body, bictr_point_geo_t region_min, bictr_point_geo_t region_max)
{
  // Set safe default values for resources in case initialization fails
  desc->gmt_sess = NULL;
  desc->vf_grid[0] = '\0';

  desc->gmt_sess = GMT_Create_Session("BICTR", 2, 0, NULL);

  desc->region_min = region_min;
  desc->region_max = region_max;

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

unsigned int bictr_generate_channel(bictr_desc_t *desc, struct complexd *ch, unsigned int *channel_offset)
{
  /// TODO: IMP
  return 0;
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

  // Grid option
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

#ifdef BICTR_TEST_EXECUTABLE
int main(int argc, char const *argv[])
{
  bictr_desc_t desc = {.tx_lat = 35.590627,
                       .tx_lon = -111.633156,
                       .tx_height = 10,
                       .rx_lat = 35.596667,
                       .rx_lon = -111.625833,
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
                       .sampling_freq = 3e9};

  bictr_point_geo_t region_min = {-111.655615, 35.568169};
  bictr_point_geo_t region_max = {-111.610698, 35.613086};

  int err;
  if ((err = bictr_initialize(&desc, BICTR_BODY_EARTH, region_min, region_max))) {
    printf("Failed to initialize BICTR\n");
    return -1;
  }

  double points[2][1] = {{-111.633156}, {35.590627}};

  char vf_heights[GMT_VF_LEN];
  if ((err = GMT_Open_VirtualFile(desc.gmt_sess, GMT_IS_DATASET, GMT_IS_PLP, GMT_OUT, NULL, vf_heights))) {
    printf("Failed to open VF for heights\n");
    return -1;
  }

  if ((err = _bictr_get_heights(&desc, (double *)&points[0][0], (double *)&points[1][0], 1, vf_heights))) {
    printf("Failed to sample heights\n");
    return -1;
  }

  struct GMT_DATASET *heightsDS = (struct GMT_DATASET *)GMT_Read_VirtualFile(desc.gmt_sess, vf_heights);
  printf("Height: %f\n", heightsDS->table[0][0].segment[0][0].data[2][0]);

  if ((err = GMT_Close_VirtualFile(desc.gmt_sess, vf_heights))) {
    printf("Failed to close VF for heights\n");
    return -1;
  }

  bictr_free(&desc);
  return 0;
}

#endif