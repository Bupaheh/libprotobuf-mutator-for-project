// Copyright 2017 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <fstream>
#include <vector>

#include "examples/xml/xml.pb.h"
#include "src/libfuzzer/libfuzzer_macro.h"
#include "xml_renderer.h"

#include "xml2json.hpp"

int counter = 0;

DEFINE_PROTO_FUZZER(const protobuf_mutator::xml::Input& message) {
  const std::string xml = xml::render(message);
  std::string res = "";

  try {
    res = xml2json(xml.c_str());
  } catch(const std::exception& e) {
    std::cerr << "Exception was thrown: " << e.what() << '\n';
  }

  if (res == "") {
    std::cerr << "Invalid result\n";
  }
}
