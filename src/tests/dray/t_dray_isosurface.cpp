// Copyright 2019 Lawrence Livermore National Security, LLC and other
// Devil Ray Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (BSD-3-Clause)

#include "t_utils.hpp"
#include "test_config.h"
#include "gtest/gtest.h"

#include <dray/camera.hpp>
#include <dray/filters/isosurface.hpp>
#include <dray/io/blueprint_reader.hpp>
#include <dray/shaders.hpp>

TEST (dray_isosurface, simple)
{
  std::string output_path = prepare_output_dir ();
  std::string output_file =
  conduit::utils::join_file_path (output_path, "isosurface_simple");
  remove_test_image (output_file);

  std::string root_file = std::string (DATA_DIR) + "taylor_green.cycle_000190.root";

  dray::nDataSet dataset = dray::BlueprintReader::nload (root_file);

  // Camera
  const int c_width = 1024;
  const int c_height = 1024;
  dray::Camera camera;
  camera.set_width (c_width);
  camera.set_height (c_height);
  camera.azimuth(-40);

  camera.reset_to_bounds (dataset.topology()->bounds());
  dray::Array<dray::Ray> rays;
  camera.create_rays (rays);
  dray::Framebuffer framebuffer (camera.get_width (), camera.get_height ());

  dray::ColorTable color_table ("ColdAndHot");
  // dray::Vec<float,3> normal;

  const float isoval = 0.09;

  dray::Isosurface isosurface;
  isosurface.set_field ("velocity_x");
  isosurface.set_color_table (color_table);
  isosurface.set_iso_value (isoval);
  isosurface.execute (dataset, rays, framebuffer);

  framebuffer.save (output_file);
  EXPECT_TRUE (check_test_image (output_file));
}

TEST (dray_isosurface, complex)
{
  std::string output_path = prepare_output_dir ();
  std::string output_file =
  conduit::utils::join_file_path (output_path, "isosurface");
  remove_test_image (output_file);

  std::string root_file = std::string (DATA_DIR) + "taylor_green.cycle_001860.root";

  dray::nDataSet dataset = dray::BlueprintReader::nload (root_file);

  // Camera
  const int c_width = 1024;
  const int c_height = 1024;
  dray::Camera camera;
  camera.set_width (c_width);
  camera.set_height (c_height);
  camera.azimuth(-40);

  camera.reset_to_bounds (dataset.topology()->bounds());
  dray::Array<dray::Ray> rays;
  camera.create_rays (rays);
  dray::Framebuffer framebuffer (camera.get_width (), camera.get_height ());

  dray::ColorTable color_table ("ColdAndHot");
  // dray::Vec<float,3> normal;

  const float isoval = 0.09;

  dray::Isosurface isosurface;
  isosurface.set_field ("velocity_x");
  isosurface.set_color_table (color_table);
  isosurface.set_iso_value (isoval);
  isosurface.execute (dataset, rays, framebuffer);

  framebuffer.save (output_file);
  EXPECT_TRUE (check_test_image (output_file));
}
