#include <dray/filters/first_scatter.hpp>
#include <dray/uniform_topology.hpp>
#include <dray/error.hpp>
#include <dray/policies.hpp>
#include <dray/utils/point_writer.hpp>
#include <dray/utils/png_encoder.hpp>
#include <dray/data_model/low_order_field.hpp>
#include <dray/array_utils.hpp>
#include <dray/device_array.hpp>
#include <dray/spherical_harmonics.hpp>
#include <dray/uniform_faces.hpp>

namespace dray
{

namespace detail
{

// Returns cell center for ever cell in topo.
static Array<Vec<Float,3>> cell_centers(UniformTopology &topo)
{

  const Vec<int32,3> cell_dims = topo.cell_dims();
  const Vec<Float,3> origin = topo.origin();
  const Vec<Float,3> spacing = topo.spacing();

  const int32 num_cells = cell_dims[0] * cell_dims[1] * cell_dims[2];

  Array<Vec<Float,3>> locations;
  locations.resize(num_cells);
  Vec<Float,3> *loc_ptr = locations.get_device_ptr();

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, num_cells), [=] DRAY_LAMBDA (int32 index)
  {
    Vec<int32,3> cell_id;
    cell_id[0] = index % cell_dims[0];
    cell_id[1] = (index / cell_dims[0]) % cell_dims[1];
    cell_id[2] = index / (cell_dims[0] * cell_dims[1]);

    Vec<Float,3> loc;
    for(int32 i = 0; i < 3; ++i)
    {
      loc[i] = origin[i] + Float(cell_id[i]) * spacing[i] + spacing[i] * 0.5f;
    }

    loc_ptr[index] = loc;
  });

  return locations;
}


// Returns cell center for every cell of in topo listed in source_cells.
static Array<Vec<Float, 3>> cell_centers_from_id(const UniformTopology &topo,
                                                 Array<int32> source_cells)
{
  const Vec<int32,3> cell_dims = topo.cell_dims();
  const Vec<Float,3> origin = topo.origin();
  const Vec<Float,3> spacing = topo.spacing();

  const int32 num_cells = source_cells.size();

  Array<Vec<Float,3>> locations;
  locations.resize(num_cells);
  Vec<Float,3> *loc_ptr = locations.get_device_ptr();

  const int32 * source_cells_ptr = source_cells.get_device_ptr_const();

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, num_cells), [=] DRAY_LAMBDA (int32 index)
  {
    const int32 linear_index = source_cells_ptr[index];

    Vec<int32,3> cell_id;
    cell_id[0] = linear_index % cell_dims[0];
    cell_id[1] = (linear_index / cell_dims[0]) % cell_dims[1];
    cell_id[2] = linear_index / (cell_dims[0] * cell_dims[1]);

    Vec<Float,3> loc;
    for(int32 i = 0; i < 3; ++i)
    {
      loc[i] = origin[i] + Float(cell_id[i]) * spacing[i] + spacing[i] * 0.5f;
    }

    loc_ptr[index] = loc;
  });

  return locations;
}


// Returns cell center of every cell in topo
// that has at least one nonzero emission value.
// The nonzero_list array (same length as return)
// contains the original indices of the nonzero cells.
static Array<Vec<Float,3>> cell_centers_nonzero(UniformTopology &topo,
                                                LowOrderField * emission,
                                                int32 _num_moments,
                                                Array<int32> &nonzero_list)
{

  const Vec<int32,3> cell_dims = topo.cell_dims();
  const Vec<Float,3> origin = topo.origin();
  const Vec<Float,3> spacing = topo.spacing();
  const int32 num_moments = _num_moments;

  // Include a zone if any component _of_any_moment_ is nonzero.
  Array<int32> nonzero_moments_list = index_any_nonzero(emission->values());
  const int32 num_nonzero_items = nonzero_moments_list.size();
  Array<int32> uniq_flags;
  uniq_flags.resize (num_nonzero_items);
  // At first, assume all unique.
  array_memset (uniq_flags, 1);
  NonConstDeviceArray<int32> nzm_deva(nonzero_moments_list);
  NonConstDeviceArray<int32> uniq_flags_deva(uniq_flags);
  RAJA::forall<for_policy>(RAJA::RangeSegment(1, num_nonzero_items),
      [=] DRAY_LAMBDA (int32 nzm_index)
  {
    const int32 left_index = nzm_index - 1;
    const int32 zone = nzm_deva.get_item(nzm_index) / num_moments;
    const int32 left_zone = nzm_deva.get_item(left_index) / num_moments;
    if (zone == left_zone)
    {
      uniq_flags_deva.get_item(nzm_index) = 0;
    }
  });

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, num_nonzero_items),
      [=] DRAY_LAMBDA (int32 nzm_index)
  {
    const int32 zone = nzm_deva.get_item(nzm_index) / num_moments;
    nzm_deva.get_item(nzm_index) = zone;
  });

  nonzero_list = index_flags(uniq_flags, nonzero_moments_list);

  ConstDeviceArray<int32> nonzero_list_deva(nonzero_list);
  const int32 num_nonzero_cells = nonzero_list.size();

  Array<Vec<Float,3>> locations;
  locations.resize(num_nonzero_cells);
  Vec<Float,3> *loc_ptr = locations.get_device_ptr();

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, num_nonzero_cells),
      [=] DRAY_LAMBDA (int32 nz_index)
  {
    const int32 index = nonzero_list_deva.get_item(nz_index);
    Vec<int32,3> cell_id;
    cell_id[0] = index % cell_dims[0];
    cell_id[1] = (index / cell_dims[0]) % cell_dims[1];
    cell_id[2] = index / (cell_dims[0] * cell_dims[1]);

    Vec<Float,3> loc;
    for(int32 i = 0; i < 3; ++i)
    {
      loc[i] = origin[i] + Float(cell_id[i]) * spacing[i] + spacing[i] * 0.5f;
    }

    loc_ptr[nz_index] = loc;
  });

  return locations;
}

