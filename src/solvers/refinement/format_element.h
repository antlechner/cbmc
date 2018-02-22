/*******************************************************************\

 Module: String solver

 Author: Diffblue Ltd.

\*******************************************************************/


#ifndef CPROVER_SOLVERS_REFINEMENT_FORMAT_ELEMENT_H
#define CPROVER_SOLVERS_REFINEMENT_FORMAT_ELEMENT_H

#include "string_constraint_generator.h"

/// A format string consists of format elements. Each type of format element has
/// its own rules for formatting.
class string_constraint_generatort::format_elementt
{
protected:
  format_elementt() = default;

public:
  virtual ~format_elementt() = default;

  virtual std::pair<exprt, array_string_exprt> add_axioms_for_format_element(
    string_constraint_generatort &gen,
    std::size_t &arg_count,
    const typet &index_type,
    const typet &char_type,
    const exprt::operandst &args) = 0;
};

#endif
