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

#include "obj_loader.h"

#include <fstream>
#include <iostream>
#include <map>
#include <vector>
#include <string.h>

namespace embree
{
  /*! Three-index vertex, indexing start at 0, -1 means invalid vertex. */
  struct Vertex {
    int v, vt, vn;
    Vertex() {};
    Vertex(int v) : v(v), vt(v), vn(v) {};
    Vertex(int v, int vt, int vn) : v(v), vt(vt), vn(vn) {};
  };

  static inline bool operator < ( const Vertex& a, const Vertex& b ) {
    if (a.v  != b.v)  return a.v  < b.v;
    if (a.vn != b.vn) return a.vn < b.vn;
    if (a.vt != b.vt) return a.vt < b.vt;
    return false;
  }

  /*! Fill space at the end of the token with 0s. */
  static inline const char* trimEnd(const char* token) {
    size_t len = strlen(token);
    if (len == 0) return token;
    char* pe = (char*)(token + len - 1);
    while ((*pe == ' ' || *pe == '\t' || *pe == '\r') && pe >= token) *pe-- = 0;
    return token;
  }

  /*! Determine if character is a separator. */
  static inline bool isSep(const char c) {
    return (c == ' ') || (c == '\t');
  }

  /*! Parse separator. */
  static inline const char* parseSep(const char*& token) {
    size_t sep = strspn(token, " \t");
    if (!sep) throw std::runtime_error("separator expected");
    return token+=sep;
  }

  /*! Parse optional separator. */
  static inline const char* parseSepOpt(const char*& token) {
    return token+=strspn(token, " \t");
  }

  /*! Read float from a string. */
  static inline float getFloat(const char*& token) {
    token += strspn(token, " \t");
    float n = (float)atof(token);
    token += strcspn(token, " \t\r");
    return n;
  }

  /*! Read Vec2f from a string. */
  static inline Vec2f getVec2f(const char*& token) {
    float x = getFloat(token);
    float y = getFloat(token);
    return Vec2f(x,y);
  }

  /*! Read Vec3f from a string. */
  static inline Vec3f getVec3f(const char*& token) {
    float x = getFloat(token);
    float y = getFloat(token);
    float z = getFloat(token);
    return Vec3f(x,y,z);
  }

  class OBJLoader
  {
  public:

    /*! Constructor. */
    OBJLoader(const FileName& fileName, OBJScene& mesh, const Vec3fa& offset);

    /*! Destruction */
    ~OBJLoader();
 
    /*! Public methods. */
    void loadMTL(const FileName& fileName);

  private:

    /*! file to load */
    FileName path;

    /*! output model */
    OBJScene& model;
    
    /*! Geometry buffer. */
    vector_t<Vec3fa> v;
    vector_t<Vec3fa> vn;
    std::vector<Vec2f> vt;
    std::vector<std::vector<Vertex> > curGroup;
    Vec3fa offset;

    /*! Material handling. */
    int curMaterial;
    std::map<std::string, int> material;

    /*! Internal methods. */
    int fix_v (int index);
    int fix_vt(int index);
    int fix_vn(int index);
    void flushFaceGroup();
    Vertex getInt3(const char*& token);
    uint32 getVertex(std::map<Vertex,uint32>& vertexMap, OBJScene::Mesh* mesh, const Vertex& i);
  };

  OBJLoader::OBJLoader(const FileName &fileName, OBJScene& mesh, const Vec3fa& offset) 
    : path(fileName.path()), model(mesh), offset(offset)
  {
    /* open file */
    std::ifstream cin;
    cin.open(fileName.c_str());
    if (!cin.is_open()) {
      throw std::runtime_error("cannot open " + fileName.str());
      return;
    }

    /* generate default material */
    model.materials.push_back(OBJScene::Material());
    curMaterial = 0;

    char line[10000];
    memset(line, 0, sizeof(line));

    while (cin.peek() != -1)
    {
      /* load next multiline */
      char* pline = line;
      while (true) {
        cin.getline(pline, sizeof(line) - (pline - line) - 16, '\n');
        ssize_t last = strlen(pline) - 1;
        if (last < 0 || pline[last] != '\\') break;
        pline += last;
        *pline++ = ' ';
      }

      const char* token = trimEnd(line + strspn(line, " \t"));
      if (token[0] == 0) continue;

      /*! parse position */
      if (token[0] == 'v' && isSep(token[1]))                    { v.push_back((Vec3fa)getVec3f(token += 2)-offset); continue; }

      /* parse normal */
      if (token[0] == 'v' && token[1] == 'n' && isSep(token[2])) { vn.push_back((Vec3fa)getVec3f(token += 3)); continue; }

      /* parse texcoord */
      if (token[0] == 'v' && token[1] == 't' && isSep(token[2])) { vt.push_back(getVec2f(token += 3)); continue; }

      /*! parse face */
      if (token[0] == 'f' && isSep(token[1]))
      {
        parseSep(token += 1);

        std::vector<Vertex> face;
        while (token[0]) {
          face.push_back(getInt3(token));
          parseSepOpt(token);
        }
        curGroup.push_back(face);
        continue;
      }

      /*! use material */
      if (!strncmp(token, "usemtl", 6) && isSep(token[6]))
      {
        flushFaceGroup();
        std::string name(parseSep(token += 6));
        if (material.find(name) == material.end()) curMaterial = 0;
        else curMaterial = material[name];
        continue;
      }

      /* load material library */
      if (!strncmp(token, "mtllib", 6) && isSep(token[6])) {
        loadMTL(path + std::string(parseSep(token += 6)));
        continue;
      }

      // ignore unknown stuff
    }
    flushFaceGroup();
    cin.close();
  }

  OBJLoader::~OBJLoader() {
  }

