//
// Created by pvl on 24.11.24.
//

#include "printer.h"

void xml::Printer::push_indent() { tabs_amount++; }
void xml::Printer::pop_indent() { tabs_amount--; }

void xml::Printer::println(std::string text) {
  if (at_line_start) {
    print_indent();
    at_line_start = false;
  }

  builder << text << std::endl;
  at_line_start = true;
}

void xml::Printer::print(std::string text) {
  if (at_line_start) {
    print_indent();
    at_line_start = false;
  }

  builder << text;
}

std::string xml::Printer::str() const { return builder.str(); }
void xml::Printer::print_indent() { builder << indent(); }
std::string xml::Printer::indent() const {
  std::ostringstream res;

  for (int i = 0; i < tabs_amount; i++) {
    res << tab;
  }

  return res.str();
}

