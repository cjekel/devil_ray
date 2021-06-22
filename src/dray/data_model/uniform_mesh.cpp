// Copyright 2019 Lawrence Livermore National Security, LLC and other
// Devil Ray Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (BSD-3-Clause)

#include <dray/data_model/uniform_mesh.hpp>
#include <dray/data_model/uniform_device_mesh.hpp>
#include <dray/data_model/structured_indexing.hpp>
#include <dray/data_model/low_order_field.hpp>

#include <dray/error.hpp>
#include <dray/error_check.hpp>
#include <dray/policies.hpp>
#include <dray/utils/data_logger.hpp>

namespace dray
{
UniformMesh::UniformMesh(const Vec<Float,3> &spacing,
                         const Vec<Float,3> &origin,
                         const Vec<int32,3> &dims)
  : m_spacing(spacing),
    m_origin(origin),
    m_dims(dims)
{
}

UniformMesh::~UniformMesh()
{

}

int32
UniformMesh::cells() const
{
  return m_dims[0] * m_dims[1] * m_dims[2];
}

int32
UniformMesh::order() const
{
  return 1;
}

int32
UniformMesh::dims() const
{
  return 3;
}

std::string
UniformMesh::type_name() const
{
  return "uniform";
}

AABB<3>
UniformMesh::bounds()
{
  AABB<3> bounds;
  bounds.include(m_origin);
  Vec<Float,3> upper;
  upper[0] = m_origin[0] + m_spacing[0] * Float(m_dims[0]);
  upper[1] = m_origin[1] + m_spacing[1] * Float(m_dims[1]);
  upper[2] = m_origin[2] + m_spacing[2] * Float(m_dims[2]);
  bounds.include(upper);
  return bounds;
}

Array<Location>
UniformMesh::locate(Array<Vec<Float, 3>> &wpoints)
{
  DRAY_LOG_OPEN ("uniform_locate");

  UniformDeviceMesh device_mesh(*this);
  Array<Location> locs;
  const int32 size = wpoints.size();
  locs.resize(size);
  Location *locs_ptr = locs.get_device_ptr();
  const Vec<Float,3> *points_ptr = wpoints.get_device_ptr_const();

  RAJA::forall<for_policy> (RAJA::RangeSegment (0, size), [=] DRAY_LAMBDA (int32 i) {

    Location loc = { -1, { -1.f, -1.f, -1.f } };
    const Vec<Float, 3> target_pt = points_ptr[i];
    locs_ptr[i] = device_mesh.locate(target_pt);
  });
  DRAY_ERROR_CHECK();
  DRAY_LOG_CLOSE();

  return locs;
}


void UniformMesh::to_node(conduit::Node &n_topo)
{
  n_topo.reset();
  n_topo["type_name"] = type_name();
  /// n_topo["order"] = m_mesh.get_poly_order();

  throw std::logic_error(("Not implemented to_node()! " __FILE__));

  /// conduit::Node &n_gf = n_topo["grid_function"];
  /// GridFunction<3u> gf = m_mesh.get_dof_data();
  /// gf.to_node(n_gf);
}


Vec<int32,3>
UniformMesh::cell_dims() const
{
  return m_dims;
}

Vec<Float,3>
UniformMesh::spacing() const
{
  return m_spacing;
}

Vec<Float,3>
UniformMesh::origin() const
{
  return m_origin;
}

void
UniformMesh::eval(Field *field, const Array<Location> &locs, Array<Float> &values)
{
  if(field->mesh_name() != name())
  {
    DRAY_ERROR("eval: field mesh association '"<<field->mesh_name()
               <<"' must match this mesh's name '"<<m_name<<"'");
  }

  LowOrderField *low = static_cast<LowOrderField*>(field);
  if(low == nullptr)
  {
    DRAY_ERROR("Uniform mesh currenly only supports low order fields");
  }

  const Location *loc_ptr = locs.get_device_ptr_const();
  const Float *values_ptr = low->values().get_device_ptr_const();
  const int32 size = locs.size();
  if(values.size() != size)
  {
    values.resize(size);
  }
  Float *res_ptr = values.get_device_ptr();
  const Vec<int32,3> point_dims = {{m_dims[0],
                                    m_dims[1],
                                    m_dims[2]}};
  const Vec<int32,3> cell_dims = m_dims;

  bool is_vertex = low->assoc() == LowOrderField::Assoc::Vertex;
  if(is_vertex)
  {
    const int32 x_stride = 1;
    const int32 y_stride = point_dims[0];
    const int32 z_stride = point_dims[0] * point_dims[1];

    RAJA::forall<for_policy> (RAJA::RangeSegment (0, size), [=] DRAY_LAMBDA (int32 ii)
    {
      const Location loc = loc_ptr[ii];
      const Vec<int32,3> bottom_left = logical_index_3d(loc.m_cell_id, cell_dims);
      // bottom left flat index
      const int32 p0 = flat_index_3d(bottom_left, point_dims);
      Float vals[8];
      vals[0] = values_ptr[p0];
      vals[1] = values_ptr[p0 + x_stride];
      vals[2] = values_ptr[p0 + y_stride];
      vals[3] = values_ptr[p0 + x_stride + y_stride];
      vals[4] = values_ptr[p0 + z_stride];
      vals[5] = values_ptr[p0 + x_stride + z_stride];
      vals[6] = values_ptr[p0 + y_stride + z_stride];
      vals[7] = values_ptr[p0 + x_stride + y_stride + z_stride];

      // values are stored in lexagraphical order
      // lerp the x sides bottom
      Float t01 = lerp(vals[0], vals[1], loc.m_ref_pt[0]);
      Float t45 = lerp(vals[4], vals[5], loc.m_ref_pt[0]);
      // lerp the x sides top
      Float t23 = lerp(vals[2], vals[3], loc.m_ref_pt[0]);
      Float t67 = lerp(vals[6], vals[7], loc.m_ref_pt[0]);
      // now lerp in y
      Float y0 = lerp(t01, t45, loc.m_ref_pt[1]);
      Float y1 = lerp(t23, t67, loc.m_ref_pt[1]);
      // and in z
      Float res = lerp(y0,y1, loc.m_ref_pt[2]);
      res_ptr[ii] = res;
    });
  }
  else
  {
    // element centered variable
    RAJA::forall<for_policy> (RAJA::RangeSegment (0, size), [=] DRAY_LAMBDA (int32 ii)
    {
      res_ptr[ii] = values_ptr[ii];
    });
  }

}

void UniformMesh::to_blueprint(conduit::Node &n_dataset)
{
  // hard coded topology and coords names;
  const std::string topo_name = this->name();
  const std::string coord_name = "coords_"+topo_name;

  conduit::Node &n_topo = n_dataset["topologies/"+topo_name];
  n_topo["coordset"] = coord_name;
  n_topo["type"] = "uniform";

  conduit::Node &n_coords = n_dataset["coordsets/"+coord_name];
  n_coords["type"] = "uniform";
  n_coords["dims/i"] = m_dims[0] + 1;
  n_coords["dims/j"] = m_dims[1] + 1;
  n_coords["dims/k"] = m_dims[2] + 1;

  n_coords["origin/x"] = m_origin[0];
  n_coords["origin/y"] = m_origin[1];
  n_coords["origin/z"] = m_origin[2];

  n_coords["spacing/dx"] = m_spacing[0];
  n_coords["spacing/dy"] = m_spacing[1];
  n_coords["spacing/dz"] = m_spacing[2];
}

} // namespace dray