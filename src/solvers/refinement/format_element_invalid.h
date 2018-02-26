/*******************************************************************\

 Module: String solver

 Author: Diffblue Ltd.

\*******************************************************************/


#ifndef CPROVER_SOLVERS_REFINEMENT_FORMAT_ELEMENT_INVALID_H
#define CPROVER_SOLVERS_REFINEMENT_FORMAT_ELEMENT_INVALID_H

#include "string_constraint_generator.h"
#include "format_element.h"

/// Class to represent an invalid format element.
class string_constraint_generatort::format_element_invalidt final : public format_elementt
{
public:
  format_element_invalidt(unsigned _return_code);

  unsigned get_code() const;

  std::pair<exprt, array_string_exprt> add_axioms_for_format_element(
    string_constraint_generatort &gen,
    std::size_t &,
    const typet &index_type,
    const typet &char_type,
    const exprt::operandst &);

private:
  unsigned return_code;
};

#endif
