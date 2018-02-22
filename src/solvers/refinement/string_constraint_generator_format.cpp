/*******************************************************************\

Module: Generates string constraints for the Java format function

Author: Romain Brenguier

Date:   May 2017

\*******************************************************************/

/// \file
/// Generates string constraints for the Java format function
/// \todo We currently use the following return codes:
///     0: success
///     1: whenever a java.util.UnknownFormatConversionException would be thrown
///     2: whenever a java.util.MissingFormatArgumentException would be thrown
///   100: correct Java code which we do not yet support
///   These should be declared in a different file and given more meaningful
///   names.

#include <iomanip>
#include <string>
#include <regex>
#include <vector>

#include <util/make_unique.h>
#include <util/std_expr.h>
#include <util/unicode.h>

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


/// A format specifier is of the form
/// %[arg_index$][flags][width][.precision]conversion
/// and is applied to an element of the argument list passed to String.format.
/// It describes how this value should be printed. For details see
/// https://docs.oracle.com/javase/7/docs/api/java/util/Formatter.html#syntax
class string_constraint_generatort::format_specifiert :
  public string_constraint_generatort::format_elementt
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

  // Class members representing the structure of the format specifier
  int arg_index=-1;
  std::string flag;
  int width;
  int precision;
  bool dt=false;
  char conversion;

  format_specifiert(
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

  /// Helper function for parsing format strings.
  /// This follows the implementation in openJDK of the java.util.Formatter class:
  /// http://hg.openjdk.java.net/jdk7/jdk7/jdk/file/9b8c96f96a0f/src/share/classes/java/util/Formatter.java#l2660
  /// \param m: a match in a regular expression
  /// \return Format specifier represented by the matched string. The groups in
  ///   the match should represent: argument index, flag, width, precision, date
  ///   and conversion type.
  static std::unique_ptr<format_specifiert>
    format_specifier_of_match(std::smatch &m)
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
  std::pair<exprt, array_string_exprt> add_axioms_for_format_specifier(
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
  std::pair<exprt, array_string_exprt> add_axioms_for_format_element(
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

};

/// Class to represent fixed text in a format string. The contents of it are
/// unchanged by calls to java.lang.String.format.
class string_constraint_generatort::fixed_textt : public format_elementt
{
public:
  explicit fixed_textt(std::string _content): content(_content) { }

  fixed_textt(const fixed_textt &fs): content(fs.content) { }

  std::string get_content() const
  {
    return content;
  }

  /// Given a format element, add axioms ensuring the output corresponds to the
  /// output of String.format applied to that specifier with the given argument
  /// list.
  /// In the case of fixed text, we simply add an axiom for a constant string
  /// storing the value corresponding to the fixed text.
  /// \param gen: a `string_constraint_generatort` object (used for accessing
  ///   some of the member functions of this class)
  /// \param index_type: type for indices in strings
  /// \param char_type: type of characters in strings
  /// \return Pair consisting of return code and string expression representing
  /// the output of String.format. The return code is 0 on success, and the
  /// string expression represents the constant content of the fixed text.
  std::pair<exprt, array_string_exprt> add_axioms_for_format_element(
    string_constraint_generatort &gen,
    std::size_t &,
    const typet &index_type,
    const typet &char_type,
    const exprt::operandst &)
  {
    const array_string_exprt res = gen.fresh_string(index_type, char_type);
    const exprt return_code =
      gen.add_axioms_for_constant(res, get_content());
    return {return_code, res};
  }

private:
  std::string content;
};

typedef string_constraint_generatort::format_elementt format_elementt;
typedef string_constraint_generatort::fixed_textt fixed_textt;
typedef string_constraint_generatort::format_specifiert format_specifiert;

#if 0
// This code is deactivated as it is not used for now, but ultimately this
// should be used to throw an exception when the format string is not correct
/// Used to check first argument of `String.format` is correct.
/// \param s: a string
/// \return True if the argument is a correct format string. Any '%' found
///   between format specifier means the string is invalid.
static bool check_format_string(std::string s)
{
  std::string format_specifier=
      "%(\\d+\\$)?([-#+ 0,(\\<]*)?(\\d+)?(\\.\\d+)?([tT])?([a-zA-Z%])";
  std::regex regex(format_specifier);
  std::smatch match;

  while(std::regex_search(s, match, regex))
  {
    if(match.position()!= 0)
      for(const auto &c : match.str())
        if(c=='%')
          return false;
    s=match.suffix();
  }

  for(const auto &c : s)
    if(c=='%')
      return false;

  return true;
}
#endif

/// Parse the given string into format specifiers and text.
/// This follows the implementation in openJDK of the java.util.Formatter class:
/// http://hg.openjdk.java.net/jdk7/jdk7/jdk/file/9b8c96f96a0f/src/share/classes/java/util/Formatter.java#l2513
/// \param s: a string
/// \return A vector of format_elementt.
static std::vector<std::unique_ptr<format_elementt>>
parse_format_string(std::string s)
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

/// Parse `s` and add axioms ensuring the output corresponds to the output of
/// String.format.
/// \param res: string expression for the result of the format function
/// \param s: a format string
/// \param args: a vector of arguments
/// \return code, 0 on success
exprt string_constraint_generatort::add_axioms_for_format(
  const array_string_exprt &res,
  const std::string &s,
  const exprt::operandst &args)
{
  // Split s into its format elements
  const auto format_strings=parse_format_string(s);
  // We will format each format element according to the specification of
  // java.lang.String.format and store the results in a temporary vector
  // variable.
  std::vector<array_string_exprt> formatted_elements;
  // Number of format specifiers that we have processed that did not specify an
  // argument index
  std::size_t arg_count=0;
  const typet &char_type = res.content().type().subtype();
  const typet &index_type = res.length().type();

  for(const auto &format : format_strings)
  {
    const auto code_format_pair =
      format->add_axioms_for_format_element(
          *this, arg_count, index_type, char_type, args);
    if(code_format_pair.first != from_integer(0, get_return_code_type()))
    {
      // A nonzero exit code means an exception was thrown.
      /// \todo Add support for exceptions
      return code_format_pair.first;
    }
    const auto string_expr = code_format_pair.second;
    formatted_elements.push_back(string_expr);
  }

  if(formatted_elements.empty())
  {
    // Formatting an empty string results in an empty string
    add_axioms_for_constant(res, "");
    return from_integer(0, get_return_code_type());
  }

  auto it = formatted_elements.begin();
  array_string_exprt str = *(it++);
  exprt return_code = from_integer(0, get_return_code_type());
  for(; it != formatted_elements.end(); ++it)
  {
    const array_string_exprt fresh = fresh_string(index_type, char_type);
    // fresh is the result of concatenating str and it
    /// \todo `add_axioms_for_concat` currently always returns zero. In the
    ///   future we might want it to return other values depending on whether or
    ///   not the concatenation succeeded. For example, it will not succeed if
    ///   concatenating the two strings would exceed the value specified for
    ///   string-max-length.
    exprt concat_return_code = add_axioms_for_concat(fresh, str, *it);
    return_code =
      if_exprt(
          equal_exprt(return_code, from_integer(0, get_return_code_type())),
          concat_return_code,
          return_code);
    str = fresh;
  }
  // Copy
  add_axioms_for_substring(res, str, from_integer(0, index_type), str.length());
  return return_code;
}

/// Construct a string from a constant array.
/// \param arr: an array expression containing only constants
/// \param length: an unsigned value representing the length of the array
/// \return String of length `length` represented by the array assuming each
///   field in `arr` represents a character.
std::string
utf16_constant_array_to_java(const array_exprt &arr, std::size_t length)
{
  std::wstring out(length, '?');
  unsigned int c;
  for(std::size_t i=0; i<arr.operands().size() && i<length; i++)
  {
    PRECONDITION(arr.operands()[i].id()==ID_constant);
    bool conversion_failed=to_unsigned_integer(
      to_constant_expr(arr.operands()[i]), c);
    INVARIANT(!conversion_failed, "constant should be convertible to unsigned");
    out[i]=c;
  }
  return utf16_little_endian_to_java(out);
}

/// Formatted string using a format string and list of arguments
///
/// Add axioms to specify the Java String.format function.
/// \todo This is correct only if the argument at index 2 (ie the format string)
///   is of type constant_exprt.
/// \param f: A function application whose first two arguments store the result
///   of an application of java.lang.String.format. Its remaining arguments
///   correspond to the arguments passed to this call to String.format. That is,
///   the argument at index 2 is the format string, and arguments from index 3
///   onwards are elements of what is called the argument list in
///   https://docs.oracle.com/javase/7/docs/api/java/util/Formatter.html#syntax
///   Axioms are added to the result, i.e. the first two arguments of this
///   function application.
/// \return code, 0 on success
exprt string_constraint_generatort::add_axioms_for_format(
  const function_application_exprt &f)
{
  // Result and format string have to be present, argument list may be empty
  PRECONDITION(f.arguments().size() >= 3);
  const array_string_exprt res =
    char_array_of_pointer(f.arguments()[1], f.arguments()[0]);
  const array_string_exprt format_string = get_string_expr(f.arguments()[2]);
  const auto length = numeric_cast<unsigned>(format_string.length());

  if(length && format_string.content().id()==ID_array)
  { // format_string is constant
    std::string s=utf16_constant_array_to_java(
      to_array_expr(format_string.content()), *length);
    // List of arguments after s
    std::vector<exprt> args(f.arguments().begin() + 3, f.arguments().end());
    return add_axioms_for_format(res, s, args);
  }
  else
  { // format_string is nondeterministic
    message.warning()
      << "ignoring format function with non constant first argument"
      << message.eom;
    return from_integer(100, f.type());
  }
}
