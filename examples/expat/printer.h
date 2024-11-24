//
// Created by pvl on 24.11.24.
//

#ifndef PRINTER_H
#define PRINTER_H
#include <sstream>
#include <string>

namespace xml {

class Printer {

public:
  void push_indent();
  void pop_indent();

  void println(std::string text = "");
  void print(std::string text);

  std::string str() const;

private:
  void print_indent();
  std::string indent() const;

  std::ostringstream builder;
  bool at_line_start = true;

  int tabs_amount = 0;
  std::string tab = "  ";

};

}

#endif //PRINTER_H
