// Copyright 2019 Lawrence Livermore National Security, LLC and other
// Devil Ray Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (BSD-3-Clause)

#ifndef DRAY_DEVICE_TEXTURE2D_HPP
#define DRAY_DEVICE_TEXTURE2D_HPP

#include <dray/exports.hpp>
#include <dray/types.hpp>
#include <dray/vec.hpp>

namespace dray
{

// TODO: we could easily template this on
//       on T = floa32, or Vec<float32,4> i.e. color
struct DeviceTexture2D
{
  const float32 *m_texture;
  const int32 m_width;
  const int32 m_height;
  DeviceScalarTexture2D() = delete;
  DeviceScalarTexture2D(Array<float32> &texture, const int width, const int32 height)
    : m_texture(texture.get_device_ptr_const()),
      m_width(width),
      m_height(height)
  {}

  DRAY_EXEC
  float32 blerp(const float32 s,
                const float32 t) const
  {
    // we now need to blerp
    Vec<int32,2> st_min, st_max;
    st_min[0] = clamp(int32(s), 0, m_width - 1);
    st_min[1] = clamp(int32(t), 0, m_height - 1);
    st_max[0] = clamp(st_min[0]+1, 0, m_width - 1);
    st_max[1] = clamp(st_min[1]+1, 0, m_height - 1);

    Vec<float32,4> vals;
    vals[0] = m_texture[st_min[1] * m_width + st_min[0]];
    vals[1] = m_texture[st_min[1] * m_width + st_max[0]];
    vals[2] = m_texture[st_max[1] * m_width + st_min[0]];
    vals[3] = m_texture[st_max[1] * m_width + st_max[0]];

    float32 dx = s - float32(st_min[0]);
    float32 dy = t - float32(st_min[1]);

    float32 x0 = lerp(vals[0], vals[1], dx);
    float32 x1 = lerp(vals[2], vals[3], dx);
    // this the signed distance to the glyph
    return lerp(x0, x1, dy);
  }

};

} // namespace dray
#endif