struct FS_TraversalState
{
  Vec<Float,3> m_delta_max;
  Vec<Float,3> m_delta;
  Vec<int32,3> m_voxel;
  Vec<Float,3> m_dir;

  // distance to voxel exit from initial point
  DRAY_EXEC
  Float exit() const
  {
    return min(m_delta_max[0], min(m_delta_max[1], m_delta_max[2]));
  }

  // advances to the next voxel along the ray
  DRAY_EXEC void advance()
  {
    int32 advance_dir = 0;
    for(int32 i = 1; i < 3; ++i)
    {
      if(m_delta_max[i] < m_delta_max[advance_dir])
      {
        advance_dir = i;
      }
    }
    m_delta_max[advance_dir] += m_delta[advance_dir];
    m_voxel[advance_dir] += m_dir[advance_dir] < 0.f ? -1 : 1;
  }

};

struct FS_DDATraversal
{
  const Vec<int32,3> m_dims;
  const Vec<Float,3> m_origin;
  const Vec<Float,3> m_spacing;

  FS_DDATraversal(UniformTopology &topo)
    : m_dims(topo.cell_dims()),
      m_origin(topo.origin()),
      m_spacing(topo.spacing())
  {

  }

  DRAY_EXEC
  bool is_inside(const Vec<int32, 3>& index) const
  {
    bool inside = true;
    const int32 minIndex = min(index[0], min(index[1], index[2]));
    if(minIndex < 0) inside = false;
    if(index[0] >= m_dims[0]) inside = false;
    if(index[1] >= m_dims[1]) inside = false;
    if(index[2] >= m_dims[2]) inside = false;
    return inside;
  }

  DRAY_EXEC
  int32 voxel_index(const Vec<int32, 3> &voxel) const
  {
    return voxel[0] + voxel[1] * m_dims[0] + voxel[2] * m_dims[0] * m_dims[1];
  }

