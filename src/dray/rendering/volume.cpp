// Copyright 2019 Lawrence Livermore National Security, LLC and other
// Devil Ray Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (BSD-3-Clause)
#include <dray/rendering/volume.hpp>
#include <dray/rendering/colors.hpp>
#include <dray/dispatcher.hpp>
#include <dray/device_color_map.hpp>
#include <dray/rendering/device_framebuffer.hpp>

#include <dray/utils/data_logger.hpp>

#include <dray/GridFunction/device_mesh.hpp>
#include <dray/GridFunction/device_field.hpp>

namespace dray
{
namespace detail
{

template<typename MeshType, typename FieldType>
DRAY_EXEC
void scalar_gradient(const Location &loc,
                     MeshType &mesh,
                     FieldType &field,
                     Float &scalar,
                     Vec<Float,3> &gradient)
{

  // i think we need this to oreient the deriv
  Vec<Vec<Float, 3>, 3> jac_vec;
  Vec<Float, 3> world_pos = // don't need this but we need the jac
    mesh.get_elem(loc.m_cell_id).eval_d(loc.m_ref_pt, jac_vec);

  Vec<Vec<Float, 1>, 3> field_deriv;
  scalar =
    field.get_elem(loc.m_cell_id).eval_d(loc.m_ref_pt, field_deriv)[0];

  Matrix<Float, 3, 3> jacobian_matrix;
  Matrix<Float, 1, 3> gradient_ref;
  for(int32 rdim = 0; rdim < 3; ++rdim)
  {
    jacobian_matrix.set_col(rdim, jac_vec[rdim]);
    gradient_ref.set_col(rdim, field_deriv[rdim]);
  }

  bool inv_valid;
  const Matrix<Float, 3, 3> j_inv = matrix_inverse(jacobian_matrix, inv_valid);
  //TODO How to handle the case that inv_valid == false?
  const Matrix<Float, 1, 3> gradient_mat = gradient_ref * j_inv;
  gradient = gradient_mat.get_row(0);
}


} // namespace detail

// ------------------------------------------------------------------------
Volume::Volume(DataSet &data_set)
  : Traceable(data_set),
    m_samples(100)
{
  // add some default alpha
  ColorTable table = m_color_map.color_table();
  table.add_alpha(0.1000, .0f);
  table.add_alpha(1.0000, .7f);
  m_color_map.color_table(table);
}

// ------------------------------------------------------------------------
Volume::~Volume()
{
}

// ------------------------------------------------------------------------
bool
Volume::is_volume() const
{
  return true;
}

// ------------------------------------------------------------------------
struct IntegrateFunctor
{
  Volume *m_volume;
  Array<Ray> *m_rays;
  Framebuffer m_framebuffer;
  Array<PointLight> m_lights;
  IntegrateFunctor(Volume *volume,
                   Array<Ray> *rays,
                   Framebuffer &fb,
                   Array<PointLight> &lights
                   )
    : m_volume(volume),
      m_rays(rays),
      m_framebuffer(fb),
      m_lights(lights)
  {
  }

