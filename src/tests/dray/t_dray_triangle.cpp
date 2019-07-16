#include "gtest/gtest.h"
#include "test_config.h"
/// #include "t_utils.hpp"

#include <stdio.h>

#include <dray/utils/png_encoder.hpp>
#include <dray/filters/surface_triangle.hpp>

TEST(dray_triangle, dray_triangle_single)
{
  std::string output_file = "triangle";
  /// const int c_width = 1024;
  /// const int c_height = 1024;
  const int c_width = 32;
  const int c_height = 32;

  using Coord = dray::Vec<float,2>;
  using Color = dray::Vec<float,4>;
  dray::Array<Color> img_buffer;

  // Define a linear triangle.
  Coord linear_triangle[3] = { {0.0f, 0.0f},
                               {.75f, 0.0f},
                               {0.0f, .75f},
  };

  // Define a quadratic triangle.
  Coord quadratic_triangle[6] = { {0.0f, 0.0f},  {0.4f, 0.0f},  {0.8f, 0.0f},
                                  {0.0f, 0.4f},  {0.4f, 0.4f},
                                  {0.0f, 0.8f},
  };

  /// dray::Array<Coord> nodes_array(linear_triangle, 3);
  /// const int poly_order = 1;
  dray::Array<Coord> nodes_array(quadratic_triangle, 6);
  const int poly_order = 2;

  /// img_buffer = dray::SurfaceTriangle().execute<float>(c_width, c_height, nodes_array, poly_order, 50000);
  img_buffer = dray::SurfaceTriangle().execute<float>(c_width, c_height, nodes_array, poly_order, 100);

  // Save image.
  dray::PNGEncoder png_encoder;
  png_encoder.encode( (float *) img_buffer.get_host_ptr(), c_width, c_height);
  png_encoder.save(output_file + ".png");
}