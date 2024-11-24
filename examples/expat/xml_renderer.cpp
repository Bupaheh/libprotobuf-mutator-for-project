//
// Created by pvl on 24.11.24.
//

#include "xml_renderer.h"

#include "printer.h"

using namespace google::protobuf;

namespace {

void render_message(const Message& message, xml::Printer& out);

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

    case internal::FieldDescriptorLite::TYPE_MESSAGE: {
      if (!field->is_repeated()) {
        render_message(reflection->GetMessage(message, field), out);
      } else {
        for (int i = 0; i < reflection->FieldSize(message, field); i++) {
          render_message(reflection->GetRepeatedMessage(message, field, i), out);
        }
      }
      break;
    }

    default: {
      std::cout << "Not supported field " << field->name() << std::endl;
    }

    break;
  }
}

void render_message(const Message& message, xml::Printer& out) {
  const auto reflection = message.GetReflection();
  const auto descriptor = message.GetDescriptor();

  std::vector<const FieldDescriptor*> fields;
  reflection->ListFields(message, &fields);

  const std::string name = descriptor->name();
  out.println("<" + name + ">");
  out.push_indent();

  for (auto field : fields) {
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
  render_message(body, printer);
  return printer.str();
}

