#include "gtest/gtest.h"
#include "test_config.h"
#include "t_utils.hpp"

#include <dray/camera.hpp>
#include <dray/shaders.hpp>
#include <dray/io/mfem_reader.hpp>
#include <dray/filters/slice.hpp>
#include <dray/utils/png_encoder.hpp>

void setup_camera(dray::Camera &camera)
{
  camera.set_width(1024);
  camera.set_height(1024);

  dray::Vec<dray::float32,3> pos;
  pos[0] = .5f;
  pos[1] = -1.5f;
  pos[2] = .5f;
  camera.set_up(dray::make_vec3f(0,0,1));
  camera.set_pos(pos);
  camera.set_look_at(dray::make_vec3f(0.5, 0.5, 0.5));
}

TEST(dray_slice, dray_slice)
{
  std::string output_path = prepare_output_dir();
  std::string output_file = conduit::utils::join_file_path(output_path, "slice");
  remove_test_image(output_file);

  std::string file_name = std::string(DATA_DIR) + "taylor_green/Laghos";

  int cycle = 457;
  dray::DataSet<float> dataset = dray::MFEMReader::load32(file_name, cycle);

  dray::Camera camera;
  setup_camera(camera);

  dray::Array<dray::ray32> rays;
  camera.create_rays(rays);

  dray::PointLightSource light;
  //light.m_pos = {6.f, 3.f, 5.f};
  light.m_pos = {1.2f, -0.15f, 0.4f};
  light.m_amb = {0.3f, 0.3f, 0.3f};
  light.m_diff = {0.70f, 0.70f, 0.70f};
  light.m_spec = {0.30f, 0.30f, 0.30f};
  light.m_spec_pow = 90.0;
  dray::Shader::set_light_properties(light);

  dray::Vec<float,3> point;
  point[0] = 0.5f;
  point[1] = 0.5f;
  point[2] = 0.5f;

  //dray::Vec<float,3> normal;

  dray::Slice slicer;
  slicer.set_field("Velocity_y");
  slicer.set_point(point);
  dray::Array<dray::Vec<dray::float32,4>> color_buffer;
  color_buffer = slicer.execute(rays, dataset);

  dray::PNGEncoder png_encoder;

  png_encoder.encode( (float *) color_buffer.get_host_ptr(),
                      camera.get_width(),
                      camera.get_height() );

  png_encoder.save(output_file + ".png");
  EXPECT_TRUE(check_test_image(output_file));

}