  DRAY_EXEC Float
  init_traversal(const Vec<Float,3> &point,
                 const Vec<Float,3> &dir,
                 FS_TraversalState &state) const
  {
    Vec<Float, 3> temp = point;
    temp = temp - m_origin;
    state.m_voxel[0] = temp[0] / m_spacing[0];
    state.m_voxel[1] = temp[1] / m_spacing[1];
    state.m_voxel[2] = temp[2] / m_spacing[2];
    state.m_dir = dir;

    Vec<Float,3> step;
    step[0] = (dir[0] >= 0.f) ? 1.f : -1.f;
    step[1] = (dir[1] >= 0.f) ? 1.f : -1.f;
    step[2] = (dir[2] >= 0.f) ? 1.f : -1.f;

    Vec<Float,3> next_boundary;
    next_boundary[0] = (Float(state.m_voxel[0]) + step[0]) * m_spacing[0];
    next_boundary[1] = (Float(state.m_voxel[1]) + step[1]) * m_spacing[1];
    next_boundary[2] = (Float(state.m_voxel[2]) + step[2]) * m_spacing[2];

    // correct next boundary for negative directions
    if(step[0] == -1.f) next_boundary[0] += m_spacing[0];
    if(step[1] == -1.f) next_boundary[1] += m_spacing[1];
    if(step[2] == -1.f) next_boundary[2] += m_spacing[2];

    // distance to next voxel boundary
    state.m_delta_max[0] = (dir[0] != 0.f) ?
      (next_boundary[0] - (point[0] - m_origin[0])) / dir[0] : infinity<Float>();

    state.m_delta_max[1] = (dir[1] != 0.f) ?
      (next_boundary[1] - (point[1] - m_origin[1])) / dir[1] : infinity<Float>();

    state.m_delta_max[2] = (dir[2] != 0.f) ?
      (next_boundary[2] - (point[2] - m_origin[2])) / dir[2] : infinity<Float>();

    // distance along ray to traverse x,y, and z of a voxel
    state.m_delta[0] = (dir[0] != 0) ? m_spacing[0] / dir[0] * step[0] : infinity<Float>();
    state.m_delta[1] = (dir[1] != 0) ? m_spacing[1] / dir[1] * step[1] : infinity<Float>();
    state.m_delta[2] = (dir[2] != 0) ? m_spacing[2] / dir[2] * step[2] : infinity<Float>();

    Vec<Float,3> exit_boundary;
    exit_boundary[0] = step[0] < 0.f ? 0.f : Float(m_dims[0]) * m_spacing[0];
    exit_boundary[1] = step[1] < 0.f ? 0.f : Float(m_dims[1]) * m_spacing[1];
    exit_boundary[2] = step[2] < 0.f ? 0.f : Float(m_dims[2]) * m_spacing[2];

    // Masado questions these lines
    if(step[0] == -1.f) exit_boundary[0] += m_spacing[0];
    if(step[1] == -1.f) exit_boundary[1] += m_spacing[1];
    if(step[2] == -1.f) exit_boundary[2] += m_spacing[2];

    Vec<Float,3> exit_dist;
    // distance to grid exit
    exit_dist[0] = (dir[0] != 0.f) ?
      (exit_boundary[0] - (point[0] - m_origin[0])) / dir[0] : infinity<Float>();

    exit_dist[1] = (dir[1] != 0.f) ?
      (exit_boundary[1] - (point[1] - m_origin[1])) / dir[1] : infinity<Float>();

    exit_dist[2] = (dir[2] != 0.f) ?
      (exit_boundary[2] - (point[2] - m_origin[2])) / dir[2] : infinity<Float>();

    //std::cout<<"Init voxel "<<voxel<<"\n";

    return min(exit_dist[0], min(exit_dist[1], exit_dist[2]));
  }



};


} // namespace detail

FirstScatter::FirstScatter()
  : m_legendre_order(0),
    m_sigs(0.0f),
    m_ret(ReturnFirstScatter)
{

}


// Returns flattened array of size num_destinations * num_sources.
// Results for each source and destination, with sources varying faster.
// Assumes that absorption is isotropic (no dependence on moments).
static
Array<Float>
go_trace(Array<Vec<Float,3>> &destinations,
         Array<Vec<Float,3>> &ray_sources,
         UniformTopology &topo,
         LowOrderField *absorption)
{
  // input
  const detail::FS_DDATraversal dda(topo);
  const Vec<Float,3> *destn_ptr = destinations.get_device_ptr_const();
  const Vec<Float,3> *ray_src_ptr = ray_sources.get_device_ptr_const();
  const int32 size_ray_srcs = ray_sources.size();
  const int32 size = destinations.size();
  const ConstDeviceArray<Float> absorption_arr( absorption->values() );

  const int32 ncomp = absorption_arr.ncomp();

  // output
  Array<Float> path_lengths;
  path_lengths.resize(size * size_ray_srcs, ncomp);
  NonConstDeviceArray<Float> length_arr( path_lengths );

  RAJA::forall<for_policy>(RAJA::RangeSegment(0, size), [=] DRAY_LAMBDA (int32 index)
  {
    Float * res = new Float[ncomp];

    Vec<Float,3> destn = destn_ptr[index];
    for(int ray_src = 0; ray_src < size_ray_srcs; ++ray_src)
    {
      Vec<Float,3> loc = ray_src_ptr[ray_src];
      Vec<Float,3> dir = destn - loc;
      Float distance_left = dir.magnitude();
      if (dir.magnitude2() == 0.0)
        continue;

      dir.normalize();
      detail::FS_TraversalState state;
      dda.init_traversal(loc, dir, state);

      Float distance = 0.f;
      for (int32 component = 0; component < ncomp; ++component)
        res[component] = 1.f;

      while(dda.is_inside(state.m_voxel) && distance_left > 0.0f)
      {
        const Float voxel_exit = state.exit();
        Float length = voxel_exit - distance;
        length = (length < distance_left ? length : distance_left);

        const int32 cell_id = dda.voxel_index(state.m_voxel);
        for (int32 component = 0; component < ncomp; ++component)
        {
          const Float absorb
              = exp(-absorption_arr.get_item(cell_id, component) * length);

          res[component] = res[component] * absorb;
        }
        // this will get more complicated with MPI and messed up
        // metis domain decompositions

        distance_left -= length;
        distance = voxel_exit;
        state.advance();
      }
      // Directions matter.
      // Instead of summing over all sources,
      // return result from each source separately.
      for (int32 component = 0; component < ncomp; ++component)
      {
        length_arr.get_item(size_ray_srcs * index + ray_src, component)
            = res[component];
      }
    }

    delete [] res;
  });
  return path_lengths;
}



