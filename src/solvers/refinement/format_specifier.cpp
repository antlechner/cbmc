/*******************************************************************\

 Module: String solver

 Author: Diffblue Ltd.

\*******************************************************************/


#include <iomanip>
#include <string>
#include <regex>
#include <vector>

#include "format_specifier.h"
#include "format_element.h"
#include "format_specifier.h"
#include "fixed_text.h"

#include <util/make_unique.h>

typedef string_constraint_generatort::format_elementt format_elementt;
typedef string_constraint_generatort::format_specifiert format_specifiert;
typedef string_constraint_generatort::fixed_textt fixed_textt;

format_specifiert::format_specifiert(
  int _arg_index,
  std::string _flag,
  int _width,
  int _precision,
  bool _dt,
  char _conversion):
    arg_index(_arg_index),
    flag(_flag),
    width(_width),
    precision(_precision),
    dt(_dt),
    conversion(_conversion)
{ }

/// Parse the given string into format specifiers and text.
/// This follows the implementation in openJDK of the java.util.Formatter class:
/// http://hg.openjdk.java.net/jdk7/jdk7/jdk/file/9b8c96f96a0f/src/share/classes/java/util/Formatter.java#l2513
/// \param s: a string
/// \return A vector of format_elementt.
std::vector<std::unique_ptr<format_elementt>>
format_specifiert::parse_format_string(std::string s)
{
  std::string format_specifier=
    "%(\\d+\\$)?([-#+ 0,(\\<]*)?(\\d+)?(\\.\\d+)?([tT])?([a-zA-Z%])";
  std::regex regex(format_specifier);
  std::vector<std::unique_ptr<format_elementt>> elements;
  std::smatch match;

  while(std::regex_search(s, match, regex))
  {
    if(match.position()!=0)
    {
      std::string pre_match=s.substr(0, match.position());
      elements.push_back(util_make_unique<fixed_textt>(pre_match));
    }

    elements.push_back(format_specifiert::format_specifier_of_match(match));
    s=match.suffix();
  }

  elements.push_back(util_make_unique<fixed_textt>(s));
  return elements;
}

/// Helper function for parsing format strings.
/// This follows the implementation in openJDK of the java.util.Formatter class:
/// http://hg.openjdk.java.net/jdk7/jdk7/jdk/file/9b8c96f96a0f/src/share/classes/java/util/Formatter.java#l2660
/// \param m: a match in a regular expression
/// \return Format specifier represented by the matched string. The groups in
///   the match should represent: argument index, flag, width, precision, date
///   and conversion type.
std::unique_ptr<format_specifiert>
format_specifiert::format_specifier_of_match(std::smatch &m)
{
  int arg_index=m[1].str().empty()?-1:std::stoi(m[1].str());
  std::string flag=m[2].str().empty()?"":m[2].str();
  int width=m[3].str().empty()?-1:std::stoi(m[3].str());
  int precision=m[4].str().empty()?-1:std::stoi(m[4].str());
  std::string tT=m[5].str();

  bool dt=(tT!="");
  if(tT=="T")
    flag.push_back(
      format_specifiert::DATE_TIME_UPPER);

  INVARIANT(
    m[6].str().length()==1, "format conversion should be one character");
  char conversion=m[6].str()[0];

  return util_make_unique<format_specifiert>(
    arg_index, flag, width, precision, dt, conversion);
}

/// Helper for add_axioms_for_format_specifier
/// \param expr: a structured expression
/// \param component_name: name of the desired component
/// \return Expression in the component of `expr` named `component_name`.
static exprt get_component_in_struct(
  const struct_exprt &expr, irep_idt component_name)
{
  const struct_typet &type=to_struct_type(expr.type());
  std::size_t number=type.component_number(component_name);
  return expr.operands()[number];
}