  template<typename TopologyType, typename FieldType>
  void operator()(TopologyType &topo, FieldType &field)
  {
    m_volume->integrate(topo.mesh(), field, *m_rays, m_framebuffer, m_lights);
  }
};
// ------------------------------------------------------------------------

void
Volume::integrate(Array<Ray> &rays, Framebuffer &fb, Array<PointLight> &lights)
{
  if(m_field_name == "")
  {
    DRAY_ERROR("Field never set");
  }

  TopologyBase *topo = m_data_set.topology();
  FieldBase *field = m_data_set.field(m_field_name);

  IntegrateFunctor func(this, &rays, fb, lights);
  dispatch_3d(topo, field, func);
}
// ------------------------------------------------------------------------

template<typename MeshElement, typename FieldElement>
void Volume::integrate(Mesh<MeshElement> &mesh,
                        Field<FieldElement> &field,
                        Array<Ray> &rays,
                        Framebuffer &fb,
                        Array<PointLight> &lights)
{
  DRAY_LOG_OPEN("volume");

  assert(m_field_name != "");

  constexpr float32 correction_scalar = 10.f;
  float32 ratio = correction_scalar / m_samples;
  ColorMap corrected = m_color_map;
  ColorTable table = corrected.color_table();
  corrected.color_table(table.correct_opacity(ratio));

  dray::AABB<> bounds = mesh.get_bounds();
  dray::float32 mag = (bounds.max() - bounds.min()).magnitude();
  const float32 sample_dist = mag / dray::float32(m_samples);

  const int32 num_elems = mesh.get_num_elem();

  DRAY_LOG_ENTRY("samples", m_samples);
  DRAY_LOG_ENTRY("sample_distance", sample_dist);
  DRAY_LOG_ENTRY("cells", num_elems);
  // Start the rays out at the min distance from calc ray start.
  // Note: Rays that have missed the mesh bounds will have near >= far,
  //       so after the copy, we can detect misses as dist >= far.

  // Initial compaction: Literally remove the rays which totally miss the mesh.
  Array<Ray> active_rays = remove_missed_rays(rays, mesh.get_bounds());

  const int32 ray_size = active_rays.size();
  const Ray *rays_ptr = active_rays.get_device_ptr_const();

  // complicated device stuff
  DeviceMesh<MeshElement> device_mesh(mesh);
  DeviceFramebuffer d_framebuffer(fb);
  DeviceField<FieldElement> device_field(field);

  if(!m_color_map.range_set())
  {
    std::vector<Range> ranges  = m_data_set.field(m_field_name)->range();
    if(ranges.size() != 1)
    {
      DRAY_ERROR("Expected 1 range component, got "<<ranges.size());
    }
    m_color_map.scalar_range(ranges[0]);
  }

  DeviceColorMap d_color_map(m_color_map);

  const PointLight *light_ptr = lights.get_device_ptr_const();
  const int32 num_lights = lights.size();

  // TODO: somehow load balance based on far - near

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, ray_size), [=] DRAY_LAMBDA (int32 i)
  {
    const Ray ray = rays_ptr[i];
    // advance the ray one step
    Float distance = ray.m_near + sample_dist;
    Vec4f color = {0.f, 0.f, 0.f, 0.f};;
    while(distance < ray.m_far)
    {
      //Vec<Float,3> point = ray.m_orig + ray.m_dir * distance;
      Vec<Float,3> point = ray.m_orig + distance * ray.m_dir;
      Location loc = device_mesh.locate(point);
      if(loc.m_cell_id != -1)
      {
        Vec<Float,3> gradient;
        Float scalar;
        detail::scalar_gradient(loc, device_mesh, device_field, scalar, gradient);
        Vec4f sample_color = d_color_map.color(scalar);

        //composite
        blend(color, sample_color);
        if(color[3] > 0.95f)
        {
          // terminate
          distance = ray.m_far;
        }
      }

      distance += sample_dist;
    }
    Vec4f back_color = d_framebuffer.m_colors[ray.m_pixel_id];
    blend(color, back_color);
    d_framebuffer.m_colors[ray.m_pixel_id] = color;
    // should this be first valid sample or even set this?
    //d_framebuffer.m_depths[pid] = hit.m_dist;

  });

  DRAY_LOG_CLOSE();
}

Array<RayHit> Volume::nearest_hit(Array<Ray> &rays)
{
  // this is a placeholder
  // Possible implementations include hitting the bounding box
  // or actually hitting the external faces. When we support mpi
  // volume rendering, we will need to extract partial composites
  // since there is no promise, ever, about domain decomposition
  Array<RayHit> hits;
  DRAY_ERROR("not implemented");
  return hits;
}
} // namespace dray