// Assumes that emission uses anisotropic representation,
// i.e. num_items == num_moments * num_zones
// and moments vary faster than zones.
Array<Float> integrate_moments(Array<Vec<Float,3>> &destinations,
                               int32 legendre_order,
                               Array<Float> &path_lengths,
                               Array<Vec<Float,3>> &ray_sources,
                               Array<int32> &source_cells,
                               Float _cell_volume,
                               LowOrderField *emission);

Array<Float> integrate_faces_to_cell_moments(
    Array<Vec<Float,3>> &destinations,
    Array<Float> &weights,
    const QuadratureRule &quadrature,
    int32 _legendre_order,
    Array<Float> &path_lengths,
    Array<Vec<Float,3>> &ray_sources,
    Array<int32> &source_cells,
    Float _cell_volume,
    LowOrderField *emission,
    LowOrderField *total_cross_section,
    const UniformFaces &face_map);


Array<Float> pointwise_cell_fluxes(UniformTopology &topo,
                                   LowOrderField *total_cross_section,
                                   Array<int32> source_cells,
                                   LowOrderField *emission,
                                   int32 legendre_order)
{
  // rays = {all cell centers} x {source cell centers}
  Array<Vec<Float, 3>> destinations = detail::cell_centers(topo);
  Array<Vec<Float, 3>> source_loc = detail::cell_centers_from_id(topo, source_cells);
  Array<Float> plengths = go_trace(destinations, source_loc, topo, total_cross_section);

  const Vec<Float, 3> spacing = topo.spacing();
  const Float cell_volume = (spacing[0] * spacing[1] * spacing[2]);

  // for each ray,
  //   evaluate the source intensity at the destination,
  //   attenuate the source term due to ray path length,
  //   integrate into moment representation of pointwise angular flux.
  Array<Float> cell_center_moments = integrate_moments(
      destinations,
      legendre_order,
      plengths,
      source_loc,
      source_cells,
      cell_volume,
      emission);

  return cell_center_moments;
}

Array<Float> averaged_cell_fluxes(UniformTopology &topo,
                                  const UniformFaces &face_map,
                                  const QuadratureRule &quadrature,
                                  LowOrderField *total_cross_section,
                                  Array<int32> source_cells,
                                  LowOrderField *emission,
                                  int32 legendre_order)
{
  // rays = {all face centers} x {source cell centers}
  Array<Vec<Float, 3>> destinations;
  Array<Float> weights;
  destinations.resize(face_map.num_total_faces()
                      * quadrature.points()
                      * quadrature.points());
  weights.resize(face_map.num_total_faces()
                      * quadrature.points()
                      * quadrature.points());
  face_map.fill_total_faces(destinations.get_host_ptr(),
                            weights.get_host_ptr(),
                            quadrature);
  Array<Vec<Float, 3>> source_loc = detail::cell_centers_from_id(topo, source_cells);
  Array<Float> plengths = go_trace(destinations, source_loc, topo, total_cross_section);

  const Vec<Float, 3> spacing = topo.spacing();
  const Float cell_volume = (spacing[0] * spacing[1] * spacing[2]);

  // for each ray,
  //   evaluate the source intensity at the destination (face),
  //   attenuate the source term due to ray path length,
  //   multiply by cosine factor for flux through oriented face,
  //   integrate and sum into moment representation of cell-averaged current.
  // Divide by (cell volume) * (Sigma_t) to get the cell-averaged angular flux.
  Array<Float> cell_avg_moments = integrate_faces_to_cell_moments(
      destinations,
      weights,
      quadrature,
      legendre_order,
      plengths,
      source_loc,
      source_cells,
      cell_volume,
      emission,
      total_cross_section,
      face_map);

  return cell_avg_moments;
}

void scatter(Array<Float> destination_moments,
             int32 num_moments,
             Float m_sigs,    //TODO m_sigs should be a matrix-valued field
             LowOrderField *first_scatter_out);

void copy_moments(Array<Float> destination_moments,
             int32 _num_moments,
             LowOrderField *uncollided_flux_out);

Float popcount(Array<Float> destination_moments, int32 _num_moments, Float _cell_volume);