  /* load material file */
  void OBJLoader::loadMTL(const FileName &fileName)
  {
    std::ifstream cin;
    cin.open(fileName.c_str());
    if (!cin.is_open()) {
      std::cerr << "cannot open " << fileName.str() << std::endl;
      return;
    }

    char line[10000];
    memset(line, 0, sizeof(line));

    int cur = 0;
    while (cin.peek() != -1)
    {
      /* load next multiline */
      char* pline = line;
      while (true) {
        cin.getline(pline, sizeof(line) - (pline - line) - 16, '\n');
        ssize_t last = strlen(pline) - 1;
        if (last < 0 || pline[last] != '\\') break;
        pline += last;
        *pline++ = ' ';
      }
      const char* token = trimEnd(line + strspn(line, " \t"));

      if (token[0] == 0  ) continue; // ignore empty lines
      if (token[0] == '#') continue; // ignore comments

      if (!strncmp(token, "newmtl", 6)) {
        parseSep(token+=6);
        std::string name(token);
        material[name] = cur = model.materials.size();
        model.materials.push_back(OBJScene::Material());
        continue;
      }

      if (!cur) throw std::runtime_error("invalid material file: newmtl expected first");

      if (!strncmp(token, "illum", 5)) { parseSep(token += 5);  continue; }

      if (!strncmp(token, "d",  1)) { parseSep(token += 1);  model.materials[cur].d  = getFloat(token); continue; }
      if (!strncmp(token, "Ns", 2)) { parseSep(token += 2);  model.materials[cur].Ns = getFloat(token); continue; }
      if (!strncmp(token, "Ni", 2)) { parseSep(token += 2);  model.materials[cur].Ni = getFloat(token); continue; }

      if (!strncmp(token, "Ka", 2)) { parseSep(token += 2);  model.materials[cur].Ka = getVec3f(token); continue; }
      if (!strncmp(token, "Kd", 2)) { parseSep(token += 2);  model.materials[cur].Kd = getVec3f(token); continue; }
      if (!strncmp(token, "Ks", 2)) { parseSep(token += 2);  model.materials[cur].Ks = getVec3f(token); continue; }
      if (!strncmp(token, "Tf", 2)) { parseSep(token += 2);  model.materials[cur].Tf = getVec3f(token); continue; }
    }
    cin.close();
  }

  /*! handles relative indices and starts indexing from 0 */
  int OBJLoader::fix_v (int index) { return (index > 0 ? index - 1 : (index == 0 ? 0 : (int) v .size() + index)); }
  int OBJLoader::fix_vt(int index) { return (index > 0 ? index - 1 : (index == 0 ? 0 : (int) vt.size() + index)); }
  int OBJLoader::fix_vn(int index) { return (index > 0 ? index - 1 : (index == 0 ? 0 : (int) vn.size() + index)); }

  /*! Parse differently formated triplets like: n0, n0/n1/n2, n0//n2, n0/n1.          */
  /*! All indices are converted to C-style (from 0). Missing entries are assigned -1. */
  Vertex OBJLoader::getInt3(const char*& token)
  {
    Vertex v(-1);
    v.v = fix_v(atoi(token));
    token += strcspn(token, "/ \t\r");
    if (token[0] != '/') return(v);
    token++;

    // it is i//n
    if (token[0] == '/') {
      token++;
      v.vn = fix_vn(atoi(token));
      token += strcspn(token, " \t\r");
      return(v);
    }

    // it is i/t/n or i/t
    v.vt = fix_vt(atoi(token));
    token += strcspn(token, "/ \t\r");
    if (token[0] != '/') return(v);
    token++;

    // it is i/t/n
    v.vn = fix_vn(atoi(token));
    token += strcspn(token, " \t\r");
    return(v);
  }

  uint32 OBJLoader::getVertex(std::map<Vertex,uint32>& vertexMap, OBJScene::Mesh* mesh, const Vertex& i)
  {
    const std::map<Vertex, uint32>::iterator& entry = vertexMap.find(i);
    if (entry != vertexMap.end()) return(entry->second);
    mesh->v.push_back(Vec3fa(v[i.v].x,v[i.v].y,v[i.v].z));
    if (i.vn >= 0) mesh->vn.push_back(vn[i.vn]);
    if (i.vt >= 0) mesh->vt.push_back(vt[i.vt]);
    return(vertexMap[i] = int(mesh->v.size()) - 1);
  }

  /*! end current facegroup and append to mesh */
  void OBJLoader::flushFaceGroup()
  {
    if (curGroup.empty()) return;
    OBJScene::Mesh* mesh = new OBJScene::Mesh;
    model.meshes.push_back(mesh);

    // merge three indices into one
    std::map<Vertex, uint32> vertexMap;
    for (size_t j=0; j < curGroup.size(); j++)
    {
      /* iterate over all faces */
      const std::vector<Vertex>& face = curGroup[j];
      Vertex i0 = face[0], i1 = Vertex(-1), i2 = face[1];

      /* triangulate the face with a triangle fan */
      for (size_t k=2; k < face.size(); k++) {
        i1 = i2; i2 = face[k];
        uint32 v0 = getVertex(vertexMap, mesh, i0);
        uint32 v1 = getVertex(vertexMap, mesh, i1);
        uint32 v2 = getVertex(vertexMap, mesh, i2);
        mesh->triangles.push_back(OBJScene::Triangle(v0,v1,v2,curMaterial));
        assert(v0 < mesh->v.size());
        assert(v1 < mesh->v.size());
        assert(v2 < mesh->v.size());
      }
    }
    curGroup.clear();
  }

  void loadOBJ(const FileName& fileName, OBJScene& mesh_o, const Vec3fa& offset) {
    OBJLoader loader(fileName,mesh_o,offset); 
  }
}

