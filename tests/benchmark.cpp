// ======================================================================== //
// Copyright 2009-2013 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "sys/platform.h"
#include "sys/ref.h"
#include "embree2/rtcore.h"
#include "embree2/rtcore_ray.h"
#include "math/vec3.h"
#include <vector>

namespace embree
{
  RTCAlgorithmFlags aflags = (RTCAlgorithmFlags) (RTC_INTERSECT1 | RTC_INTERSECT4 | RTC_INTERSECT8 | RTC_INTERSECT16);

  /* configuration */
  static std::string g_rtcore = "";
  static size_t testN = 100000;

  /* vertex and triangle layout */
  struct Vertex   { float x,y,z,a; };
  struct Triangle { int v0, v1, v2; };

#define AssertNoError() \
  if (rtcGetError() != RTC_NO_ERROR) return false;
#define AssertAnyError() \
  if (rtcGetError() == RTC_NO_ERROR) return false;
#define AssertError(code) \
  if (rtcGetError() != code) return false;
#define POSITIVE(name,test) {                                               \
    bool ok = test;                                                     \
    printf("%30s ... %s\n",name,ok ? "\033[32m[PASSED]\033[0m" : "\033[31m[FAILED]\033[0m");          \
  }
#if defined(__EXIT_ON_ERROR__)
#define NEGATIVE(name,test)
#else
#define NEGATIVE(name,test) {                                           \
    bool notok = test;                                                  \
    printf("%30s ... %s\n",name,notok ? "\033[31m[FAILED]\033[0m" : "\033[32m[PASSED]\033[0m");       \
  }
#endif
#define COUNT(name,test) {                                              \
    size_t notok = test;                                                \
    printf("%30s ... %s (%f%%)\n",name,notok ? "\033[31m[FAILED]\033[0m" : "\033[32m[PASSED]\033[0m", 100.0f*(double)notok/(double)testN); \
  }