void FirstScatter::execute(DataSet &data_set)
{
  if(m_total_cross_section_field == "")
  {
    DRAY_ERROR("Total cross section field not set");
  }

  if(m_emission_field == "")
  {
    DRAY_ERROR("Emission field not set");
  }

  if(!data_set.has_field(m_total_cross_section_field))
  {
    DRAY_ERROR("No total cross section field '"<<m_total_cross_section_field<<"' found");
  }

  if(!data_set.has_field(m_emission_field))
  {
    DRAY_ERROR("No emission field '"<<m_emission_field<<"' found");
  }

  if(!data_set.has_field(m_overwrite_first_scatter_field))
  {
    DRAY_ERROR("No output first scatter field '"<<m_overwrite_first_scatter_field<<"' found");
  }

  Mesh *mesh = data_set.mesh();
  if(dynamic_cast<UniformTopology*>(mesh) != nullptr)
  {
    std::cout<<"Boom\n";

    UniformTopology *uni_topo = dynamic_cast<UniformTopology*>(mesh);
    LowOrderField *total_cross_section = dynamic_cast<LowOrderField*>(data_set.field(m_total_cross_section_field));
    LowOrderField *emission = dynamic_cast<LowOrderField*>(data_set.field(m_emission_field));
    LowOrderField *first_scatter_out = dynamic_cast<LowOrderField*>(data_set.field(m_overwrite_first_scatter_field));

    if(total_cross_section->assoc() != LowOrderField::Assoc::Element)
    {
      DRAY_ERROR("Total cross section field must be associated with elements");
    }

    if(emission->assoc() != LowOrderField::Assoc::Element)
    {
      DRAY_ERROR("Emission field must be associated with elements");
    }

    if(first_scatter_out->assoc() != LowOrderField::Assoc::Element)
    {
      DRAY_ERROR("First scatter field must be associated with elements");
    }

    const int32 legendre_order = this->legendre_order();

    const int32 num_moments = (legendre_order+1)*(legendre_order+1);

    if (emission->values().size()
        != total_cross_section->values().size() * num_moments)
    {
      DRAY_ERROR("Emission field must have moments.");
    }

    if (first_scatter_out->values().size()
        != total_cross_section->values().size() * num_moments)
    {
      DRAY_ERROR("First scatter output field must have moments.");
    }

    Array<int32> source_cells;
    Array<Vec<Float,3>> ray_sources = detail::cell_centers_nonzero(*uni_topo, emission, num_moments, source_cells);

    const size_t possible_sources = emission->values().size() / num_moments;
    const size_t actual_sources = source_cells.size();
    /// std::cout << actual_sources << " of " << possible_sources << " cells are sources.\n";

    // TODO add output to validate the evaluation of the flux based on the centers.
    Array<Float> cell_moments;

    enum ResultFluxType { Pointwise, CellAveraged };
    const ResultFluxType result_flux_type = CellAveraged;  // TODO property

    const Vec<Float, 3> spacing = uni_topo->spacing();
    const Float cell_volume = (spacing[0] * spacing[1] * spacing[2]);

    if (result_flux_type == Pointwise)  // pointwise at cell centers
    {
      Array<Float> plengths = pointwise_cell_fluxes(*uni_topo,
                                                    total_cross_section,
                                                    source_cells,
                                                    emission,
                                                    legendre_order);
    }

    else if (result_flux_type == CellAveraged)  // average from faces to cells
    {
      const UniformFaces face_map = UniformFaces::from_uniform_topo(*uni_topo);
      const QuadratureRule quadrature = QuadratureRule::create(this->face_quadrature_degree());
      cell_moments = averaged_cell_fluxes(*uni_topo,
                                          face_map,
                                          quadrature,
                                          total_cross_section,
                                          source_cells,
                                          emission,
                                          legendre_order);
    }

    // TODO compute cell-averaged flux using the divergence theorem and faces.
    // While looping over cell centers, use the face_map to lookup adjacent faces.

    if (m_ret == ReturnFirstScatter)
    {
      //TODO use SigmaS matrix variable to compute scattering.
      scatter(cell_moments, num_moments, m_sigs, first_scatter_out);
      std::cout << "Scattered.\n";
    }
    else
    {
      copy_moments(cell_moments, num_moments, first_scatter_out);
      std::cout << "Uncollided flux.\n";
      fprintf(stdout, "DRay pop count: %e\n", popcount(cell_moments, num_moments, cell_volume));
    }
  }
  else
  {
    DRAY_ERROR("FirstScatter filter only supports UniformTopology");
  }
}


void ///Collection
FirstScatter::execute(Collection &collection)
{
  /// Collection res;
  for(int32 i = 0; i < collection.local_size(); ++i)
  {
    DataSet data_set = collection.domain(i);
    if(data_set.mesh()->dims() == 3)
    {
      /// DataSet result_data_set =
      this->execute(data_set);
      /// res.add_domain(result_data_set);
    }
    else
    {
      // just pass it through
      /// res.add_domain(data_set);
    }
  }
  /// return res;
}


void FirstScatter::total_cross_section_field(const std::string field_name)
{
  m_total_cross_section_field = field_name;
}

void FirstScatter::emission_field(const std::string field_name)
{
  m_emission_field = field_name;
}

void FirstScatter::overwrite_first_scatter_field(const std::string field_name)
{
  m_overwrite_first_scatter_field = field_name;
}

int32 FirstScatter::legendre_order() const
{
  return m_legendre_order;
}

void FirstScatter::legendre_order(int32 l_order)
{
  m_legendre_order = l_order;
}

int32 FirstScatter::face_quadrature_degree() const
{
  return m_face_quadrature_degree;
}

