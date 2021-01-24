// Copyright 2019 Lawrence Livermore National Security, LLC and other
// Devil Ray Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (BSD-3-Clause)

#include "test_config.h"
#include "gtest/gtest.h"

#include "t_utils.hpp"
#include <dray/io/blueprint_reader.hpp>
#include <dray/io/blueprint_low_order.hpp>
#include <dray/rendering/sampling.hpp>
#include <dray/rendering/sphere_light.hpp>
#include <dray/rendering/disney_sampling.hpp>
#include <dray/random.hpp>


#include <dray/math.hpp>

#include <conduit.hpp>
#include <conduit_relay.hpp>
#include <conduit_blueprint.hpp>


#include <fstream>
#include <stdlib.h>

using namespace dray;

void write_vectors(std::vector<Vec<float32,3>> &dirs, std::string name);

TEST (dray_test_sampling, dray_sphere_light)
{
  Array<Vec<uint32,2>> rstate;
  rstate.resize(1);
  bool deterministic = true;
  seed_rng(rstate, deterministic);
  Vec<uint32,2> rand_state = rstate.get_value(0);

  int32 samples = 1;

  SphereLight light;
  light.m_pos = {{0.f, 0.f, 4.f}};;
  light.m_radius = 1.f;
  light.m_intensity[0] = 1.f;
  light.m_intensity[1] = 1.f;
  light.m_intensity[2] = 1.f;

  Vec<float32,3> hit_point = {{0.f, 0.f, 0.f}};

  std::vector<Vec<float32,3>> dirs;
  for(int i = 0; i < samples; ++i)
  {
    Vec<float32,2> rand;
    rand[0] = randomf(rand_state);
    rand[1] = randomf(rand_state);
    float32 light_pdf;
    Vec<float32,3> sample_point = light.sample(hit_point, rand, light_pdf, true);
    Vec<float32,3> light_normal = sample_point - light.m_pos;
    light_normal.normalize();
    Vec<float32,3> dir = sample_point - hit_point;
    dirs.push_back(dir);
  }
  write_vectors(dirs,"sphere");
}

TEST (dray_test_sampling, dray_ggx_vndf)
{
  Array<Vec<uint32,2>> rstate;
  rstate.resize(1);
  bool deterministic = true;
  seed_rng(rstate, deterministic);
  Vec<uint32,2> rand_state = rstate.get_value(0);

  int32 samples = 100;

  Vec<float32,3> normal = {{0.f, 0.f, 1.f}};
  //Vec<float32,3> view = {{-0.1f, -.1f, 0.9f}};
  Vec<float32,3> view = {{-0.1f, -.4f, 0.4f}};
  view.normalize();

  //normal = {{0.956166, -0, -0.292826}};
  //view = {{0.830572, 0.358185, -0.426444}};
  //view = {{0.830572, 0.358185, 0.0}};

  Vec<float32,3> wcX, wcY;
  create_basis(normal,wcX,wcY);
  Matrix<float32,3,3> to_world, to_tangent;
  to_world.set_col(0,wcX);
  to_world.set_col(1,wcY);
  to_world.set_col(2,normal);

  to_tangent = to_world.transpose();
  Vec<float32,3> wo = to_tangent * view;

  float32 roughness = 0.1;
  float32 anisotropic = 0.1;
  float32 ax,ay;
  std::cout<<"anis "<<ax<<" "<<ay<<"\n";
  calc_anisotropic(roughness, anisotropic, ax, ay);

  std::vector<Vec<float32,3>> dirs;
  for(int i = 0; i < samples; ++i)
  {
    Vec<float32,2> rand;
    rand[0] = randomf(rand_state);
    rand[1] = randomf(rand_state);
    Vec<float32,3> new_dir = sample_vndf_ggx(wo, ax, ay, rand);
    new_dir.normalize();
    new_dir = to_world * new_dir;
    dirs.push_back(new_dir);
  }
  dirs.push_back(normal * 2.f);
  dirs.push_back(view* 3.f);
  write_vectors(dirs,"vndf");
}