#define BUILD(name,test) {                                              \
    double perf = test;                                                \
    printf("%30s ... %f Mtris/s\n",name,perf*1E-6);                                 \
  }

  RTCRay makeRay(Vec3f org, Vec3f dir) 
  {
    RTCRay ray;
    ray.org[0] = org.x; ray.org[1] = org.y; ray.org[2] = org.z;
    ray.dir[0] = dir.x; ray.dir[1] = dir.y; ray.dir[2] = dir.z;
    ray.tnear = 0.0f; ray.tfar = inf;
    ray.time = 0; ray.mask = -1;
    ray.geomID = ray.primID = ray.instID = -1;
    return ray;
  }

  RTCRay makeRay(Vec3f org, Vec3f dir, float tnear, float tfar) 
  {
    RTCRay ray;
    ray.org[0] = org.x; ray.org[1] = org.y; ray.org[2] = org.z;
    ray.dir[0] = dir.x; ray.dir[1] = dir.y; ray.dir[2] = dir.z;
    ray.tnear = tnear; ray.tfar = tfar;
    ray.time = 0; ray.mask = -1;
    ray.geomID = ray.primID = ray.instID = -1;
    return ray;
  }
  
  void setRay(RTCRay4& ray_o, int i, const RTCRay& ray_i)
  {
    ray_o.orgx[i] = ray_i.org[0];
    ray_o.orgy[i] = ray_i.org[1];
    ray_o.orgz[i] = ray_i.org[2];
    ray_o.dirx[i] = ray_i.dir[0];
    ray_o.diry[i] = ray_i.dir[1];
    ray_o.dirz[i] = ray_i.dir[2];
    ray_o.tnear[i] = ray_i.tnear;
    ray_o.tfar[i] = ray_i.tfar;
    ray_o.time[i] = ray_i.time;
    ray_o.mask[i] = ray_i.mask;
    ray_o.geomID[i] = ray_i.geomID;
    ray_o.primID[i] = ray_i.primID;
    ray_o.instID[i] = ray_i.instID;
  }

  void setRay(RTCRay8& ray_o, int i, const RTCRay& ray_i)
  {
    ray_o.orgx[i] = ray_i.org[0];
    ray_o.orgy[i] = ray_i.org[1];
    ray_o.orgz[i] = ray_i.org[2];
    ray_o.dirx[i] = ray_i.dir[0];
    ray_o.diry[i] = ray_i.dir[1];
    ray_o.dirz[i] = ray_i.dir[2];
    ray_o.tnear[i] = ray_i.tnear;
    ray_o.tfar[i] = ray_i.tfar;
    ray_o.time[i] = ray_i.time;
    ray_o.mask[i] = ray_i.mask;
    ray_o.geomID[i] = ray_i.geomID;
    ray_o.primID[i] = ray_i.primID;
    ray_o.instID[i] = ray_i.instID;
  }

  void setRay(RTCRay16& ray_o, int i, const RTCRay& ray_i)
  {
    ray_o.orgx[i] = ray_i.org[0];
    ray_o.orgy[i] = ray_i.org[1];
    ray_o.orgz[i] = ray_i.org[2];
    ray_o.dirx[i] = ray_i.dir[0];
    ray_o.diry[i] = ray_i.dir[1];
    ray_o.dirz[i] = ray_i.dir[2];
    ray_o.tnear[i] = ray_i.tnear;
    ray_o.tfar[i] = ray_i.tfar;
    ray_o.time[i] = ray_i.time;
    ray_o.mask[i] = ray_i.mask;
    ray_o.geomID[i] = ray_i.geomID;
    ray_o.primID[i] = ray_i.primID;
    ray_o.instID[i] = ray_i.instID;
  }

  static void parseCommandLine(int argc, char** argv)
  {
    for (int i=1; i<argc; i++)
    {
      std::string tag = argv[i];
      if (tag == "") return;

      /* rtcore configuration */
      else if (tag == "-rtcore" && i+1<argc) {
        g_rtcore = argv[++i];
      }

      /* skip unknown command line parameter */
      else {
        std::cerr << "unknown command line parameter: " << tag << " ";
        std::cerr << std::endl;
      }
    }
  }

  struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
  };

  void createSphereMesh (const Vec3f pos, const float r, size_t numPhi, Mesh& mesh_o)
  {
    /* create a triangulated sphere */
    size_t numTheta = 2*numPhi;
    mesh_o.vertices.resize(numTheta*(numPhi+1));
    mesh_o.triangles.resize(2*numTheta*(numPhi-1));
    
    /* map triangle and vertex buffer */
    Vertex*   vertices  = (Vertex*  ) &mesh_o.vertices[0];
    Triangle* triangles = (Triangle*) &mesh_o.triangles[0];
    
    /* create sphere geometry */
    int tri = 0;
    const float rcpNumTheta = 1.0f/float(numTheta);
    const float rcpNumPhi   = 1.0f/float(numPhi);
    for (size_t phi=0; phi<=numPhi; phi++)
    {
      for (size_t theta=0; theta<numTheta; theta++)
      {
        const float phif   = phi*float(pi)*rcpNumPhi;
        const float thetaf = theta*2.0f*float(pi)*rcpNumTheta;
        Vertex& v = vertices[phi*numTheta+theta];
        v.x = pos.x + r*sin(phif)*sin(thetaf);
        v.y = pos.y + r*cos(phif);
        v.z = pos.z + r*sin(phif)*cos(thetaf);
      }
      if (phi == 0) continue;
      
      for (size_t theta=1; theta<=numTheta; theta++) 
      {
        int p00 = (phi-1)*numTheta+theta-1;
        int p01 = (phi-1)*numTheta+theta%numTheta;
        int p10 = phi*numTheta+theta-1;
        int p11 = phi*numTheta+theta%numTheta;
        
        if (phi > 1) {
          triangles[tri].v0 = p10; 
          triangles[tri].v1 = p00; 
          triangles[tri].v2 = p01; 
          tri++;
        }
        
        if (phi < numPhi) {
          triangles[tri].v0 = p11; 
          triangles[tri].v1 = p10;
          triangles[tri].v2 = p01; 
        tri++;
        }
      }
    }
  }

  unsigned addSphere (RTCScene scene, RTCFlags flag, const Vec3f pos, const float r, size_t numPhi)
  {
    Mesh mesh; createSphereMesh (pos, r, numPhi, mesh);
    unsigned geom = rtcNewTriangleMesh (scene, RTC_STATIC, mesh.triangles.size(), mesh.vertices.size());
    memcpy(rtcMapBuffer(scene,geom,RTC_VERTEX_BUFFER), &mesh.vertices[0], mesh.vertices.size()*sizeof(Vertex));
    memcpy(rtcMapBuffer(scene,geom,RTC_INDEX_BUFFER ), &mesh.triangles[0], mesh.triangles.size()*sizeof(Triangle));
    rtcUnmapBuffer(scene,geom,RTC_VERTEX_BUFFER);
    rtcUnmapBuffer(scene,geom,RTC_INDEX_BUFFER);
    return geom;
  }

  double rtcore_create_geometry(RTCFlags flags0, RTCFlags flags1, size_t numPhi, size_t numMeshes)
  {
    Mesh mesh; createSphereMesh (Vec3f(0,0,0), 1, numPhi, mesh);

    double t0 = getSeconds();
    RTCScene scene = rtcNewScene(flags0,aflags);

    for (size_t i=0; i<numMeshes; i++) 
    {
      unsigned geom = rtcNewTriangleMesh (scene, flags1, mesh.triangles.size(), mesh.vertices.size());
      memcpy(rtcMapBuffer(scene,geom,RTC_VERTEX_BUFFER), &mesh.vertices[0], mesh.vertices.size()*sizeof(Vertex));
      memcpy(rtcMapBuffer(scene,geom,RTC_INDEX_BUFFER ), &mesh.triangles[0], mesh.triangles.size()*sizeof(Triangle));
      rtcUnmapBuffer(scene,geom,RTC_VERTEX_BUFFER);
      rtcUnmapBuffer(scene,geom,RTC_INDEX_BUFFER);
      for (size_t i=0; i<mesh.vertices.size(); i++) {
        mesh.vertices[i].x += 1.0f;
        mesh.vertices[i].y += 1.0f;
        mesh.vertices[i].z += 1.0f;
      }
    }
    rtcCommit (scene);
    double t1 = getSeconds();
    rtcDeleteScene(scene);

    size_t numTriangles = mesh.triangles.size() * numMeshes;
    return double(numTriangles)/(t1-t0);
  }
  
  double rtcore_update_geometry(RTCFlags flags, size_t numPhi, size_t numMeshes)
  {
    Mesh mesh; createSphereMesh (Vec3f(0,0,0), 1, numPhi, mesh);
    RTCScene scene = rtcNewScene(RTC_DYNAMIC,aflags);

    for (size_t i=0; i<numMeshes; i++) 
    {
      unsigned geom = rtcNewTriangleMesh (scene, flags, mesh.triangles.size(), mesh.vertices.size());
      memcpy(rtcMapBuffer(scene,geom,RTC_VERTEX_BUFFER), &mesh.vertices[0], mesh.vertices.size()*sizeof(Vertex));
      memcpy(rtcMapBuffer(scene,geom,RTC_INDEX_BUFFER ), &mesh.triangles[0], mesh.triangles.size()*sizeof(Triangle));
      rtcUnmapBuffer(scene,geom,RTC_VERTEX_BUFFER);
      rtcUnmapBuffer(scene,geom,RTC_INDEX_BUFFER);
      for (size_t i=0; i<mesh.vertices.size(); i++) {
        mesh.vertices[i].x += 1.0f;
        mesh.vertices[i].y += 1.0f;
        mesh.vertices[i].z += 1.0f;
      }
    }
    rtcCommit (scene);

    double t0 = getSeconds();
    for (size_t i=0; i<numMeshes; i++) rtcUpdate(scene,i);
    rtcCommit (scene);
    double t1 = getSeconds();
    rtcDeleteScene(scene);

    size_t numTriangles = mesh.triangles.size() * numMeshes;
    return double(numTriangles)/(t1-t0);
  }

  void rtcore_coherent_intersect1(RTCScene scene)
  {
    size_t width = 1024;
    size_t height = 1024;
    float rcpWidth = 1.0f/1024.0f;
    float rcpHeight = 1.0f/1024.0f;
    double t0 = getSeconds();
    for (size_t y=0; y<height; y++) {
      for (size_t x=0; x<width; x++) {
        RTCRay ray = makeRay(zero,Vec3f(float(x)*rcpWidth,1,float(y)*rcpHeight));
        rtcIntersect(scene,ray);
      }
    }
    double t1 = getSeconds();

    printf("%30s ... %f Mrps\n","coherent_intersect1",1E-6*(double)(width*height)/(t1-t0));
  }

  void rtcore_coherent_intersect4(RTCScene scene)
  {
    size_t width = 1024;
    size_t height = 1024;
    float rcpWidth = 1.0f/1024.0f;
    float rcpHeight = 1.0f/1024.0f;
    double t0 = getSeconds();
    for (size_t y=0; y<height; y+=2) {
      for (size_t x=0; x<width; x+=2) {
        RTCRay4 ray4; 
        for (size_t dy=0; dy<2; dy++) {
          for (size_t dx=0; dx<2; dx++) {
            setRay(ray4,2*dy+dx,makeRay(zero,Vec3f(float(x+dx)*rcpWidth,1,float(y+dy)*rcpHeight)));
          }
        }
        __align(16) int valid4[4] = { -1,-1,-1,-1 };
        rtcIntersect4(valid4,scene,ray4);
      }
    }
    double t1 = getSeconds();

    printf("%30s ... %f Mrps\n","coherent_intersect4",1E-6*(double)(width*height)/(t1-t0));
  }

  void rtcore_coherent_intersect8(RTCScene scene)
  {
    size_t width = 1024;
    size_t height = 1024;
    float rcpWidth = 1.0f/1024.0f;
    float rcpHeight = 1.0f/1024.0f;
    double t0 = getSeconds();
    for (size_t y=0; y<height; y+=4) {
      for (size_t x=0; x<width; x+=2) {
        RTCRay8 ray8; 
        for (size_t dy=0; dy<4; dy++) {
          for (size_t dx=0; dx<2; dx++) {
            setRay(ray8,2*dy+dx,makeRay(zero,Vec3f(float(x+dx)*rcpWidth,1,float(y+dy)*rcpHeight)));
          }
        }
        __align(32) int valid8[8] = { -1,-1,-1,-1,-1,-1,-1,-1 };
        rtcIntersect8(valid8,scene,ray8);
      }
    }
    double t1 = getSeconds();

    printf("%30s ... %f Mrps\n","coherent_intersect8",1E-6*(double)(width*height)/(t1-t0));
  }

  void rtcore_coherent_intersect16(RTCScene scene)
  {
    size_t width = 1024;
    size_t height = 1024;
    float rcpWidth = 1.0f/1024.0f;
    float rcpHeight = 1.0f/1024.0f;
    double t0 = getSeconds();
    for (size_t y=0; y<height; y+=4) {
      for (size_t x=0; x<width; x+=4) {
        RTCRay16 ray16; 
        for (size_t dy=0; dy<4; dy++) {
          for (size_t dx=0; dx<4; dx++) {
            setRay(ray16,4*dy+dx,makeRay(zero,Vec3f(float(x+dx)*rcpWidth,1,float(y+dy)*rcpHeight)));
          }
        }
        __align(64) int valid16[16] = { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 };
        rtcIntersect16(valid16,scene,ray16);
      }
    }
    double t1 = getSeconds();

    printf("%30s ... %f Mrps\n","coherent_intersect16",1E-6*(double)(width*height)/(t1-t0));
  }

  void rtcore_incoherent_intersect1(RTCScene scene, Vec3f* numbers, size_t N)
  {
    double t0 = getSeconds();
    for (size_t i=0; i<N; i++) {
      RTCRay ray = makeRay(zero,numbers[i]);
      rtcIntersect(scene,ray);
    }
    double t1 = getSeconds();

    printf("%30s ... %f Mrps\n","incoherent_intersect1",1E-6*(double)N/(t1-t0));
  }

  void rtcore_incoherent_intersect4(RTCScene scene, Vec3f* numbers, size_t N)
  {
    double t0 = getSeconds();
    for (size_t i=0; i<N; i+=4) {
      RTCRay4 ray4;
      for (size_t j=0; j<4; j++) {
        setRay(ray4,j,makeRay(zero,numbers[i+j]));
      }
      __align(16) int valid4[4] = { -1,-1,-1,-1 };
      rtcIntersect4(valid4,scene,ray4);
    }
    double t1 = getSeconds();

    printf("%30s ... %f Mrps\n","incoherent_intersect4",1E-6*(double)N/(t1-t0));
  }

  void rtcore_incoherent_intersect8(RTCScene scene, Vec3f* numbers, size_t N)
  {
    double t0 = getSeconds();
    for (size_t i=0; i<N; i+=8) {
      RTCRay8 ray8;
      for (size_t j=0; j<8; j++) {
        setRay(ray8,j,makeRay(zero,numbers[i+j]));
      }
      __align(16) int valid8[8] = { -1,-1,-1,-1,-1,-1,-1,-1 };
      rtcIntersect8(valid8,scene,ray8);
    }
    double t1 = getSeconds();

    printf("%30s ... %f Mrps\n","incoherent_intersect8",1E-6*(double)N/(t1-t0));
  }

  void rtcore_incoherent_intersect16(RTCScene scene, Vec3f* numbers, size_t N)
  {
    double t0 = getSeconds();
    for (size_t i=0; i<N; i+=16) {
      RTCRay16 ray16;
      for (size_t j=0; j<16; j++) {
        setRay(ray16,j,makeRay(zero,numbers[i+j]));
      }
      __align(64) int valid16[16] = { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 };
      rtcIntersect16(valid16,scene,ray16);
    }
    double t1 = getSeconds();

    printf("%30s ... %f Mrps\n","incoherent_intersect16",1E-6*(double)N/(t1-t0));
  }

  void rtcore_intersect_benchmark(RTCFlags flags, size_t numPhi)
  {
    RTCScene scene = rtcNewScene(flags,aflags);
    addSphere (scene, flags, zero, 1, numPhi);
    rtcCommit (scene);

    rtcore_coherent_intersect1(scene);
#if !defined(__MIC__)
    rtcore_coherent_intersect4(scene);
    rtcore_coherent_intersect8(scene);
#endif
    rtcore_coherent_intersect16(scene);

    size_t N = 1024*1024;
    Vec3f* numbers = new Vec3f[N];
    for (size_t i=0; i<N; i++) {
      float x = 2.0f*drand48()-1.0f;
      float y = 2.0f*drand48()-1.0f;
      float z = 2.0f*drand48()-1.0f;
      numbers[i] = Vec3f(x,y,z);
    }

    rtcore_incoherent_intersect1(scene,numbers,N);
#if !defined(__MIC__)
    rtcore_incoherent_intersect4(scene,numbers,N);
    rtcore_incoherent_intersect8(scene,numbers,N);
#endif
    rtcore_incoherent_intersect16(scene,numbers,N);

    delete numbers;

    rtcDeleteScene(scene);
  }

  /* main function in embree namespace */
  int main(int argc, char** argv) 
  {
    /* parse command line */  
    parseCommandLine(argc,argv);

    /* perform tests */
    rtcInit(g_rtcore.c_str());
    
    rtcore_intersect_benchmark(RTC_STATIC, 501);

    BUILD   ("create_static_geometry_120",       rtcore_create_geometry(RTC_STATIC,RTC_STATIC,6,1));
    BUILD   ("create_static_geometry_1k",        rtcore_create_geometry(RTC_STATIC,RTC_STATIC,17,1));
    BUILD   ("create_static_geometry_10k",       rtcore_create_geometry(RTC_STATIC,RTC_STATIC,51,1));
    BUILD   ("create_static_geometry_100k",      rtcore_create_geometry(RTC_STATIC,RTC_STATIC,159,1));
    BUILD   ("create_static_geometry_1000k_1",   rtcore_create_geometry(RTC_STATIC,RTC_STATIC,501,1));
    BUILD   ("create_static_geometry_100k_10",   rtcore_create_geometry(RTC_STATIC,RTC_STATIC,159,10));
    BUILD   ("create_static_geometry_10k_100",   rtcore_create_geometry(RTC_STATIC,RTC_STATIC,51,100));
    BUILD   ("create_static_geometry_1k_1000",   rtcore_create_geometry(RTC_STATIC,RTC_STATIC,17,1000));
    BUILD   ("create_static_geometry_120_10000", rtcore_create_geometry(RTC_STATIC,RTC_STATIC,6,8334));

    BUILD   ("create_dynamic_geometry_120",       rtcore_create_geometry(RTC_DYNAMIC,RTC_STATIC,6,1));
    BUILD   ("create_dynamic_geometry_1k",        rtcore_create_geometry(RTC_DYNAMIC,RTC_STATIC,17,1));
    BUILD   ("create_dynamic_geometry_10k",       rtcore_create_geometry(RTC_DYNAMIC,RTC_STATIC,51,1));
    BUILD   ("create_dynamic_geometry_100k",      rtcore_create_geometry(RTC_DYNAMIC,RTC_STATIC,159,1));
    BUILD   ("create_dynamic_geometry_1000k_1",   rtcore_create_geometry(RTC_DYNAMIC,RTC_STATIC,501,1));
    BUILD   ("create_dynamic_geometry_100k_10",   rtcore_create_geometry(RTC_DYNAMIC,RTC_STATIC,159,10));
    BUILD   ("create_dynamic_geometry_10k_100",   rtcore_create_geometry(RTC_DYNAMIC,RTC_STATIC,51,100));
    BUILD   ("create_dynamic_geometry_1k_1000",   rtcore_create_geometry(RTC_DYNAMIC,RTC_STATIC,17,1000));
    BUILD   ("create_dynamic_geometry_120_10000", rtcore_create_geometry(RTC_DYNAMIC,RTC_STATIC,6,8334));

    BUILD   ("refit_geometry_120",        rtcore_update_geometry(RTC_DEFORMABLE,6,1));
    BUILD   ("refit_geometry_1k",         rtcore_update_geometry(RTC_DEFORMABLE,17,1));
    BUILD   ("refit_geometry_10k",        rtcore_update_geometry(RTC_DEFORMABLE,51,1));
    BUILD   ("refit_geometry_100k",       rtcore_update_geometry(RTC_DEFORMABLE,159,1));
    BUILD   ("refit_geometry_1000k_1",    rtcore_update_geometry(RTC_DEFORMABLE,501,1));
    BUILD   ("refit_geometry_100k_10",    rtcore_update_geometry(RTC_DEFORMABLE,159,10));
    BUILD   ("refit_geometry_10k_100",    rtcore_update_geometry(RTC_DEFORMABLE,51,100));
    BUILD   ("refit_geometry_1k_1000",    rtcore_update_geometry(RTC_DEFORMABLE,17,1000));
    BUILD   ("refit_geometry_120_10000",  rtcore_update_geometry(RTC_DEFORMABLE,6,8334));

    BUILD   ("update_geometry_120",        rtcore_update_geometry(RTC_DYNAMIC,6,1));
    BUILD   ("update_geometry_1k",         rtcore_update_geometry(RTC_DYNAMIC,17,1));
    BUILD   ("update_geometry_10k",        rtcore_update_geometry(RTC_DYNAMIC,51,1));
    BUILD   ("update_geometry_100k",       rtcore_update_geometry(RTC_DYNAMIC,159,1));
    BUILD   ("update_geometry_1000k_1",    rtcore_update_geometry(RTC_DYNAMIC,501,1));
    BUILD   ("update_geometry_100k_10",    rtcore_update_geometry(RTC_DYNAMIC,159,10));
    BUILD   ("update_geometry_10k_100",    rtcore_update_geometry(RTC_DYNAMIC,51,100));
    BUILD   ("update_geometry_1k_1000",    rtcore_update_geometry(RTC_DYNAMIC,17,1000));
    BUILD   ("update_geometry_120_10000",  rtcore_update_geometry(RTC_DYNAMIC,6,8334));

    rtcExit();

    return 0;
  }
}

int main(int argc, char** argv)
{
  try {
    return embree::main(argc, argv);
  }
  catch (const std::exception& e) {
    std::cout << "Error: " << e.what() << std::endl;
    return 1;
  }
  catch (...) {
    std::cout << "Error: unknown exception caught." << std::endl;
    return 1;
  }
}