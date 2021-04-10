// Copyright 2019 Lawrence Livermore National Security, LLC and other
// Devil Ray Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (BSD-3-Clause)

#ifndef DRAY_DISNEY_SAMPLING_HPP
#define DRAY_DISNEY_SAMPLING_HPP

#include <dray/rendering/sampling.hpp>
#include <dray/rendering/path_data.hpp>
#include <dray/rendering/debug_printing.hpp>
#include <dray/random.hpp>
#include <dray/matrix.hpp>

namespace dray
{


// sampling convientions
// wo = tangent space direction of output direction.
//      this is the view direction or -ray.m_dir
// wi = tangent space direction of input direction
//      this is ths sample direction (incoming light dir)
// wh = tangent space direction of the half vector
//      (wo + wi).normalize


// trig functions for normalized vectors in tangent space
// where the normal is (0,0,1)
DRAY_EXEC float32 tcos_theta(const Vec<float32,3> &dir)
{
  return dir[2];
}

DRAY_EXEC float32 tcos2_theta(const Vec<float32,3> &dir)
{
  return dir[2] * dir[2];
}

DRAY_EXEC float32 tsin2_theta(const Vec<float32,3> &dir)
{
  return max(0.f, 1.f - tcos2_theta(dir));
}

DRAY_EXEC float32 tsin_theta(const Vec<float32,3> &dir)
{
  return sqrt(tsin2_theta(dir));
}

DRAY_EXEC float32 ttan_theta(const Vec<float32,3> &dir)
{
  return tsin_theta(dir) / tcos_theta(dir);
}

DRAY_EXEC float32 ttan2_theta(const Vec<float32,3> &dir)
{
  return tsin2_theta(dir) / tcos2_theta(dir);
}

DRAY_EXEC float32 tcos_phi(const Vec<float32,3> &dir)
{
  float32 sin_theta = tsin_theta(dir);
  return sin_theta == 0.f ? 1.f : clamp(dir[0] / sin_theta, -1.f, 1.f);
}

DRAY_EXEC float32 tsin_phi(const Vec<float32,3> &dir)
{
  float32 sin_theta = tsin_theta(dir);
  return sin_theta == 0.f ? 0.f : clamp(dir[1] / sin_theta, -1.f, 1.f);
}

DRAY_EXEC float32 tsin2_phi(const Vec<float32,3> &dir)
{
  return tsin_phi(dir) * tsin_phi(dir);
}

DRAY_EXEC float32 tcos2_phi(const Vec<float32,3> &dir)
{
  return tcos_phi(dir) * tcos_phi(dir);
}

DRAY_EXEC
float32 scale_roughness(const float32 roughness, const float32 ior)
{
    return roughness * clamp(0.65f * ior - 0.35f, 0.f, 1.f);
}

DRAY_EXEC
bool same_hemi(const Vec<float32,3> &w1, const Vec<float32,3> &w2)
{
  return w1[2] * w2[2] > 0.f;
}

DRAY_EXEC
Vec<float32,3> refract(const Vec<float32,3> &wi,
                       const Vec<float32,3> &n,
                       float32 eta,
                       bool &valid,
                       bool debug = false)
{
     Vec<float32,3> wt;
    // Compute $\cos \theta_\roman{t}$ using Snell's law
    float32 cos_theta_i = dot(n, wi);
    float32 sin2_theta_i = max(0.f, 1.f - cos_theta_i * cos_theta_i);
    float32 sin2_theta_t = eta * eta * sin2_theta_i;

    // Handle total internal reflection for transmission
    if (sin2_theta_t >= 1.f)
    {
      valid = false;
      if(debug) kernel_printf("[refract] sin2_theta_t %f\n",sin2_theta_t);
      if(debug) kernel_printf("[refract] costheta_i %f\n",cos_theta_i);
    }
    float32 cos_theta_t = sqrt(1 - sin2_theta_t);
    wt = eta * -wi + (eta * cos_theta_i - cos_theta_t) * n;
    return wt;
}

DRAY_EXEC
void calc_anisotropic(float32 roughness, float32 anisotropic, float32 &ax, float32 &ay)
{
  float32 aspect = sqrtf(1.0f - 0.9f * anisotropic);
  ax = max(0.001f, sqrt(roughness) / aspect);
  ay = max(0.001f, sqrt(roughness) * aspect);
}

DRAY_EXEC
float32 smithg_ggx_aniso(float32 n_dot_v,
                         float32 v_dot_x,
                         float32 v_dot_y,
                         float32 ax,
                         float32 ay)
{
  float32 a = v_dot_x * ax;
  float32 b = v_dot_y * ay;
  float32 c = n_dot_v;
  return 1.0 / (n_dot_v + sqrt(a*a + b*b + c*c));
}

DRAY_EXEC
float32 lambda(const Vec<float32,3> &w,
               const float32 ax,
               const float32 ay)
{

  if(tcos_theta(w) == 0.f)
  {
    return 0.f;
  }

  float32 abs_tan_theta = abs(ttan_theta(w));
  float32 alpha = sqrt(tcos2_phi(w) * ax * ax + tsin2_phi(w) * ay * ay);
  float32 atan_theta = alpha * abs_tan_theta * alpha * abs_tan_theta;
  return 0.5f * (-1.f + sqrt(1.f + atan_theta));
}

DRAY_EXEC
float32 ggx_g(const Vec<float32,3> &wo,
              const Vec<float32,3> &wi,
              const float32 ax,
              const float32 ay)
{
  return 1.f / (1.f + lambda(wi, ax, ay) + lambda(wo, ax, ay));
}

DRAY_EXEC
float32 ggx_g1(const Vec<float32,3> &w,
               const float32 ax,
               const float32 ay)
{
  return 1.f / (1.f + lambda(w, ax, ay));
}

DRAY_EXEC
float32 ggx_d(const Vec<float32,3> &wh, const float32 ax, const float32 ay, bool debug = false)
{
  if(tcos_theta(wh) == 0.f)
  {
    return 0.f;
  }

  float32 tan2_theta = ttan2_theta(wh);

  float32 cos4_theta = tcos2_theta(wh) * tcos2_theta(wh);
  float32 e = (tcos2_phi(wh) / (ax * ax) + tsin2_phi(wh) / (ay * ay) ) * tan2_theta;
  if(debug)
  {
    kernel_printf("[ggx_d] wh %f %f %f\n",wh[0], wh[1], wh[2]);
    kernel_printf("[ggx_d] cos4 %f\n",cos4_theta);
    kernel_printf("[ggx_d] e %f\n",e);
    kernel_printf("[ggx_d] tan2 %f\n",tan2_theta);
  }
  return 1.f / (pi() * ax * ay * cos4_theta * (1.f + e) * (1.f + e));
}

DRAY_EXEC
float32 separable_ggx_aniso(const Vec<float32,3> &w,
                            float32 ax,
                            float32 ay)
{
  // just do this calculation in tangent space
  float32 cos_theta = tcos_theta(w);
  if(cos_theta == 0.f)
  {
    return 0.f;
  }

  float32 abs_tan_theta = abs(ttan_theta(w));
  float32 a = sqrt(tcos2_phi(w) * ax * ax + tsin2_phi(w) * ay * ay);
  float32 b = a * a * abs_tan_theta * abs_tan_theta;
  float32 lambda = 0.5f * (-1.f + sqrt(1.f * b));

  return 1.f / (1.f + lambda);
}

DRAY_EXEC
float32 gtr1(float32 n_dot_h, float32 a)
{
  if (a >= 1.0)
      return (1.0 / pi());
  float32 a2 = a * a;
  float32 t = 1.0 + (a2 - 1.0) * n_dot_h * n_dot_h;
  return (a2 - 1.0) / (pi() * log(a2) * t);
}

DRAY_EXEC
float32 fresnel(float32 theta, float32 n1, float32 n2)
{
  float32 r0 = (n1 - n2) / (n1 + n2);
  r0 = r0 * r0;
  return r0 + (1.f - r0) * schlick_fresnel(theta);
}

//http://www.jcgt.org/published/0007/04/01/paper.pdf
// GgxAnisotropicD
DRAY_EXEC
float32 gtr2_aniso(const Vec<float32,3> &wh,
                   float32 ax,
                   float32 ay, bool debug = false)
{
  float32 h_dot_x = wh[0];
  float32 h_dot_y = wh[1];
  float32 n_dot_h = tcos_theta(wh);

  float32 a = h_dot_x / ax;
  float32 b = h_dot_y / ay;
  float32 c = a * a + b * b + n_dot_h * n_dot_h;
  if(debug)
  {
    kernel_printf("[gtr2] a b c %f %f %f \n",a,b,c);
  }

  return 1.0f / (pi() * ax * ay * c * c);
}

DRAY_EXEC
float32 dielectric(float32 cos_theta_i, float32 ni, float32 nt, bool debug = false)
{
    // Copied from PBRT. This function calculates the
    // full Fresnel term for a dielectric material.

    cos_theta_i = clamp(cos_theta_i, -1.0f, 1.0f);

    // Swap index of refraction if this is coming from inside the surface
    if(cos_theta_i < 0.0f)
    {
      float32 temp = ni;
      ni = nt;
      nt = temp;
      cos_theta_i = -cos_theta_i;
    }
    if(debug)
    {
      kernel_printf("etaI %f\n",ni);
      kernel_printf("etaT %f\n",nt);
    }

    float32 sin_theta_i = sqrtf(max(0.0f, 1.0f - cos_theta_i * cos_theta_i));
    float32 sin_theta_t = ni / nt * sin_theta_i;

    // Check for total internal reflection
    if(sin_theta_t >= 1) {
        return 1;
    }

    float32 cos_theta_t = sqrtf(max(0.0f, 1.0f - sin_theta_t * sin_theta_t));

    float32 r_parallel = ((nt * cos_theta_i) - (ni * cos_theta_t)) / ((nt * cos_theta_i) + (ni * cos_theta_t));
    float32 r_perpendicuar = ((ni * cos_theta_i) - (nt * cos_theta_t)) / ((ni * cos_theta_i) + (nt * cos_theta_t));
    return (r_parallel * r_parallel + r_perpendicuar * r_perpendicuar) / 2;
}

DRAY_EXEC
Vec<float32,3> sample_ggx(float32 roughness, Vec<float32,2> rand)
{
  float32 a = max(0.001f, roughness);

  float32 phi = rand[0] * 2.f * pi();

  float32 cos_theta = sqrt((1.f - rand[1]) / (1.f + (a * a - 1.f) * rand[1]));
  float32 sin_theta = clamp(sqrt(1.f - (cos_theta * cos_theta)), 0.f, 1.f);
  float32 sin_phi = sin(phi);
  float32 cos_phi = cos(phi);

  Vec<float32,3> dir;
  dir[0] = sin_theta * cos_phi;
  dir[1] = sin_theta * sin_phi;
  dir[2] = cos_theta;

  dir.normalize();
  return dir;
}

DRAY_EXEC
Vec<float32,3> sample_vndf_ggx(const Vec<float32,3> &wo,
                               float32 ax,
                               float32 ay,
                               Vec<float32,2> rand)
{

  // this code samples based on the assumption that
  // the view direction is in tangent space
  // http://www.jcgt.org/published/0007/04/01/paper.pdf

  // stretched view vector
  Vec<float32,3> s_view;
  s_view[0] = wo[0] * ax;
  s_view[1] = wo[1] * ay;
  s_view[2] = wo[2];
  s_view.normalize();


  Vec<float32,3> wcX, wcY;
  create_basis(s_view,wcX,wcY);

  float32 r = sqrt(rand[0]);
  float32 phi = 2.f * pi() * rand[1];
  float32 t1 = r * cos(phi);
  float32 t2 = r * sin(phi);
  float32 s = 0.5f * (1.f + s_view[2]);
  t2 = (1.f - s) * sqrt(1.f - t1 * t1) + s * t2;
  float32 t3 = sqrt(max(0.f, 1.f - t1 * t1 - t2 * t2));

  //// dir is the half vector
  Vec<float32,3> dir;

  dir = t1 * wcX + t2 * wcY + t3 * s_view;
  dir[0] *= ax;
  dir[1] *= ay;
  dir[2] = max(0.f, dir[2]);
  dir.normalize();

  return dir;
}

DRAY_EXEC
float32 pdf_vndf_ggx(const Vec<float32,3> &wo,
                     const Vec<float32,3> &wh,
                     const float32 ax,
                     const float32 ay,
                     bool debug = false)
{

  float32 g = ggx_g1(wo, ax, ay);

  float32 d = ggx_d(wh, ax, ay);

  if(debug)
  {
    kernel_printf("[ VNDF pdf ] g %f\n",g);
    kernel_printf("[ VNDF pdf ] d %f\n",d);
  }

  return g * abs(dot(wo,wh)) * d / abs(tcos_theta(wo));
}

DRAY_EXEC
Vec<float32,3> sample_microfacet_transmission(const Vec<float32,3> &wo,
                                              const float32 &eta,
                                              const float32 &ax,
                                              const float32 &ay,
                                              Vec<uint,2> &rand_state,
                                              bool &valid,
                                              bool debug = false)
{
  valid = true;
  if(wo[2] == 0.f)
  {
    valid = false;
  }

  Vec<float32,2> rand;
  rand[0] = randomf(rand_state);
  rand[1] = randomf(rand_state);
  Vec<float32,3> wh = sample_vndf_ggx(wo, ax, ay, rand);
  if(debug)
  {
    kernel_printf("[Sample MT] wh %f %f %f\n",wh[0], wh[1], wh[2]);
  }

  if(dot(wo, wh) < 0)
  {
    valid = false;
  }

  // normally we would calculate the eta based on the
  // side of of the wo, but we are currently modeling that
  // only thin (entrance and exit in the same interaction)

  Vec<float32,3> wi = refract(wo, wh, eta, valid);
  return wi;
}

DRAY_EXEC
float32 pdf_microfacet_transmission(const Vec<float32,3> &wo,
                                    const Vec<float32,3> &wi,
                                    float32 eta,
                                    const float32 &ax,
                                    const float32 &ay,
                                    bool debug = false)
{
  float32 pdf = 1.f;

  if(same_hemi(wo,wi))
  {
    return 0.f;
  }

  if(tcos_theta(wo) > 0.f)
  {
    eta = 1.f / eta;
  }

  Vec<float32,3> wh = wo + eta * wi;
  wh.normalize();

  if(dot(wo,wh) * dot(wi,wh) > 0.f)
  {
    pdf = 0.f;
  }

  float32 a = dot(wo,wh) + eta * dot(wi,wh);

  float32 dwh_dwi = abs((eta * eta * dot(wi,wh)) / (a * a));

  float32 distribution_pdf = pdf_vndf_ggx(wo, wh, ax, ay, debug);
  if(debug)
  {
    kernel_printf("[MT PDF] a %f\n",a);
    kernel_printf("[MT PDF] dist %f\n",distribution_pdf);
    kernel_printf("[MT PDF] dwf_dwi %f\n ",dwh_dwi);
    kernel_printf("[MT PDF] wh %f %f %f\n",wh[0], wh[1], wh[2]);
  }
  pdf *= distribution_pdf * dwh_dwi;
  return pdf;
}

DRAY_EXEC
Vec<float32,3> eval_microfacet_transmission(const Vec<float32,3> &wo,
                                            const Vec<float32,3> &wi,
                                            const float32 ior,
                                            const float32 &ax,
                                            const float32 &ay,
                                            bool debug = false)
{
  Vec<float32,3> color = {{1.f, 1.f, 1.f}};

  // same hemi
  if(tcos_theta(wo) > 0.f && tcos_theta(wi) > 0.f)
  {
    color = {{0.f, 0.f, 0.f}};
  }

  float32 n_dot_v = tcos_theta(wo);
  float32 n_dot_l = tcos_theta(wi);
  if(n_dot_v == 0.f || n_dot_l == 0.f)
  {
    color = {{0.f, 0.f, 0.f}};
  }
  // flip eta if we were not just modeling thin

  // always air
  float32 eta = ior / 1.f;

  if(n_dot_v > 0.f)
  {
    eta = 1.f / eta;
  }

  Vec<float32,3> wh = wo + wi * eta;
  wh.normalize();
  // make sure we are in the same hemi as the normal
  if(wh[2] < 0)
  {
    wh = -wh;
  }

  if(dot(wo,wh) * dot(wi,wh) > 0)
  {
    color = {{0.f, 0.f, 0.f}};
  }

  float32 f = dielectric(dot(wo,wh), ior, 1.f);

  float32 a = dot(wo,wh) + eta * dot(wi, wh);

  float32 d = ggx_d(wh, ax, ay);
  float32 g = ggx_g(wo,wi, ax, ay);
  if(debug)
  {
    kernel_printf("[Eval MT] wo %f %f %f\n",wo[0], wo[1], wo[2]);
    kernel_printf("[Eval MT] wi %f %f %f\n",wi[0], wi[1], wi[2]);
    kernel_printf("[Eval MT] eta %f\n",eta);
    kernel_printf("[Eval MT] g %f\n",g);
    kernel_printf("[Eval MT] d %f\n",d);
    kernel_printf("[Eval MT] frensel %f\n",f);
    kernel_printf("[Eval MT] wh %f %f %f\n",wh[0],wh[1],wh[2]);
  }

  color = color * (1.f - f) * abs(d * g *
          eta *eta * abs(dot(wi,wh)) * abs(dot(wo,wh)) /
          (n_dot_v * n_dot_l * a * a));

  return color;
}


DRAY_EXEC
Vec<float32,3> sample_microfacet_reflection(const Vec<float32,3> &wo,
                                            const float32 &ax,
                                            const float32 &ay,
                                            Vec<uint,2> &rand_state,
                                            bool &valid,
                                            bool debug = false)
{
  valid = true;
  // TODO: we can probaly elimate the valid
  // checks since the pdf will be 0. Check to see if
  // the pdf is zero and set the color to 0;
  if(wo[2] == 0.f)
  {
    valid = false;
  }

  Vec<float32,2> rand;
  rand[0] = randomf(rand_state);
  rand[1] = randomf(rand_state);
  //rand = {{0.994541, 0.438798}};
  Vec<float32,3> wh = sample_vndf_ggx(wo, ax, ay, rand);
  if(debug)
  {
    kernel_printf("[Sample MR] wh %f %f %f\n",wh[0], wh[1], wh[2]);
    kernel_printf("[Sample MR] w0 %f %f %f\n",wo[0], wo[1], wo[2]);
    kernel_printf("[Sample MR] ax ay %f %f\n",ax,ay);
    kernel_printf("[Sample MR] rand %f %f\n",rand[0], rand[1]);
  }
  if(dot(wo,wh) < 0.)
  {
    if(debug) kernel_printf("Bad wh sample\n");
    valid = false;
  }

  Vec<float32,3> wi = reflect(wo,wh);
  if(!same_hemi(wo,wi))
  {
    if(debug) kernel_printf("Bad reflect wi %f %f %f\n",wi[0], wi[1], wi[2]);
    valid = false;
  }
  return wi;
}

DRAY_EXEC
Vec<float32,3> eval_microfacet_reflection(const Vec<float32,3> &wo,
                                          const Vec<float32,3> &wi,
                                          const float32 ior,
                                          const float32 &ax,
                                          const float32 &ay,
                                          bool debug = false)
{
  Vec<float32,3> color = {{1.f, 1.f, 1.f}};

  float32 abs_n_dot_v = abs(tcos_theta(wo));
  float32 abs_n_dot_l = abs(tcos_theta(wi));
  Vec<float32,3> wh = wi + wo;
  bool return_zero = false;

  if(abs_n_dot_v == 0.f || abs_n_dot_v == 0.f)
  {
    return_zero = true;
  }
  if(wh[0] == 0.f && wh[1] == 0.f && wh[2] == 0.f)
  {
    return_zero = true;
  }
  wh.normalize();

  float32 d = ggx_d(wh, ax, ay,debug);
  float32 g = ggx_g(wo,wi, ax, ay);

  // for fresnel make sure that wh is in the same hemi as the normal
  // I don't this should happen but better be safe
  if(tcos_theta(wh) < 0)
  {
    wh = -wh;
  }

  float32 f = dielectric(dot(wo,wh), ior, 1.f,  debug);
  if(debug)
  {
    kernel_printf("[Color eval] reflection f %f\n",f);
    kernel_printf("[Color eval] reflection d %f\n",d);
    kernel_printf("[Color eval] reflection g %f\n",g);
    kernel_printf("[Color eval] reflection denom %f\n",4.f * abs_n_dot_v * abs_n_dot_l);
  }

  if(return_zero)
  {
    return {{0.f, 0.f, 0.f}};
  }

  color = f * d * g / (4.f * abs_n_dot_v * abs_n_dot_l);

  return color;
}

DRAY_EXEC
float32 pdf_microfacet_reflection(const Vec<float32,3> &wo,
                                  const Vec<float32,3> &wi,
                                  const float32 &ax,
                                  const float32 &ay,
                                  bool debug = false)
{
  float32 pdf = 1.f;

  if(!same_hemi(wo,wi))
  {
    return  0.f;
  }


  Vec<float32,3> wh = wo + wi;
  wh.normalize();

  float32 distribution_pdf = pdf_vndf_ggx(wo, wh, ax, ay, debug);

  pdf *= distribution_pdf / (4.0 * dot(wo,wh));
  if(debug)
  {
    kernel_printf("[MR PDF] dist %f\n",distribution_pdf);
    kernel_printf("[MR PDF] wh %f %f %f\n",wh[0], wh[1], wh[2]);
    kernel_printf("[MR PDF] pdf %f\n",pdf);
  }
  return pdf;
}

DRAY_EXEC
Vec<float32,3> sample_spec_trans(const Vec<float32,3> &wo,
                                 const Material &mat,
                                 bool &specular,
                                 Vec<uint,2> &rand_state,
                                 bool &valid,
                                 bool debug = false)
{
  valid = true;
  Vec<float32,3> wi;
  float32 ax,ay;
  calc_anisotropic(mat.m_roughness, mat.m_anisotropic, ax, ay);
  // always use air
  Vec<float32,2> rand;
  rand[0] = randomf(rand_state);
  rand[1] = randomf(rand_state);

  float32 n_mat = mat.m_ior;
  float32 thin_roughness = max(0.001f, mat.m_roughness * clamp(0.65f * mat.m_ior - 0.35f, 0.f, 1.f));
  Vec<float32,3> wh = sample_vndf_ggx(wo, ax * thin_roughness, ay * thin_roughness, rand);


  float32 theta = tcos_theta(wo);
  float32 cos2theta = 1.f - mat.m_ior * mat.m_ior * (1.f - theta * theta);

  float32 v_dot_h = dot(wo,wh);
  if(wh[2] < 0.f)
  {
    v_dot_h = -v_dot_h;
  }

  //float32 f = dielectric(v_dot_h, 1.0f, mat.m_ior, debug);
  float32 f = dielectric(v_dot_h, 1.0f, mat.m_ior, debug);

  const float32 reflect_roll = randomf(rand_state);

  if(debug)
  {
    kernel_printf("[Sample] f %f\n",f);
    kernel_printf("[Sample] roll %f\n",reflect_roll);
    kernel_printf("[Sample] cos2 %f\n",cos2theta);
    kernel_printf("[Sample] v_dot_h %f\n",v_dot_h);
    kernel_printf("[Sample] wo %f %f %f\n",wo[0],wo[1],wo[2]);
    kernel_printf("[Sample] wh %f %f %f\n",wh[0],wh[1],wh[2]);
    kernel_printf("[Sample] transmission\n");
  }


  //if(cos2theta < 0.f || reflect_roll < f)
  if(reflect_roll < f)
  {
    wi = reflect(wo,wh);
    if(!same_hemi(wo,wi))
    {
      valid = false;
    }
    if(debug)
    {
      kernel_printf("[Sample] refect\n");
      if(!valid) kernel_printf("[Sample] invalid\n");
    }
  }
  else
  {
    float32 eta = 1.f / mat.m_ior;
    wi = refract(wo, wh, eta, valid, debug);
    if(dot(wh,wo)< 0.f)
    {
      valid = false;
    }
    //wi[2] = -wi[o];
    specular = true;
    if(debug)
    {
      if(!valid) kernel_printf("[Sample] invalid\n");
      kernel_printf("[Sample] refract\n");
      kernel_printf("[Sample] dot v_dot_h %f\n",dot(wh,wo));
      kernel_printf("[Sample] dot l_dot_h %f\n",dot(wi,wo));
      kernel_printf("[Sample] wi %f %f %f\n",wi[0], wi[1], wi[2]);
    }
  }

  wi.normalize();
  return wi;
}

DRAY_EXEC
float32 disney_pdf(const Vec<float32,3> &wo,
                   const Vec<float32,3> &wi,
                   const Material &mat,
                   bool debug = false)
{
  Vec<float32,3> wh = wo + wi;
  wh.normalize();

  float32 ax,ay;
  calc_anisotropic(mat.m_roughness, mat.m_anisotropic, ax, ay);
  float32 scale = max(0.001f,mat.m_roughness * clamp(0.65f * mat.m_ior - 0.35f, 0.f, 1.f));

  float32 n_dot_h = tcos_theta(wh);

  if(debug)
  {
    kernel_printf("[PDF] n_dot_l %f\n",tcos_theta(wi));
  }

  if(!same_hemi(wo,wi))
  {
    float32 trans_pdf = pdf_microfacet_transmission(wo,wi,mat.m_ior, ax * scale, ay * scale, debug);

    float32 eta = mat.m_ior;
    if(tcos_theta(wo) > 0.f)
    {
      eta = 1.f / eta;
    }

    Vec<float32,3> wht = wo + eta * wi;
    wht.normalize();
    float32 f = dielectric(dot(wo,wht), mat.m_ior, 1.f);

    // i feel like we have to weight by the chance of sampling this
    trans_pdf *= mat.m_spec_trans * (1.f - f);

    if(debug)
    {
      kernel_printf("[PDF] trans %f\n",trans_pdf);
    }
    return trans_pdf;
  }

  float32 specular_alpha = max(0.001f, mat.m_roughness);

  float32 diff_prob = 1.f - mat.m_metallic;
  float32 spec_prob = mat.m_metallic;

  // visible normal importance sampling pdf
  float32 vndf_pdf = pdf_vndf_ggx(wo, wi, ax, ay);

  // clearcloat pdf
  float32 clearcoat_alpha = mix(0.1f,0.001f, mat.m_clearcoat_gloss);
  float32 clearcoat_pdf = gtr1(n_dot_h, clearcoat_alpha) * n_dot_h / (4.f * abs(dot(wh,wi)));
  float32 mix_ratio = 1.f / (1.f + mat.m_clearcoat);
  float32 spec_r_pdf = pdf_microfacet_reflection(wo,wi,ax,ay,debug);
  float32 pdf_spec = mix(clearcoat_pdf, spec_r_pdf, mix_ratio);

  // diffuse pdf
  float32 pdf_diff = tcos_theta(wi) / pi();

  // total brdf pdf
  float32 brdf_pdf = diff_prob * pdf_diff + spec_prob * pdf_spec;

  // bsdf reflection
  float32 bsdf_pdf = pdf_microfacet_reflection(wo, wi, ax*scale, ay*scale, debug);

  float32 pdf = mix(brdf_pdf,bsdf_pdf, mat.m_spec_trans);

  if(debug)
  {
    kernel_printf("[PDF pdf_spec] %f\n",pdf_spec);
    kernel_printf("[PDF pdf_diff] %f\n",pdf_diff);
    kernel_printf("[PDF pdf_brdf] %f\n",brdf_pdf);
    kernel_printf("[PDF pdf_bsdf] %f\n",bsdf_pdf);
    kernel_printf("[PDF pdf] %f\n",pdf);
  }
  return pdf;
}


DRAY_EXEC
Vec<float32,3> sample_disney(const Vec<float32,3> &wo,
                             const Material &mat,
                             RayFlags &flags,
                             Vec<uint,2> &rand_state,
                             bool debug = false)
{

  bool valid = true;
  if(debug)
  {
    kernel_printf("[Sample] mat rough %f\n",mat.m_roughness);
    kernel_printf("[Sample] mat spec %f\n",mat.m_specular);
    kernel_printf("[Sample] mat metallic %f\n",mat.m_metallic);
  }
  flags = RayFlags::EMPTY;

  Vec<float32,3> wi;
  float32 ax,ay;
  calc_anisotropic(mat.m_roughness, mat.m_anisotropic, ax, ay);

  float32 spec_trans_roll = randomf(rand_state);
  if(debug)
  {
    kernel_printf("[Sample] spec_trans roll %f\n",spec_trans_roll);
    kernel_printf("[Sample] spec_trans %f\n",mat.m_spec_trans);
  }

  if(mat.m_spec_trans > spec_trans_roll)
  {
    bool specular;
    wi = sample_spec_trans(wo, mat, specular, rand_state, valid, debug);
    if(debug && !valid)
    {
      kernel_printf("[Sample] trans invalid\n");
    }

    // i don't think diffues it techincally accurate, but want to put something
    // in there anyway
    flags = specular ?  RayFlags::SPECULAR : RayFlags::DIFFUSE;

  }
  else
  {
    float32 diff_prob = 1.f - mat.m_metallic;
    Vec<float32,2> rand;
    rand[0] = randomf(rand_state);
    rand[1] = randomf(rand_state);

    if(randomf(rand_state) < diff_prob)
    {
      wi = cosine_weighted_hemisphere(rand);
      flags = RayFlags::DIFFUSE;

      if(debug)
      {
        kernel_printf("[Sample] diffuse\n");
        kernel_printf("[Sample] n_dot_l %f\n",tcos_theta(wi));
      }
    }
    else
    {
      wi = sample_microfacet_reflection(wo, ax, ay, rand_state, valid, debug);
      flags = RayFlags::SPECULAR;
      if(debug)
      {
        kernel_printf("[Sample] specular\n");
        if(!valid) kernel_printf("[Sample] invalid\n");
      }
    }

  }

  if(!valid) flags = RayFlags::INVALID;

  wi.normalize();
  return wi;
}



DRAY_EXEC
Vec<float32,3> eval_disney(const Vec<float32,3> &base_color,
                           const Vec<float32,3> &wi,
                           const Vec<float32,3> &wo,
                           const Material &mat,
                           bool debug = false)
{
  Vec<float32,3> color = {{0.f, 0.f, 0.f}};
  Vec<float32,3> brdf = {{0.f, 0.f, 0.f}};
  Vec<float32,3> bsdf = {{0.f, 0.f, 0.f}};
  if(debug)
  {
    kernel_printf("[Color eval] base_color %f %f %f\n",base_color[0], base_color[1], base_color[2]);
  }

  Vec<float32,3> wh = wi + wo;
  wh.normalize();

  if(debug)
  {
    kernel_printf("[Color eval] wi %f %f %f\n",wi[0], wi[1], wi[2]);
    kernel_printf("[Color eval] wo %f %f %f\n",wo[0], wo[1], wo[2]);
    kernel_printf("[Color eval] wh %f %f %f\n",wh[0], wh[1], wh[2]);
  }

  float32 n_dot_l = tcos_theta(wi);
  float32 n_dot_v = tcos_theta(wo);
  float32 n_dot_h = tcos_theta(wh);
  float32 l_dot_h = dot(wi, wh);

  float32 ax,ay;
  calc_anisotropic(mat.m_roughness, mat.m_anisotropic, ax, ay);


  if(debug)
  {
    kernel_printf("[Color eval] n_dot_l %f\n",n_dot_l);
    kernel_printf("[Color eval] n_dot_v %f\n",n_dot_v);
    kernel_printf("[Color eval] l_dot_h %f\n",l_dot_h);
  }

  if((mat.m_spec_trans < 1.f) && (n_dot_l > 0.f) && (n_dot_v > 0.f))
  {
    float32 clum = 0.3f * base_color[0] +
                   0.6f * base_color[1] +
                   0.1f * base_color[2];

    Vec<float32,3> ctint = {{1.f, 1.f, 1.f}};
    if(clum > 0.0)
    {
      ctint = base_color / clum;
    }
    constexpr Vec<float32,3> cone = {{1.f, 1.f, 1.f}};

    Vec<float32,3> csheen = {{1.f, 1.f, 1.f}};
    csheen = mix(cone, ctint, mat.m_sheen_tint);
    Vec<float32,3> cspec = mix(mat.m_specular * 0.08f * mix(cone, ctint, mat.m_spec_tint),
                               base_color,
                               mat.m_metallic);


    // diffuse fresnel
    float32 fl = schlick_fresnel(n_dot_l);
    float32 fv = schlick_fresnel(n_dot_v);
    float32 fd90 = 0.5f + 2.0f * l_dot_h * l_dot_h * mat.m_roughness;
    float32 fd = mix(1.f, fd90, fl) * mix(1.f, fd90, fv);

    // subsurface
    float32 fss90 = l_dot_h * l_dot_h * mat.m_roughness;
    float32 fss = mix(1.f, fss90, fl) * mix(1.f, fss90, fv);
    float32 ss = 1.25f * (fss * (1.f/(n_dot_l + n_dot_v) - 0.5f) + 0.5f);

    // specular
    float32 ax,ay;
    calc_anisotropic(mat.m_roughness, mat.m_anisotropic, ax, ay);

    float32 ds = gtr2_aniso(wh, ax, ay);
    float32 fh = schlick_fresnel(l_dot_h);
    Vec<float32,3> fs = mix(cspec, cone, fh);
    float32 gl = separable_ggx_aniso(wi, ax, ay);
    float32 gv = separable_ggx_aniso(wo, ax, ay);
    float32 gs = gl * gv;

    if(debug)
    {
      kernel_printf("[Color eval] fs %f %f %f\n",fs[0], fs[1], fs[2]);
      kernel_printf("[Color eval] gs %f\n",gs);
      kernel_printf("[Color eval] ds %f\n",ds);
    }


    // sheen
    Vec<float32,3> fsheen =  fh * mat.m_sheen * csheen;

    // clear coat
    float32 dr = gtr1(n_dot_h, mix(0.1f, 0.001f, mat.m_clearcoat_gloss));
    float32 fr = mix(0.04f, 1.f, fh);
    float32 gr = smithg_ggx(n_dot_l, 0.25f) * smithg_ggx(n_dot_v,0.25f);

    float32 inv_pi = 1.f / pi();
    Vec<float32,3> diff = (inv_pi * mix(fd, ss, mat.m_subsurface) *  base_color +fsheen) *
                          (1.f - mat.m_metallic);

    //Vec<float32,3> spec =  gs * ds * fs;
    // TODO: is this right? and I need to get rid of gs,fs.ds.
    Vec<float32,3> spec =  eval_microfacet_reflection(wo,wi,mat.m_ior, ax, ay, debug);
    spec[0] *= cspec[0];
    spec[1] *= cspec[1];
    spec[2] *= cspec[2];

    float32 cc_fact = 0.25f * mat.m_clearcoat * gr * fr * dr;
    Vec<float32,3> clearcoat = {{cc_fact, cc_fact, cc_fact}};
    brdf = diff + spec + clearcoat;

    if(debug)
    {
      kernel_printf("[Color eval] cspec %f %f %f\n",cspec[0],cspec[1],cspec[1]);
      kernel_printf("[Color eval] spec %f %f %f\n",spec[0],spec[1],spec[2]);
      kernel_printf("[Color eval] diff %f %f %f\n",diff[0],diff[1], diff[2]);
      kernel_printf("[Color eval] clearcoat %f %f %f\n",clearcoat[0], clearcoat[1], clearcoat[2]);
    }
  }

  if(mat.m_spec_trans > 0.f)
  {
    float32 scale = max(0.001f,mat.m_roughness * clamp(0.65f * mat.m_ior - 0.35f, 0.f, 1.f));
    Vec<float32,3> trans = eval_microfacet_transmission(wo,wi,mat.m_ior, ax * scale, ay * scale);
    trans[0] *= sqrt(base_color[0]);
    trans[1] *= sqrt(base_color[1]);
    trans[2] *= sqrt(base_color[2]);

    Vec<float32,3> ref = eval_microfacet_reflection(wo,wi,mat.m_ior, ax * scale, ay * scale, debug);
    ref[0] *= base_color[0];
    ref[1] *= base_color[1];
    ref[2] *= base_color[2];
    bsdf = trans + ref;

    if(debug)
    {
      kernel_printf("[Color eval] refract %f %f %f\n",trans[0], trans[1], trans[2]);
      kernel_printf("[Color eval] reflect %f %f %f\n",ref[0], ref[1], ref[2]);
    }
  }

  color = mix(brdf,bsdf, mat.m_spec_trans);

  if(debug)
  {
    kernel_printf("[Color eval] brdf %f %f %f\n",brdf[0], brdf[1], brdf[2]);
    kernel_printf("[Color eval] bsdf %f %f %f\n",bsdf[0], bsdf[1], bsdf[2]);
    kernel_printf("[Color eval] color %f %f %f\n",color[0], color[1], color[2]);
  }

  return color;
}


} // namespace dray
#endif