void FirstScatter::face_quadrature_degree(int32 degree)
{
  m_face_quadrature_degree = degree;
}

void FirstScatter::uniform_isotropic_scattering(Float sigs)
{
  m_sigs = sigs;
}

void FirstScatter::return_type(ReturnType ret)
{
  m_ret = ret;
}



Array<Float> integrate_moments(Array<Vec<Float,3>> &destinations,
                               int32 _legendre_order,
                               Array<Float> &path_lengths,
                               Array<Vec<Float,3>> &ray_sources,
                               Array<int32> &source_cells,
                               Float _cell_volume,
                               LowOrderField *emission)
{
  using sph_t = Float;

  const int32 ncomp = path_lengths.ncomp();
  const int32 legendre_order = _legendre_order;
  const int32 num_moments = (legendre_order + 1) * (legendre_order + 1);
  const int32 num_destinations = destinations.size();
  const int32 num_sources = ray_sources.size();

  Array<Float> destination_moments;
  destination_moments.resize(num_destinations * num_moments, ncomp);

  const Float cell_volume = _cell_volume;

  ConstDeviceArray<Vec<Float, 3>> destinations_dev(destinations);
  ConstDeviceArray<Vec<Float, 3>> ray_sources_dev(ray_sources);
  ConstDeviceArray<Float> path_lengths_dev(path_lengths);
  ConstDeviceArray<int32> source_cells_dev(source_cells);
  ConstDeviceArray<Float> emission_dev(emission->values());

  NonConstDeviceArray<Float> destination_moments_dev(destination_moments);

  const Float four_pi = 4 * pi();
  const Float sqrt_four_pi = sqrt(four_pi);
  const Float rcp_sqrt_four_pi = 1.0 / sqrt_four_pi;

  RAJA::forall<for_policy> (RAJA::RangeSegment(0, num_destinations),
      [=] DRAY_LAMBDA (int32 dest)
  {
    // Clear output.
    for (int32 nm = 0; nm < num_moments; ++nm)
      for (int32 component = 0; component < ncomp; ++component)
        destination_moments_dev.get_item(num_moments * dest + nm, component) = 0.0f;

    SphericalHarmonics<sph_t> sph(legendre_order);

    // For each source
    //   For each component
    //     For each moment
    //       Multiply-and-accumulate source term with spherical harmonic.
    const Vec<Float, 3> dest_pos = destinations_dev.get_item(dest);
    for (int32 source = 0; source < num_sources; ++source)
    {
      const Vec<Float, 3> omega = (dest_pos - ray_sources_dev.get_item(source));
      const Vec<Float, 3> omega_hat = omega.normalized();
      const Float rcp_mag2 = rcp_safe(omega.magnitude2());
      // Really should use volume-average (over source cell) of rcp_mag2.

      if (omega.magnitude2() == 0.0f)
        continue;

      const sph_t * sph_eval = sph.eval_all(omega_hat);

      const int32 source_idx = source_cells_dev.get_item(source);

      for (int32 component = 0; component < ncomp; ++component)
      {
        // Evaluate emission in the direction of omega_hat.
        Float dEmission_dV = 0.0f;
        for (int32 nm = 0; nm < num_moments; ++nm)
        {
          dEmission_dV += sph_eval[nm]
                         * sqrt_four_pi   // L_plus_times
                         * emission_dev.get_item(num_moments * source_idx + nm,
                                                 component);
        }

        const Float source_dL_dOmega
          = dEmission_dV * cell_volume * rcp_mag2;

        const Float transmitted = path_lengths_dev.get_item(
            num_sources * dest + source, component);

        const Float trans_source = transmitted * source_dL_dOmega;

        for (int32 nm = 0; nm < num_moments; ++nm)
        {
          const sph_t spherical_harmonic = sph_eval[nm];

          // Integrate
          //   \int_{4\pi} (4\pi)^{-1/2} \Ynm(\Omega) \psi(\Omega) d\Omega
          // but changing coordinates into a volume integral
          // and approximating with a constant \Omega
          // and constant r over whole volume.
          const Float contribution = spherical_harmonic * trans_source * rcp_sqrt_four_pi;

          destination_moments_dev.get_item(num_moments * dest + nm, component)
              += contribution;
        }//moments
      }//components
    }//sources
  });//destinations

  return destination_moments;
}