TEST (dray_test_sampling, dray_microfacet_trans)
{
  Array<Vec<uint32,2>> rstate;
  rstate.resize(1);
  bool deterministic = true;
  seed_rng(rstate, deterministic);
  Vec<uint32,2> rand_state = rstate.get_value(0);

  int32 samples = 1;

  Vec<float32,3> normal = {{0.f, 1.f, 0.f}};
  Vec<float32,3> view = {{0.f, .4f, 0.4f}};
  //Vec<float32,3> view = {{0.f, .8f, 0.1f}};

  //normal = {{0.956166, -0, -0.292826}};
  //view = {{0.830572, -0.358185, -0.426444}};
  view.normalize();
  std::cout<<"World wo "<<view<<"\n";


  Vec<float32,3> wcX, wcY;
  create_basis(normal,wcX,wcY);
  Matrix<float32,3,3> to_world, to_tangent;
  to_world.set_col(0,wcX);
  to_world.set_col(1,wcY);
  to_world.set_col(2,normal);

  to_tangent = to_world.transpose();

  std::cout<<"World wo "<<view<<"\n";
  Vec<float32,3> wo = to_tangent * view;
  std::cout<<"tangent wo "<<wo<<"\n";


  Material mat;
  mat.m_ior = 1.3;
  mat.m_spec_trans = 1.0f;
  mat.m_specular = 0.99f;
  mat.m_roughness = 0.1f;
  Vec<float32,3> base_color = {{1.f, 1.f, 1.f}};
  std::vector<Vec<float32,3>> dirs;

  float32 ax,ay;
  calc_anisotropic(mat.m_roughness, mat.m_anisotropic, ax, ay);
  float32 scale = scale_roughness(mat.m_roughness, mat.m_ior);
  ax *= scale;
  ay *= scale;
  std::cout<<"Ax "<<ax<<" Ay "<<ay<<"\n";

  float32 eta = mat.m_ior / 1.f;

  for(int i = 0; i < samples; ++i)
  {
    bool specular;
    bool valid;
    Vec<float32,3> wi = sample_microfacet_transmission(wo, eta, ax, ay, rand_state, valid, true);
    if(!valid)
    {
      std::cout<<"invalid sample\n";
    }

    //wi = {{ 0, 0.99227792, 0.12403474 }};
    std::cout<<"new dir "<<wi<<"\n";

    //wi = {{ 0.157658085, 0.00148834591, -0.98749274}};
    //wo = {{ -0.124034785, 0, 0.99227792}};
    Vec<float32,3> color = eval_microfacet_transmission(wo,wi,mat.m_ior, ax, ay);

    std::cout<<"color "<<color<<"\n";
    float32 pdf = pdf_microfacet_transmission(wo,wi,mat.m_ior, ax, ay);

    std::cout<<"weight "<<pdf<<"\n";
    std::cout<<"weighted color "<<color/pdf<<"\n";

    wi.normalize();
    wi = to_world * wi;
    std::cout<<"World wi "<<wi<<"\n";
    dirs.push_back(wi);
  }
  dirs.push_back(normal * 2.f);
  dirs.push_back(view* 3.f);
  write_vectors(dirs,"transmission");
}

TEST (dray_test_sampling, dray_microfacet_reflection)
{
  Array<Vec<uint32,2>> rstate;
  rstate.resize(1);
  bool deterministic = true;
  seed_rng(rstate, deterministic);
  Vec<uint32,2> rand_state = rstate.get_value(0);

  int32 samples = 10;

  Vec<float32,3> normal = {{0.f, 1.f, 0.f}};
  Vec<float32,3> view = {{0.f, .4f, 0.4f}};
  //Vec<float32,3> view = {{0.f, .8f, 0.1f}};

  normal = {{0.f,0.f,1.f}};
  view = {{0.183252, -0.201243, 0.962247}};
  view.normalize();
  std::cout<<"World wo "<<view<<"\n";


  Vec<float32,3> wcX, wcY;
  create_basis(normal,wcX,wcY);
  Matrix<float32,3,3> to_world, to_tangent;
  to_world.set_col(0,wcX);
  to_world.set_col(1,wcY);
  to_world.set_col(2,normal);

  to_tangent = to_world.transpose();

  std::cout<<"World wo "<<view<<"\n";
  Vec<float32,3> wo = to_tangent * view;
  std::cout<<"tangent wo "<<wo<<"\n";


  Material mat;
  mat.m_ior = 1.3;
  mat.m_spec_trans = 1.0f;
  mat.m_specular = 0.99f;
  mat.m_roughness = 0.01f;
  Vec<float32,3> base_color = {{1.f, 1.f, 1.f}};
  std::vector<Vec<float32,3>> dirs;

  float32 ax,ay;
  calc_anisotropic(mat.m_roughness, mat.m_anisotropic, ax, ay);
  bool thin = false;
  if(thin)
  {
    float32 scale = scale_roughness(mat.m_roughness, mat.m_ior);
    ax *= scale;
    ay *= scale;
  }
  std::cout<<"Ax "<<ax<<" Ay "<<ay<<"\n";

  float32 eta = mat.m_ior / 1.f;

  for(int i = 0; i < samples; ++i)
  {
    std::cout<<"\n\nSample "<<i<<"\n";
    bool specular;
    bool valid;
    Vec<float32,3> wi = sample_microfacet_reflection(wo, ax, ay, rand_state, valid, true);
    if(!valid)
    {
      std::cout<<"invalid sample\n";
    }

    //wi = {{ 0, 0.99227792, 0.12403474 }};
    std::cout<<"new dir "<<wi<<"\n";

    //wi = {{ 0.157658085, 0.00148834591, -0.98749274}};
    //wo = {{ -0.124034785, 0, 0.99227792}};
    Vec<float32,3> color = eval_microfacet_reflection(wo,wi,mat.m_ior, ax, ay);

    std::cout<<"color "<<color<<"\n";
    float32 pdf = pdf_microfacet_reflection(wo,wi,mat.m_ior, ax, ay);

    std::cout<<"weight "<<pdf<<"\n";
    std::cout<<"weighted color "<<color/pdf<<"\n";

    wi.normalize();
    wi = to_world * wi;
    std::cout<<"World wi "<<wi<<"\n";
    dirs.push_back(wi);
  }
  dirs.push_back(normal * 2.f);
  dirs.push_back(view* 3.f);
  write_vectors(dirs,"reflection");
}

