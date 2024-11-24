//
// Created by pvl on 24.11.24.
//

#ifndef XML_RENDERER_H
#define XML_RENDERER_H
#include <examples/xml/xml.pb.h>

#include <string>

std::string render(const google::protobuf::Message& message);

#endif //XML_RENDERER_H