/// Given a format specifier, add axioms ensuring the output corresponds to the
/// output of String.format applied to that specifier. Assumes the argument is a
/// structured expression which contains the fields: string expr, int, float,
/// char, boolean, hashcode, date_time. The correct component will be fetched
/// depending on the format specifier. We do not yet support %o, %g, %G, %a, %A,
/// %t and %T format specifiers.
/// \param fs: a format specifier
/// \param arg: a struct containing the possible value of the argument to format
/// \param index_type: type for indexes in strings
/// \param char_type: type of characters
/// \return Pair consisting of return code and string expression representing
/// the output of String.format. The return code is 0 on success, 1 for invalid
/// format specifiers and 100 for format specifiers that we do not yet support.
std::pair<exprt, array_string_exprt>
format_specifiert::add_axioms_for_format_specifier(
  string_constraint_generatort &gen,
  const struct_exprt &arg,
  const typet &index_type,
  const typet &char_type)
{
  const array_string_exprt res = gen.fresh_string(index_type, char_type);
  exprt return_code;
  switch(conversion)
  {
  case DECIMAL_INTEGER:
    return_code =
      gen.add_axioms_from_int(res, get_component_in_struct(arg, ID_int));
    return {return_code, res};
  case HEXADECIMAL_INTEGER:
    return_code =
      gen.add_axioms_from_int_hex(res, get_component_in_struct(arg, ID_int));
    return {return_code, res};
  case SCIENTIFIC:
    gen.add_axioms_from_float_scientific_notation(
      res, get_component_in_struct(arg, ID_float));
    return {return_code, res};
  case DECIMAL_FLOAT:
    gen.add_axioms_for_string_of_float(res, get_component_in_struct(arg, ID_float));
    return {return_code, res};
  case CHARACTER:
    return_code =
      gen.add_axioms_from_char(res, get_component_in_struct(arg, ID_char));
    return {return_code, res};
  case BOOLEAN:
    return_code =
      gen.add_axioms_from_bool(res, get_component_in_struct(arg, ID_boolean));
    return {return_code, res};
  case STRING:
  {
    return_code = from_integer(0, gen.get_return_code_type());
    const array_string_exprt string_res = gen.get_string_expr(
        get_component_in_struct(arg, "string_expr"));
    return {return_code, string_res};
  }
  case HASHCODE:
    return_code =
      gen.add_axioms_from_int(res, get_component_in_struct(arg, "hashcode"));
    return {return_code, res};
  case LINE_SEPARATOR:
    // TODO: the constant should depend on the system: System.lineSeparator()
    return_code = gen.add_axioms_for_constant(res, "\n");
    return {return_code, res};
  case PERCENT_SIGN:
    return_code = gen.add_axioms_for_constant(res, "%");
    return {return_code, res};
  case SCIENTIFIC_UPPER:
  case GENERAL_UPPER:
  case HEXADECIMAL_FLOAT_UPPER:
  case CHARACTER_UPPER:
  case DATE_TIME_UPPER:
  case BOOLEAN_UPPER:
  case STRING_UPPER:
  case HASHCODE_UPPER:
  {
    format_specifiert fs_lower=*this;
    fs_lower.conversion=tolower(conversion);
    const auto lower_case_code_format_pair =
      fs_lower.add_axioms_for_format_specifier(
          gen, arg, index_type, char_type);
    gen.add_axioms_for_to_upper_case(res, lower_case_code_format_pair.second);
    return {lower_case_code_format_pair.first, res};
  }
  case OCTAL_INTEGER:
  /// \todo Conversion of octal is not implemented.
  case GENERAL:
  /// \todo Conversion for format specifier general is not implemented.
  case HEXADECIMAL_FLOAT:
  /// \todo Conversion of hexadecimal float is not implemented.
  case DATE_TIME:
    /// \todo Conversion of date-time is not implemented.
    ///   For all these unimplemented cases we return a non-deterministic
    ///   string.
    gen.message.warning() << "unimplemented format specifier: " << conversion
                        << gen.message.eom;
    return_code = from_integer(100, gen.get_return_code_type());
    return {return_code, res};
  default:
    /// \todo Throwing exceptions for invalid format specifiers is not yet
    ///   implemented. In Java, a java.util.UnknownFormatConversionException is
    ///   thrown in this case. Instead, we currently just return a
    ///   nondeterministic string.
    gen.message.error() << "invalid format specifier: " << conversion
      << ". format specifier must belong to [bBhHsScCdoxXeEfgGaAtT%n]"
      << gen.message.eom;
    return_code = from_integer(1, gen.get_return_code_type());
    return {return_code, res};
  }

}

