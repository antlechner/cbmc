/*******************************************************************\

 Module: String solver

 Author: Diffblue Ltd.

\*******************************************************************/

/// \file
/// Collect methods needed to be loaded using the lazy method


#ifndef CPROVER_SOLVERS_REFINEMENT_FORMAT_SPECIFIER_DATE_H
#define CPROVER_SOLVERS_REFINEMENT_FORMAT_SPECIFIER_DATE_H

#include "string_constraint_generator.h"
#include "format_specifier.h"


/// Pattern letter describes how a value should be printed.
/// In patterns parsed by java.text.SimpleDateFormat.format, unquoted letters
/// from 'A' to 'Z' and from 'a' to 'z' are interpreted as pattern letters.
/// Single quotes around text are used to avoid interpretation, and "''"
/// represents a single quote. All other characters are not interpreted, i.e.
/// the format method leaves them unchanged.
class string_constraint_generatort::format_specifier_datet final :
  public string_constraint_generatort::format_specifiert
{
public:
  /// Constants describing the meaning of pattern letters. Some pattern letters
  /// give different results when they occur repeatedly in the same pattern. For
  /// "number" letters, the number of occurrences of the letter specifies the
  /// minimum number of digits in the output. If necessary, numbers are padded
  /// with zeros to the left to meet this requirement.
  /// AD or BC (G+)
  static const char ERA_DESIGNATOR          = 'G';
  /// Assumes that the Gregorian calendar is being used. For example,  for the
  /// year 2018: yy is formatted as 18. y, yyy and yyyy are formatted as 2018.
  /// Any more occurrences use zero-padding, i.e. n+4 occurrences of y are
  /// formatted as 0{n}2018.
  static const char YEAR                    = 'y';
  /// Formatted in the same way as YEAR.
  static const char WEEK_YEAR               = 'Y';
  /// For example, for the month of January: M is formatted as 1. MM is
  /// formatted as 01. MMM is formatted as Jan. M{4,} is formatted as January.
  static const char MONTH                   = 'M';
  /// "number"
  static const char WEEK_IN_YEAR            = 'w';
  /// "number"
  static const char WEEK_IN_MONTH           = 'W';
  /// "number"
  static const char DAY_IN_YEAR             = 'D';
  /// "number"
  static const char DAY_IN_MONTH            = 'd';
  /// "number"
  static const char DAY_OF_WEEK_IN_MONTH    = 'F';
  /// For example, for Monday: Mon (E{1,3}), Monday (E{4,})
  static const char DAY_NAME_IN_WEEK        = 'E';
  /// "number"
  static const char DAY_NUMBER_OF_WEEK      = 'u';
  /// AM or PM (a+)
  static const char AM_PM_MARKER            = 'a';
  /// "number"
  static const char HOUR_IN_DAY_FROM_ZERO   = 'H';
  /// "number"
  static const char HOUR_IN_DAY_FROM_ONE    = 'k';
  /// "number"
  static const char HOUR_IN_AM_PM_FROM_ZERO = 'K';
  /// "number"
  static const char HOUR_IN_AM_PM_FROM_ONE  = 'h';
  /// "number"
  static const char MINUTE_IN_HOUR          = 'm';
  /// "number"
  static const char SECOND_IN_MINUTE        = 's';
  /// "number"
  static const char MILLISECOND             = 'S';
  static const char TIME_ZONE_GENERAL       = 'z';
  static const char TIME_ZONE_RFC           = 'Z';
  static const char TIME_ZONE_ISO           = 'X';

  format_specifier_datet() = default;

  format_specifier_datet(
    char letter,
    unsigned _length);

  static std::vector<std::unique_ptr<format_elementt>>
  parse_format_string(std::string pattern);

  std::pair<exprt, array_string_exprt> add_axioms_for_format_element(
    string_constraint_generatort &gen,
    std::size_t &arg_count,
    const typet &index_type,
    const typet &char_type,
    const exprt::operandst &args) override;

private:
  std::pair<exprt, array_string_exprt> add_axioms_for_format_specifier(
    string_constraint_generatort &gen,
    const struct_exprt &arg,
    const typet &index_type,
    const typet &char_type) override;

  char pattern_letter = '\0';
  unsigned length = 0;

};

#endif
