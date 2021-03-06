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

#include "sys/platform.h"
#include "sys/ref.h"
#include "sys/filename.h"
#include "sys/stl/string.h"

#include <vector>
#include <iostream>
#include <cstdio>
#include <string.h>

namespace embree
{
  class String : public RefCount {
  public:
    String(std::string str) : str(str) {}
    std::string str;
  };

  /*! stores the location of a stream element in the source */
  class ParseLocation
  {
  public:
    ParseLocation () : fileName(NULL), lineNumber(-1), colNumber(-1), charNumber(-1) {}
    ParseLocation (Ref<String> fileName, ssize_t lineNumber, ssize_t colNumber, ssize_t charNumber)
      : fileName(fileName), lineNumber(lineNumber), colNumber(colNumber), charNumber(charNumber) {}

    std::string str() const
    {
      if (!fileName) return "unknown";
      std::string str = fileName->str;
      if (lineNumber >= 0) str += " line " + std::stringOf(lineNumber);
      if (lineNumber >= 0 && colNumber >= 0) str += " character " + std::stringOf(colNumber);
      return str;
    }

  private:
    Ref<String> fileName;        /// name of the file (or stream) the token is from
    ssize_t lineNumber;           /// the line number the token is from
    ssize_t colNumber;            /// the character number in the current line
    ssize_t charNumber;           /// the character in the file
  };

  /*! a stream class templated over the stream elements */
  template<typename T> class Stream : public RefCount
  {
    enum { BUF_SIZE = 1024 };

  private:
    virtual T next() = 0;
    virtual ParseLocation location() = 0;
   __forceinline std::pair<T,ParseLocation> nextHelper() {
     ParseLocation l = location();
     T v = next();
     return std::pair<T,ParseLocation>(v,l);
   }
   __forceinline void push_back(const std::pair<T,ParseLocation>& v) {
     if (past+future == BUF_SIZE) pop_front();
     int end = (start+past+future++)%BUF_SIZE;
     buffer[end] = v;
   }
   __forceinline void pop_front() {
     if (past == 0) throw std::runtime_error("stream buffer empty");
     start = (start+1)%BUF_SIZE; past--;
   }
  public:
   Stream () : start(0), past(0), future(0), buffer(BUF_SIZE) {}
   virtual ~Stream() {}

  public:

   const ParseLocation& loc() {
     if (future == 0) push_back(nextHelper());
     return buffer[(start+past)%BUF_SIZE].second;
    }
    T get() {
      if (future == 0) push_back(nextHelper());
      T t = buffer[(start+past)%BUF_SIZE].first;
      past++; future--;
      return t;
    }
    const T& peek() {
      if (future == 0) push_back(nextHelper());
      return buffer[(start+past)%BUF_SIZE].first;
    }
    const T& unget(size_t n = 1) {
      if (past < n) throw std::runtime_error ("cannot unget that many items");
      past -= n; future += n;
      return peek();
    }
    void drop() {
      if (future == 0) push_back(nextHelper());
      past++; future--;
    }
  private:
    size_t start,past,future;
    std::vector<std::pair<T,ParseLocation> > buffer;
  };

  /*! warps an iostream stream */
  class StdStream : public Stream<int>
  {
  public:
    StdStream (std::istream& cin, const std::string& name = "std::stream")
      : cin(cin), lineNumber(1), colNumber(0), charNumber(0), name(new String(name)) {}
    ~StdStream() {}
    ParseLocation location() {
      return ParseLocation(name,lineNumber,colNumber,charNumber);
    }
    int next() {
      int c = cin.get();
      if (c == '\n') { lineNumber++; colNumber = 0; } else if (c != '\r') colNumber++;
      charNumber++;
      return c;
    }
  private:
    std::istream& cin;
    ssize_t lineNumber;           /// the line number the token is from
    ssize_t colNumber;            /// the character number in the current line
    ssize_t charNumber;           /// the character in the file
    Ref<String> name;        /// name of buffer
  };

  /*! creates a stream from a file */
  class FileStream : public Stream<int>
  {
  public:

    FileStream (FILE* file, const std::string& name = "file")
      : file(file), lineNumber(1), colNumber(0), charNumber(0), name(new String(name)) {}

    FileStream (const FileName& fileName)
      : lineNumber(1), colNumber(0), charNumber(0), name(new String(fileName.str()))
    {
      file = fopen(fileName.c_str(),"r");
      if (file == NULL) throw std::runtime_error("cannot open file " + fileName.str());
    }
    ~FileStream() { fclose(file); }

  public:
    ParseLocation location() {
      return ParseLocation(name,lineNumber,colNumber,charNumber);
    }

    int next() {
      int c = fgetc(file);
      if (c == '\n') { lineNumber++; colNumber = 0; } else if (c != '\r') colNumber++;
      charNumber++;
      return c;
    }

  private:
    FILE* file;
    ssize_t lineNumber;           /// the line number the token is from
    ssize_t colNumber;            /// the character number in the current line
    ssize_t charNumber;           /// the character in the file
    Ref<String> name;        /// name of buffer
  };

  /*! creates a character stream from a command line */
  class CommandLineStream : public Stream<int>
  {
  public:
    CommandLineStream (int argc, char** argv, const std::string& name = "command line")
      : i(0), j(0), charNumber(0), name(new String(name))
    {
      if (argc > 0) charNumber = strlen(argv[0])+1;
      for (ssize_t k=1; k<argc; k++) args.push_back(argv[k]);
    }
    ~CommandLineStream() {}
  public:
    ParseLocation location() {
      return ParseLocation(name,0,charNumber,charNumber);
    }
    int next() {
      if (i == args.size()) return EOF;
      if (j == args[i].size()) { i++; j=0; charNumber++; return ' '; }
      charNumber++;
      return args[i][j++];
    }
  private:
    size_t i,j;
    std::vector<std::string> args;
    ssize_t charNumber;           /// the character in the file
    Ref<String> name;        /// name of buffer
  };
}
