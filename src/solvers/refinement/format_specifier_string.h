/*******************************************************************\

 Module: String solver

 Author: Diffblue Ltd.

\*******************************************************************/


#ifndef CPROVER_SOLVERS_REFINEMENT_FORMAT_SPECIFIER_STRING_H
#define CPROVER_SOLVERS_REFINEMENT_FORMAT_SPECIFIER_STRING_H

#include "string_constraint_generator.h"
#include "format_specifier.h"

/// A format specifier is of the form
/// %[arg_index$][flags][width][.precision]conversion
/// and is applied to an element of the argument list passed to String.format.
/// It describes how this value should be printed. For details see
/// https://docs.oracle.com/javase/7/docs/api/java/util/Formatter.html#syntax
class string_constraint_generatort::format_specifier_stringt final :
  public string_constraint_generatort::format_specifiert
{
public:
  // Constants describing the meaning of conversion characters in format
  // specifiers.
  static const char DECIMAL_INTEGER          ='d';
  static const char OCTAL_INTEGER            ='o';
  static const char HEXADECIMAL_INTEGER      ='x';
  static const char HEXADECIMAL_INTEGER_UPPER='X';
  static const char SCIENTIFIC               ='e';
  static const char SCIENTIFIC_UPPER         ='E';
  static const char GENERAL                  ='g';
  static const char GENERAL_UPPER            ='G';
  static const char DECIMAL_FLOAT            ='f';
  static const char HEXADECIMAL_FLOAT        ='a';
  static const char HEXADECIMAL_FLOAT_UPPER  ='A';
  static const char CHARACTER                ='c';
  static const char CHARACTER_UPPER          ='C';
  static const char DATE_TIME                ='t';
  static const char DATE_TIME_UPPER          ='T';
  static const char BOOLEAN                  ='b';
  static const char BOOLEAN_UPPER            ='B';
  static const char STRING                   ='s';
  static const char STRING_UPPER             ='S';
  static const char HASHCODE                 ='h';
  static const char HASHCODE_UPPER           ='H';
  static const char LINE_SEPARATOR           ='n';
  static const char PERCENT_SIGN             ='%';

  format_specifier_stringt() = default;

  format_specifier_stringt(
    int _arg_index,
    std::string _flag,
    int _width,
    int _precision,
    bool _dt,
    char _conversion);

  static std::vector<std::unique_ptr<format_elementt>>
  parse_format_string(std::string s);

  std::pair<exprt, array_string_exprt> add_axioms_for_format_element(
    string_constraint_generatort &gen,
    std::size_t &arg_count,
    const typet &index_type,
    const typet &char_type,
    const exprt::operandst &args) override;

private:
  static std::unique_ptr<format_specifier_stringt>
    format_specifier_of_match(std::smatch &m);

  std::pair<exprt, array_string_exprt> add_axioms_for_format_specifier(
    string_constraint_generatort &gen,
    const struct_exprt &arg,
    const typet &index_type,
    const typet &char_type) override;

  int arg_index = -1;
  std::string flag;
  int width = 0;
  int precision = 0;
  bool dt = false;
  char conversion = '\0';

};

#endif
