/*******************************************************************\

 Module: String solver

 Author: Diffblue Ltd.

\*******************************************************************/


#ifndef CPROVER_SOLVERS_REFINEMENT_FORMAT_SPECIFIER_H
#define CPROVER_SOLVERS_REFINEMENT_FORMAT_SPECIFIER_H

#include <string>
#include <regex>

#include "string_constraint_generator.h"
#include "format_element.h"

/// A format specifier is of the form
/// %[arg_index$][flags][width][.precision]conversion
/// and is applied to an element of the argument list passed to String.format.
/// It describes how this value should be printed. For details see
/// https://docs.oracle.com/javase/7/docs/api/java/util/Formatter.html#syntax
class string_constraint_generatort::format_specifiert :
  public string_constraint_generatort::format_elementt
{
protected:
  format_specifiert() = default;

public:
  static exprt get_component_in_struct(
    const struct_exprt &expr, irep_idt component_name)
  {
    const struct_typet &type=to_struct_type(expr.type());
    std::size_t number=type.component_number(component_name);
    return expr.operands()[number];
  }

private:
  virtual std::pair<exprt, array_string_exprt> add_axioms_for_format_specifier(
    string_constraint_generatort &gen,
    const struct_exprt &arg,
    const typet &index_type,
    const typet &char_type) = 0;

};

#endif
