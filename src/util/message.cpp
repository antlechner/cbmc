/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/


#include "message.h"

#include "string2int.h"

void message_handlert::print(
  unsigned level,
  const std::string &message,
  int sequence_number,
  const source_locationt &location)
{
  std::string dest;

  const irep_idt &file=location.get_file();
  const irep_idt &line=location.get_line();
  const irep_idt &column=location.get_column();
  const irep_idt &function=location.get_function();

  if(!file.empty())
  {
    if(dest!="")
      dest+=' ';
    dest+="file "+id2string(file);
  }
  if(!line.empty())
  {
    if(dest!="")
      dest+=' ';
    dest+="line "+id2string(line);
  }
  if(!column.empty())
  {
    if(dest!="")
      dest+=' ';
    dest+="column "+id2string(column);
  }
  if(!function.empty())
  {
    if(dest!="")
      dest+=' ';
    dest+="function "+id2string(function);
  }

  if(dest!="")
    dest+=": ";
  dest+=message;

  print(level, dest);
}

void message_handlert::print(
  unsigned level,
  const std::string &message)
{
  if(level>=message_count.size())
    message_count.resize(level+1, 0);
  ++message_count[level];
}

messaget::~messaget()
{
}

/// Parse a (user-)provided string as a verbosity level and set it as the
/// verbosity of dest.
/// \param user_input  Input string; if empty, the default verbosity is used.
/// \param default_verbosity  Verbosity to use if no value is provided.
/// \param dest  message handler the verbosity of which is to be set.
/// \return Computed verbosity
unsigned messaget::eval_verbosity(
  const std::string &user_input,
  const message_levelt default_verbosity,
  message_handlert &dest)
{
  unsigned v = default_verbosity;

  if(!user_input.empty())
  {
    v = unsafe_string2unsigned(user_input);

    if(v > messaget::M_DEBUG)
    {
      dest.print(
        messaget::M_WARNING,
        "verbosity value " + user_input + " out of range, using debug-level (" +
          std::to_string(messaget::M_DEBUG) + ") verbosity",
        -1,
        source_locationt());

      v = messaget::M_DEBUG;
    }
  }

  dest.set_verbosity(v);

  return v;
}
