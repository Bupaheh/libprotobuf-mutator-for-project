//
// Created by pvl on 24.11.24.
//

#include "xml_renderer.h"

#include "printer.h"
#include "string"

using namespace google::protobuf;

namespace {

std::string attribute_name = "xml_attributes";

void render_message(const Message& message, const std::string& name, xml::Printer& out);

void render_field(
  const Message& message,
  const Reflection* reflection,
  const FieldDescriptor* field,
  xml::Printer& out
) {
  switch (field->type()) {
    case internal::FieldDescriptorLite::TYPE_STRING: {
      if (!field->is_repeated()) {
        out.println(reflection->GetString(message, field));
      } else {
        for (int i = 0; i < reflection->FieldSize(message, field); i++) {
          out.println(reflection->GetRepeatedString(message, field, i));
        }
      }
      break;
    }

    case internal::FieldDescriptorLite::TYPE_ENUM: {
      if (!field->is_repeated()) {
        out.println(reflection->GetEnum(message, field)->name());
      } else {
        for (int i = 0; i < reflection->FieldSize(message, field); i++) {
          out.println(reflection->GetRepeatedEnum(message, field, i)->name());
        }
      }
      break;
    }

    case internal::FieldDescriptorLite::TYPE_INT32: {
      if (!field->is_repeated()) {
        out.println(std::to_string(reflection->GetInt32(message, field)));
      } else {
        for (int i = 0; i < reflection->FieldSize(message, field); i++) {
          out.println(std::to_string(reflection->GetRepeatedInt32(message, field, i)));
        }
      }
      break;
    }

    case internal::FieldDescriptorLite::TYPE_UINT32: {
      if (!field->is_repeated()) {
        out.println(std::to_string(reflection->GetUInt32(message, field)));
      } else {
        for (int i = 0; i < reflection->FieldSize(message, field); i++) {
          out.println(std::to_string(reflection->GetRepeatedUInt32(message, field, i)));
        }
      }
      break;
    }

    case internal::FieldDescriptorLite::TYPE_INT64: {
      if (!field->is_repeated()) {
        out.println(std::to_string(reflection->GetInt64(message, field)));
      } else {
        for (int i = 0; i < reflection->FieldSize(message, field); i++) {
          out.println(std::to_string(reflection->GetRepeatedInt64(message, field, i)));
        }
      }
      break;
    }

    case internal::FieldDescriptorLite::TYPE_UINT64: {
      if (!field->is_repeated()) {
        out.println(std::to_string(reflection->GetUInt64(message, field)));
      } else {
        for (int i = 0; i < reflection->FieldSize(message, field); i++) {
          out.println(std::to_string(reflection->GetRepeatedUInt64(message, field, i)));
        }
      }
      break;
    }

    case internal::FieldDescriptorLite::TYPE_MESSAGE: {
      if (!field->is_repeated()) {
        render_message(reflection->GetMessage(message, field), field->name(), out);
      } else {
        for (int i = 0; i < reflection->FieldSize(message, field); i++) {
          render_message(reflection->GetRepeatedMessage(message, field, i), field->name(), out);
        }
      }
      break;
    }

    default: {
      std::cerr << "Not supported field " << field->name() << std::endl;
    }

    break;
  }
}

void render_attribute_field(
  const Message& message,
  const Reflection* reflection,
  const FieldDescriptor* field,
  xml::Printer& out
) {
  out.print(" " + field->name() + "=\"");

  if (field->is_repeated()) {
    std::cerr << "Repeated attributes are not supported";
    return;
  }

  switch (field->type()) {
    case internal::FieldDescriptorLite::TYPE_STRING: {
      out.print(reflection->GetString(message, field));
      break;
    }

    case internal::FieldDescriptorLite::TYPE_ENUM: {
      out.print(reflection->GetEnum(message, field)->name());
      break;
    }

    case internal::FieldDescriptorLite::TYPE_INT32: {
      out.print(std::to_string(reflection->GetInt32(message, field)));
      break;
    }

    case internal::FieldDescriptorLite::TYPE_UINT32: {
      out.print(std::to_string(reflection->GetUInt32(message, field)));
      break;
    }

    case internal::FieldDescriptorLite::TYPE_INT64: {
      out.print(std::to_string(reflection->GetInt64(message, field)));
      break;
    }

    case internal::FieldDescriptorLite::TYPE_UINT64: {
      out.print(std::to_string(reflection->GetUInt64(message, field)));
      break;
    }

    default: {
      std::cerr << "Not supported type " << field->name() << std::endl;
    }

    break;
  }

  out.print("\"");
}

void render_attribute(
  const Message& message,
  const Reflection* reflection,
  const std::vector<const FieldDescriptor*>& fields,
  xml::Printer& out
) {
  const FieldDescriptor *attr_field = nullptr;

  for (auto field : fields) {
    if (field->name() == attribute_name) {
      attr_field = field;
    }
  }

  if (attr_field == nullptr) {
    return;
  }

  const Message& attr_message = reflection->GetMessage(message, attr_field);
  const auto attr_reflection = attr_message.GetReflection();
  const auto attr_descriptor = attr_message.GetDescriptor();

  std::vector<const FieldDescriptor*> attr_fields;
  attr_reflection->ListFields(attr_message, &attr_fields);

  for (auto field : attr_fields) {
    render_attribute_field(attr_message, attr_reflection, field, out);
  }
}

void render_message(const Message& message, const std::string& name, xml::Printer& out) {
  const auto reflection = message.GetReflection();
  const auto descriptor = message.GetDescriptor();

  std::vector<const FieldDescriptor*> fields;
  reflection->ListFields(message, &fields);

  out.print("<" + name);
  render_attribute(message, reflection, fields, out);
  out.println(">");
  out.push_indent();

  for (auto field : fields) {
    if (field->name() == attribute_name) continue;

    render_field(message, reflection, field, out);
  }

  out.pop_indent();
  out.println("</" + name + ">");
  out.println();
}

}



std::string xml::render(const Message& message) {
  auto reflection = message.GetReflection();
  auto descriptor = message.GetDescriptor();

  assert(descriptor->field_count() == 1);
  const auto field = descriptor->field(0);

  const Message& body = reflection->GetMessage(message, field);

  xml::Printer printer;
  render_message(body, field->name(), printer);
  return printer.str();
}

