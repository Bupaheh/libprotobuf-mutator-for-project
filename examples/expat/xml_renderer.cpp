//
// Created by pvl on 24.11.24.
//

#include "xml_renderer.h"

#include "printer.h"

using namespace google::protobuf;

namespace {

void render_message(const Message& message, xml::Printer& out) {
  auto reflection = message.GetReflection();
  auto descriptor = message.GetDescriptor();

  std::vector<const FieldDescriptor*> fields;
  reflection->ListFields(message, &fields);

  std::string name = descriptor->name();
  out.println("<" + name + ">");

  out.push_indent();

  for (auto field : fields) {
    switch (field->type()) {
      case internal::FieldDescriptorLite::TYPE_STRING:
        out.println(reflection->GetString(message, field));
      break;

      case internal::FieldDescriptorLite::TYPE_ENUM: {
        const EnumValueDescriptor* value = reflection->GetEnum(message, field);
        out.println(value->name());
        break;
      }

      case internal::FieldDescriptorLite::TYPE_MESSAGE: {
        if (field->is_repeated()) {
          for (int i = 0; i < reflection->FieldSize(message, field); i++) {
            render_message(reflection->GetRepeatedMessage(message, field, i), out);
          }
        } else {
          render_message(reflection->GetMessage(message, field), out);
        }
        break;
      }

      default:
        std::cout << "Not supported field " << field->name() << std::endl;
      break;
    }
  }

  out.pop_indent();
  out.println("</" + name + ">");
  out.println();
}

}



std::string render(const Message& message) {
  auto reflection = message.GetReflection();
  auto descriptor = message.GetDescriptor();

  assert(descriptor->field_count() == 1);
  const auto field = descriptor->field(0);

  const Message& body = reflection->GetMessage(message, field);

  xml::Printer printer;
  render_message(body, printer);
  return printer.str();
}