Array<Float> integrate_faces_to_cell_moments(
    Array<Vec<Float,3>> &face_points,
    Array<Float> &face_weights,
    const QuadratureRule &quadrature,
    int32 _legendre_order,
    Array<Float> &path_lengths,
    Array<Vec<Float,3>> &ray_sources,
    Array<int32> &source_cells,
    Float _cell_volume,
    LowOrderField *emission,
    LowOrderField *total_cross_section,
    const UniformFaces &_face_map)
{
  using sph_t = Float;

  const UniformFaces face_map = _face_map;
  const int32 ncomp = path_lengths.ncomp();
  const int32 legendre_order = _legendre_order;
  const int32 num_moments = (legendre_order + 1) * (legendre_order + 1);
  const int32 num_cells = face_map.num_total_cells();
  const int32 num_sources = ray_sources.size();
  const int32 points_per_face = quadrature.points() * quadrature.points();

  Array<Float> cell_moments;
  cell_moments.resize(num_cells * num_moments, ncomp);

  const Float cell_volume = _cell_volume;

  ConstDeviceArray<Vec<Float, 3>> face_points_dev(face_points);
  ConstDeviceArray<Float> face_weights_dev(face_weights);
  ConstDeviceArray<Vec<Float, 3>> ray_sources_dev(ray_sources);
  ConstDeviceArray<Float> path_lengths_dev(path_lengths);  // face centered
  ConstDeviceArray<int32> source_cells_dev(source_cells);
  ConstDeviceArray<Float> emission_dev(emission->values());  // cell centered
  ConstDeviceArray<Float> sigmat_dev(total_cross_section->values());  // cell centered

  NonConstDeviceArray<Float> cell_moments_dev(cell_moments);

  const Float four_pi = 4 * pi();
  const Float sqrt_four_pi = sqrt(four_pi);
  const Float rcp_sqrt_four_pi = 1.0 / sqrt_four_pi;

  RAJA::forall<for_policy> (RAJA::RangeSegment(0, num_cells),
      [=] DRAY_LAMBDA (int32 cell)
  {
    // Add self-emission, otherwise source cells can have negative flux.
    for (int32 nm = 0; nm < num_moments; ++nm)
      for (int32 component = 0; component < ncomp; ++component)
        cell_moments_dev.get_item(num_moments * cell + nm, component)
            = emission_dev.get_item(num_moments * cell + nm, component)
              / sigmat_dev.get_item(cell, component);

    SphericalHarmonics<sph_t> sph(legendre_order);

    // For each face
    //   For each quadrature point
    //     For each source
    //       For each component
    //         For each moment
    //           Multiply-and-accumulate source term with
    //               spherical harmonic, face cosine, and face area
    // And divide by (Sigmat * cell_volume)
    using FaceID = UniformFaces::FaceID;
    for (uint8 face = 0; face < FaceID::NUM_FACES; ++face)
    {
      const Vec<Float, 3> face_normal = face_map.normal(FaceID(face));
      const Float face_area = face_map.face_area(FaceID(face));
      for (int32 quad_idx = 0; quad_idx < points_per_face; ++quad_idx)
      {
        const int32 face_idx = face_map.cell_idx_to_face_idx(cell, FaceID(face))
                               * points_per_face + quad_idx;
        const Vec<Float, 3> face_pos = face_points_dev.get_item(face_idx);
        const Float face_weight = face_weights_dev.get_item(face_idx);
        for (int32 source = 0; source < num_sources; ++source)
        {
          const Vec<Float, 3> omega = (face_pos - ray_sources_dev.get_item(source));
          const Vec<Float, 3> omega_hat = omega.normalized();
          const Float rcp_mag2 = rcp_safe(omega.magnitude2());
          // Really should use volume-average (over source cell) of rcp_mag2.

          const Float face_cosine = dot(omega_hat, face_normal);
          const Float slanted_area_weight = face_area * face_cosine * face_weight;

          if (omega.magnitude2() == 0.0f)
            continue;

          const sph_t * sph_eval = sph.eval_all(omega_hat);

          const int32 source_idx = source_cells_dev.get_item(source);

          for (int32 component = 0; component < ncomp; ++component)
          {
            // Factor to get cell-averaged flux from total current.
            const Float current_to_cell_flux = 1.0 /
              (cell_volume * sigmat_dev.get_item(cell, component));

            // Evaluate emission in the direction of omega_hat.
            Float dEmission_dV = 0.0f;
            for (int32 nm = 0; nm < num_moments; ++nm)
            {
              dEmission_dV += sph_eval[nm]
                             * sqrt_four_pi   // L_plus_times
                             * emission_dev.get_item(num_moments * source_idx + nm,
                                                     component);
            }

            const Float source_dL_dOmega
              = dEmission_dV * cell_volume * rcp_mag2;

            const Float transmitted = path_lengths_dev.get_item(
                num_sources * face_idx + source, component);

            const Float trans_source = transmitted * source_dL_dOmega;

            const Float flux_part = - trans_source * slanted_area_weight * current_to_cell_flux;

            for (int32 nm = 0; nm < num_moments; ++nm)
            {
              const sph_t spherical_harmonic = sph_eval[nm];

              // Integrate
              //   \int_{4\pi} (4\pi)^{-1/2} \Ynm(\Omega) \psi(\Omega) d\Omega
              // but changing coordinates into a volume integral
              // and approximating with a constant \Omega
              // and constant r over whole volume.
              const Float contribution = spherical_harmonic * flux_part * rcp_sqrt_four_pi;

              cell_moments_dev.get_item(num_moments * cell + nm, component)
                  += contribution;
            }//moments
          }//components
        }//sources
      }//quad points on face
    }//faces
  });//destination cells

  return cell_moments;
}