TEST (dray_test_sampling, dray_ggx)
{
  Array<Vec<uint32,2>> rstate;
  rstate.resize(1);
  bool deterministic = true;
  seed_rng(rstate, deterministic);
  Vec<uint32,2> rand_state = rstate.get_value(0);

  int32 samples = 100;

  Vec<float32,3> normal = {{0.f, 1.f, 0.f}};

  std::vector<Vec<float32,3>> dirs;
  for(int i = 0; i < samples; ++i)
  {
    Vec<float32,2> rand;
    rand[0] = randomf(rand_state);
    rand[1] = randomf(rand_state);
    Vec<float32,3> new_dir = sample_ggx(0.f, rand);
    new_dir.normalize();
    dirs.push_back(new_dir);
  }
  write_vectors(dirs,"ggx");
}

TEST (dray_test_sampling, dray_cos)
{
  Array<Vec<uint32,2>> rstate;
  rstate.resize(1);
  bool deterministic = true;
  seed_rng(rstate, deterministic);
  Vec<uint32,2> rand_state = rstate.get_value(0);

  int32 samples = 100;

  Vec<float32,3> normal = {{0.f, 1.f, 0.f}};
  std::vector<Vec<float32,3>> dirs;


  for(int i = 0; i < samples; ++i)
  {
    Vec<float32,2> rand;
    rand[0] = randomf(rand_state);
    rand[1] = randomf(rand_state);
    Vec<float32,3> new_dir = cosine_weighted_hemisphere (normal, rand);
    new_dir.normalize();
    dirs.push_back(new_dir);
  }
  write_vectors(dirs,"cosine_weighted");
}

TEST (dray_test_sampling, dray_spec)
{
  Array<Vec<uint32,2>> rstate;
  rstate.resize(1);
  bool deterministic = true;
  seed_rng(rstate, deterministic);
  Vec<uint32,2> rand_state = rstate.get_value(0);

  int32 samples = 100;
  Vec<float32,3> normal = {{-0.296209, 0, -0.955123}};
  Vec<float32,3> view = {{-0.0175326, 0.107532, -0.994047}};
  //Vec<float32,3> normal = {{0.f, 1.f, 0.f}};
  //Vec<float32,3> view = {{0.8f, .1f, 0.f}};
  view.normalize();

  const float32 roughness = 0.005f;

  std::vector<Vec<float32,3>> dirs;
  for(int i = 0; i < samples; ++i)
  {
    Vec<float32,2> rand;
    rand[0] = randomf(rand_state);
    rand[1] = randomf(rand_state);
    Vec<float32,3> new_dir = specular_sample(normal, view, rand, roughness, true);
    Vec<float32,3> half = new_dir + view;
    half.normalize();
    float32 pdf = eval_pdf(new_dir,view,normal,roughness,0.f);
    //dirs.push_back(half*3.f);
    dirs.push_back(new_dir);
  }

  dirs.push_back(normal * 2.f);
  dirs.push_back(view* 3.f);

  write_vectors(dirs,"specular");
}

void write_vectors(std::vector<Vec<float32,3>> &dirs, std::string name)
{
  std::vector<float> x;
  std::vector<float> y;
  std::vector<float> z;
  std::vector<int32> conn;


  x.push_back(0.f);
  y.push_back(0.f);
  z.push_back(0.f);

  int conn_count = 1;

  for(auto d : dirs)
  {
    x.push_back(d[0]);
    y.push_back(d[1]);
    z.push_back(d[2]);
    conn.push_back(0);
    conn.push_back(conn_count);
    conn_count++;
  }

  conduit::Node domain;

  domain["coordsets/coords/type"] = "explicit";
  domain["coordsets/coords/values/x"].set(x);
  domain["coordsets/coords/values/y"].set(y);
  domain["coordsets/coords/values/z"].set(z);
  domain["topologies/mesh/type"] = "unstructured";
  domain["topologies/mesh/coordset"] = "coords";
  domain["topologies/mesh/elements/shape"] = "line";
  domain["topologies/mesh/elements/connectivity"].set(conn);

  conduit::Node dataset;
  dataset.append() = domain;
  conduit::Node info;
  if(!conduit::blueprint::mesh::verify(dataset,info))
  {
    info.print();
  }
  conduit::relay::io_blueprint::save(domain, name+".blueprint_root");
}
