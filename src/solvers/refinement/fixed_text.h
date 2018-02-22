/*******************************************************************\

 Module: String solver

 Author: Diffblue Ltd.

\*******************************************************************/


#ifndef CPROVER_SOLVERS_REFINEMENT_FIXED_TEXT_H
#define CPROVER_SOLVERS_REFINEMENT_FIXED_TEXT_H

#include "string_constraint_generator.h"
#include "format_element.h"

/// Class to represent fixed text in a format string. The contents of it are
/// unchanged by calls to java.lang.String.format.
class string_constraint_generatort::fixed_textt final : public format_elementt
{
public:
  fixed_textt(std::string _content);

  std::string get_content() const;

  std::pair<exprt, array_string_exprt> add_axioms_for_format_element(
    string_constraint_generatort &gen,
    std::size_t &,
    const typet &index_type,
    const typet &char_type,
    const exprt::operandst &);

private:
  std::string content;
};

#endif