int32 moment_to_legendre(int32 nm)
{
  return int32(sqrt(nm));
}


void scatter(Array<Float> destination_moments,
             int32 _num_moments,
             Float _sigs,   //TODO m_sigs should be a matrix-valued field
             LowOrderField *first_scatter_out)
{
  const int32 zones_times_moments = destination_moments.size();
  const int32 num_moments = _num_moments;
  const int32 ngroups = destination_moments.ncomp();
  const Float sigs = _sigs;

  if (first_scatter_out->values().size() != destination_moments.size())
  {
    std::cerr << "Depositing size " << destination_moments.size()
              << " but output has size " << first_scatter_out->values().size()
              << "\n";
  }
  if (first_scatter_out->values().ncomp() != destination_moments.ncomp())
  {
    std::cerr << "Depositing ncomp " << destination_moments.ncomp()
              << " but output has ncomp " << first_scatter_out->values().ncomp()
              << "\n";
  }

  ConstDeviceArray<Float> in_deva(destination_moments);
  NonConstDeviceArray<Float> out_deva(first_scatter_out->values());

  // Based on Kripke/Kernel/Scattering.cpp

  RAJA::forall<for_policy> (RAJA::RangeSegment(0, zones_times_moments),
      [=] DRAY_LAMBDA (int32 zone_moment_idx)
  {
    const int32 zone = zone_moment_idx / num_moments;
    const int32 nm = zone_moment_idx % num_moments;

    const int32 n = moment_to_legendre(nm);

    for (int32 group_dest = 0; group_dest < ngroups; ++group_dest)
    {
      Float sum = 0.0f;
      for (int32 group_src = 0; group_src < ngroups; ++group_src)
      {
          // variable_sigs should depend on zone, group_src, group_dest, and n
        const Float variable_sigs = (n == 0 ? (group_src == group_dest ? sigs : 0.0f) : 0.0f);
        sum += variable_sigs * in_deva.get_item(num_moments * zone + nm, group_src);
      }

      out_deva.get_item(num_moments * zone + nm, group_dest) = sum;
    }
  });
}


void copy_moments(Array<Float> destination_moments,
             int32 _num_moments,
             LowOrderField *uncollided_flux_out)
{
  const int32 zones_times_moments = destination_moments.size();
  const int32 num_moments = _num_moments;
  const int32 ngroups = destination_moments.ncomp();

  if (uncollided_flux_out->values().size() != destination_moments.size())
  {
    std::cerr << "Depositing size " << destination_moments.size()
              << " but output has size " << uncollided_flux_out->values().size()
              << "\n";
  }
  if (uncollided_flux_out->values().ncomp() != destination_moments.ncomp())
  {
    std::cerr << "Depositing ncomp " << destination_moments.ncomp()
              << " but output has ncomp " << uncollided_flux_out->values().ncomp()
              << "\n";
  }

  ConstDeviceArray<Float> in_deva(destination_moments);
  NonConstDeviceArray<Float> out_deva(uncollided_flux_out->values());

  // Based on Kripke/Kernel/Scattering.cpp

  RAJA::forall<for_policy> (RAJA::RangeSegment(0, zones_times_moments),
      [=] DRAY_LAMBDA (int32 zone_moment_idx)
  {
    for (int32 group = 0; group < ngroups; ++group)
      out_deva.get_item(zone_moment_idx, group) =
          in_deva.get_item(zone_moment_idx, group);
  });
}


Float popcount(Array<Float> destination_moments, int32 _num_moments, Float _cell_volume)
{
  /// std::cout << "dray volume == " << _cell_volume << "\n";

  const int32 zones_times_moments = destination_moments.size();
  const int32 num_moments = _num_moments;
  const int32 num_zones = zones_times_moments / num_moments;
  const int32 ngroups = destination_moments.ncomp();

  const Float cell_volume = _cell_volume;

  ConstDeviceArray<Float> in_deva(destination_moments);

  RAJA::ReduceSum<reduce_policy, Float> pop(0);
  RAJA::forall<for_policy> (RAJA::RangeSegment(0, num_zones),
      [=] DRAY_LAMBDA (int32 zone)
  {
    const int32 zone_moment_idx = zone * num_moments + 0;

    Float group_sum = 0.0;
    for (int32 group = 0; group < ngroups; ++group)
      group_sum += in_deva.get_item(zone_moment_idx, group);
    pop += group_sum;
  });

  /// return pop.get() * sqrt(4*pi()) * cell_volume;
  return pop.get() * 4*pi() * cell_volume;
    // Not sure why but only matches Kripke population if use 4pi here.
}






};//namespace dray