/// Given a format element, add axioms ensuring the output corresponds to the
/// output of String.format applied to that specifier with the given argument
/// list.
/// In the case of format specifiers, we first need to check for specifiers
/// that correspond to exception cases in the Java program. Then the
/// formatting is done by a call to add_axioms_for_format_specifier.
/// \param gen: a `string_constraint_generatort` object (used for accessing
///   some of the member functions of this class)
/// \param arg_count: the number of arguments in the argument list that have
///   already been processed using format specifiers without argument index
/// \param index_type: type for indices in strings
/// \param char_type: type of characters in strings
/// \param args: argument list passed to String.format method
/// \return Pair consisting of return code and string expression representing
/// the output of String.format. The return code is 0 on success, 1 for
/// invalid conversion characters, 2 for an insufficient number of arguments,
/// and 100 for format specifiers that we do not yet support.
std::pair<exprt, array_string_exprt>
format_specifiert::add_axioms_for_format_element(
  string_constraint_generatort &gen,
  std::size_t &arg_count,
  const typet &index_type,
  const typet &char_type,
  const exprt::operandst &args)
{
  struct_exprt arg;
  // Per cent sign (%) and line separator (n) do not take any arguments.
  if(conversion!=PERCENT_SIGN &&
     conversion!=LINE_SEPARATOR)
  {
    // -1 means no value specified. 0 is a special case that is treated by
    // Java as if no value was specified.
    if(arg_index == -1 || arg_index == 0)
    {
      /// \todo In java, a java.util.MissingFormatArgumentException is
      ///   thrown when the number of arguments is less than the number of
      ///   format specifiers without argument index. We do not yet support
      ///   throwing the exception in this case and instead do not put any
      ///   additional constraints on the string.
      if(arg_count >= args.size())
      {
        gen.message.warning() << "number of arguments must be at least number "
                          << "of format specifiers without argument index"
                          << gen.message.eom;
        return {from_integer(2, gen.get_return_code_type()),
                gen.fresh_string(index_type, char_type)};
      }
      arg=to_struct_expr(args[arg_count++]);
    }
    else
    {
      /// \todo In Java, a java.util.UnknownFormatConversionException is
      ///   thrown when the argument index in the format specifier is
      ///   negative. We do not yet support throwing the exception in this
      ///   case and instead do not put any additional constraints on the
      ///   string.
      if(arg_index < 0)
      {
        gen.message.warning() << "index in format should be nonnegative"
                          << gen.message.eom;
        return {from_integer(1, gen.get_return_code_type()),
                gen.fresh_string(index_type, char_type)};
      }
      /// \todo In Java, a java.util.MissingFormatArgumentException is
      ///   thrown when the argument index in the format specifier is bigger
      ///   than the number of arguments. We do not yet support throwing the
      ///   exception in this case and instead do not put any additional
      ///   constraints on the string.
      if(static_cast<std::size_t>(arg_index) > args.size())
      {
        gen.message.warning() << "argument index in format specifier cannot be "
                          << "bigger than number of arguments"
                          << gen.message.eom;
        return {from_integer(2, gen.get_return_code_type()),
                gen.fresh_string(index_type, char_type)};
      }
      // first argument `args[0]` corresponds to argument index 1
      arg=to_struct_expr(args[arg_index-1]);
    }
  }
  return add_axioms_for_format_specifier(gen, arg, index_type, char_type);
}
