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

#pragma once

/* size of screen tiles */
#define TILE_SIZE_X 8
#define TILE_SIZE_Y 8

/* vertex and triangle layout */
struct Vertex   { float x,y,z,r; };
struct Triangle { int v0, v1, v2; };

#include "embree2/rtcore.h"
#include "ray.h"
#include "sys/taskscheduler.h"
using namespace embree;

/* returns time stamp counter */
extern "C" int64 get_tsc();

/* face forward for shading normals */
__forceinline Vec3f faceforward( const Vec3f& N, const Vec3f& I, const Vec3f& Ng ) {
  return dot(I, Ng) < 0 ? N : -N;
}

/* glut keys codes */
#define GLUT_KEY_F1 1
#define GLUT_KEY_F2 2
#define GLUT_KEY_F3 3
#define GLUT_KEY_F4 4
#define GLUT_KEY_F5 5
#define GLUT_KEY_F6 6
#define GLUT_KEY_F7 7
#define GLUT_KEY_F8 8
#define GLUT_KEY_F9 9
#define GLUT_KEY_F10 10

/* standard shading function */
typedef Vec3fa (* renderPixelFunc)(float x, float y, const Vec3fa& vx, const Vec3fa& vy, const Vec3fa& vz, const Vec3fa& p);

Vec3fa renderPixelStandard(float x, float y, const Vec3fa& vx, const Vec3fa& vy, const Vec3fa& vz, const Vec3fa& p);
Vec3fa renderPixelUV      (float x, float y, const Vec3fa& vx, const Vec3fa& vy, const Vec3fa& vz, const Vec3fa& p);
Vec3fa renderPixelGeomIDPrimID(float x, float y, const Vec3fa& vx, const Vec3fa& vy, const Vec3fa& vz, const Vec3fa& p);
Vec3fa renderPixelGeomID      (float x, float y, const Vec3fa& vx, const Vec3fa& vy, const Vec3fa& vz, const Vec3fa& p);


__forceinline Vec3f  neg(const Vec3f& a ) { return -a; }
__forceinline Vec3fa neg(const Vec3fa& a) { return -a; }

/* parallel invokation of renderTile function */
void launch_renderTile (int numTiles, 
                        int* pixels, const int width, const int height, const float time, 
                        const Vec3f& vx, const Vec3f& vy, const Vec3f& vz, const Vec3f& p, const int numTilesX, const int numTilesY);
