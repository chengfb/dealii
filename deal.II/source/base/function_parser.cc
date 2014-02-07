// ---------------------------------------------------------------------
// $Id$
//
// Copyright (C) 2005 - 2013 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE at
// the top level of the deal.II distribution.
//
// ---------------------------------------------------------------------


#include <deal.II/base/function_parser.h>
#include <deal.II/base/utilities.h>
#include <deal.II/lac/vector.h>


#ifdef DEAL_II_WITH_MUPARSER
#include <deal.II/base/std_cxx1x/bind.h>
#include <muParser.h>

#elif defined(DEAL_II_WITH_FUNCTIONPARSER)
#include <fparser.hh>
namespace fparser
{
  class FunctionParser: public ::FunctionParser
  {};
}
#else

namespace fparser
{
  class FunctionParser
  {};
}

#endif

DEAL_II_NAMESPACE_OPEN



template <int dim>
FunctionParser<dim>::FunctionParser(const unsigned int n_components,
                                    const double       initial_time)
  :
  Function<dim>(n_components, initial_time)
#ifdef DEAL_II_WITH_MUPARSER
#else
  ,fp (0)
#endif
{
#ifdef DEAL_II_WITH_MUPARSER
#else
  fp = new fparser::FunctionParser[n_components];
#endif
}



template <int dim>
FunctionParser<dim>::~FunctionParser()
{
#ifdef DEAL_II_WITH_MUPARSER
#else
  delete[] fp;
#endif
}


#ifdef DEAL_II_WITH_FUNCTIONPARSER
template <int dim>
void FunctionParser<dim>::initialize (const std::string                   &variables,
                                      const std::vector<std::string>      &expressions,
                                      const std::map<std::string, double> &constants,
                                      const bool time_dependent,
                                      const bool use_degrees)
{
  initialize (variables,
              expressions,
              constants,
              std::map< std::string, double >(),
              time_dependent,
              use_degrees);
}

template <int dim>
void FunctionParser<dim>::initialize (const std::string              &vars,
				      const std::vector<std::string> &expressions,
				      const std::map<std::string, double> &constants,
				      const bool time_dependent)
  {
    initialize(vars, expressions, constants, time_dependent, false);
  }



#ifdef DEAL_II_WITH_MUPARSER
template <int dim>
void FunctionParser<dim>:: init_muparser() const
{
  if (fp.get().size()>0)
    return;
  
  fp.get().resize(this->n_components);
  vars.get().resize(var_names.size());
  for (unsigned int i=0; i<this->n_components; ++i)
    {
      std::map< std::string, double >::const_iterator
	constant = constants.begin(),
	endc  = constants.end();

      for (; constant != endc; ++constant)
        {
 	  fp.get()[i].DefineConst(constant->first.c_str(), constant->second);
	}

      for (unsigned int iv=0;iv<var_names.size();++iv)
	fp.get()[i].DefineVar(var_names[iv].c_str(), &vars.get()[iv]);

      try
	{
	  fp.get()[i].SetExpr(expressions[i]);
	}
      catch (mu::ParserError &e)
	{
	  cerr << "Message:  " << e.GetMsg() << "\n";
	  cerr << "Formula:  " << e.GetExpr() << "\n";
	  cerr << "Token:    " << e.GetToken() << "\n";
	  cerr << "Position: " << e.GetPos() << "\n";
	  cerr << "Errc:     " << e.GetCode() << std::endl;	  
	  AssertThrow(false, ExcParseError(e.GetCode(), e.GetMsg().c_str()));
	}      
    }
}
#endif

template <int dim>
void FunctionParser<dim>::initialize (const std::string   &variables,
                                      const std::vector<std::string>      &expressions,
                                      const std::map<std::string, double> &constants,
                                      const std::map<std::string, double> &units,
                                      const bool time_dependent,
                                      const bool use_degrees)
{

#ifdef DEAL_II_WITH_MUPARSER
  this->fp.clear(); // this will reset all thread-local objects
  
  this->constants = constants;
  this->var_names = Utilities::split_string_list(variables, ',');
  this->expressions = expressions;
  AssertThrow(((time_dependent)?dim+1:dim) == var_names.size(),
	      ExcMessage("wrong number of variables"));
  AssertThrow(!use_degrees, ExcNotImplemented());
#endif  

  // We check that the number of
  // components of this function
  // matches the number of components
  // passed in as a vector of
  // strings.
  AssertThrow(this->n_components == expressions.size(),
              ExcInvalidExpressionSize(this->n_components,
                                       expressions.size()) );

  for (unsigned int i=0; i<this->n_components; ++i)
    {

      // Add the various units to
      // the parser.
      std::map< std::string, double >::const_iterator
      unit = units.begin(),
      endu = units.end();

      for (; unit != endu; ++unit)
        {
#ifdef DEAL_II_WITH_MUPARSER
	  // TODO: 
	  //	  fp[i].DefinePostfixOprt(unit->first.c_str(),
//				  std_cxx1x::bind(&constant_eval, std_cxx1x:_1, unit->second));
	  
	  AssertThrow(false, ExcNotImplemented());
#else	  
          const bool success = fp[i].AddUnit(unit->first, unit->second);
          AssertThrow (success,
                       ExcMessage("Invalid Unit Name [error adding a unit]"));
#endif	  
        }


      // Add the various constants to
      // the parser.
      std::map< std::string, double >::const_iterator
      constant = constants.begin(),
      endc  = constants.end();


      for (; constant != endc; ++constant)
        {
#ifdef DEAL_II_WITH_MUPARSER
#else
          const bool success = fp[i].AddConstant(constant->first, constant->second);
          AssertThrow (success, ExcMessage("Invalid Constant Name"));
#endif
        }

#ifdef DEAL_II_WITH_MUPARSER
#else
      const int ret_value = fp[i].Parse(expressions[i],
                                        variables,
                                        use_degrees);
      AssertThrow (ret_value == -1,
                   ExcParseError(ret_value, fp[i].ErrorMsg()));
#endif

      // The fact that the parser did
      // not throw an error does not
      // mean that everything went
      // ok... we can still have
      // problems with the number of
      // variables...
    }
  // Now we define how many variables
  // we expect to read in.  We
  // distinguish between two cases:
  // Time dependent problems, and not
  // time dependent problems. In the
  // first case the number of
  // variables is given by the
  // dimension plus one. In the other
  // case, the number of variables is
  // equal to the dimension. Once we
  // parsed the variables string, if
  // none of this is the case, then
  // an exception is thrown.
  if (time_dependent)
    n_vars = dim+1;
  else
    n_vars = dim;

  /*
                                     // Let's check if the number of
                                     // variables is correct...
    AssertThrow (n_vars == fp[0].NVars(),
  !                              ~~~~~~~
  !                              not available anymore in fparser-4.5, maier 2012
                 ExcDimensionMismatch(n_vars,fp[0].NVars()));
  */

#ifdef DEAL_II_WITH_MUPARSER
  init_muparser();
#endif  
  
  // Now set the initialization bit.
  initialized = true;
}

template <int dim>
void FunctionParser<dim>::initialize (const std::string &vars,
                   const std::string &expression,
                   const std::map<std::string, double> &constants,
                   const bool time_dependent)
{
  initialize(vars, expression, constants, time_dependent, false);
}


template <int dim>
void FunctionParser<dim>::initialize (const std::string &variables,
                                      const std::string &expression,
                                      const std::map<std::string, double> &constants,
                                      const bool time_dependent,
                                      const bool use_degrees)
{
  // initialize with the things
  // we got.
  initialize (variables,
              Utilities::split_string_list (expression, ';'),
              constants,
              time_dependent,
              use_degrees);
}



template <int dim>
void FunctionParser<dim>::initialize (const std::string &variables,
                                      const std::string &expression,
                                      const std::map<std::string, double> &constants,
                                      const std::map<std::string, double> &units,
                                      const bool time_dependent,
                                      const bool use_degrees)
{
  // initialize with the things
  // we got.
  initialize (variables,
              Utilities::split_string_list (expression, ';'),
              constants,
              units,
              time_dependent,
              use_degrees);
}



template <int dim>
double FunctionParser<dim>::value (const Point<dim>  &p,
                                   const unsigned int component) const
{
  Assert (initialized==true, ExcNotInitialized());
  Assert (component < this->n_components,
          ExcIndexRange(component, 0, this->n_components));

#ifdef DEAL_II_WITH_MUPARSER

  init_muparser();

  for (unsigned int i=0; i<dim; ++i)
    vars.get()[i] = p(i);
  if (dim != n_vars)
    vars.get()[dim] = this->get_time();

  try
    {
      return fp.get()[component].Eval();
    }
  catch (mu::ParserError &e)
    {
      cerr << "Message:  " << e.GetMsg() << "\n";
      cerr << "Formula:  " << e.GetExpr() << "\n";
      cerr << "Token:    " << e.GetToken() << "\n";
      cerr << "Position: " << e.GetPos() << "\n";
      cerr << "Errc:     " << e.GetCode() << std::endl;	  
      AssertThrow(false, ExcParseError(e.GetCode(), e.GetMsg().c_str()));
      return 0.0;
    }
#else
  // Statically allocate dim+1
  // double variables.
  double vars[dim+1];
  for (unsigned int i=0; i<dim; ++i)
    vars[i] = p(i);
  if (dim != n_vars)
    vars[dim] = this->get_time();

  double my_value = fp[component].Eval((double *)vars);

  AssertThrow (fp[component].EvalError() == 0,
               ExcMessage(fp[component].ErrorMsg()));
  return my_value;
#endif  
}



template <int dim>
void FunctionParser<dim>::vector_value (const Point<dim> &p,
                                        Vector<double>   &values) const
{
  Assert (initialized==true, ExcNotInitialized());
  Assert (values.size() == this->n_components,
          ExcDimensionMismatch (values.size(), this->n_components));


#ifdef DEAL_II_WITH_MUPARSER
  init_muparser();

  for (unsigned int i=0; i<dim; ++i)
    vars.get()[i] = p(i);
  if (dim != n_vars)
    vars.get()[dim] = this->get_time();
  
  for (unsigned int component = 0; component < this->n_components;
       ++component)
    values(component) = fp.get()[component].Eval();
  
#else
  
  // Statically allocates dim+1
  // double variables.
  double vars[dim+1];

  for (unsigned int i=0; i<dim; ++i)
    vars[i] = p(i);
  if (dim != n_vars)
    vars[dim] = this->get_time();

  for (unsigned int component = 0; component < this->n_components;
       ++component)
    {
      values(component) = fp[component].Eval((double *)vars);
      AssertThrow(fp[component].EvalError() == 0,
                  ExcMessage(fp[component].ErrorMsg()));
    }
#endif  
}

#else

template <int dim>
void
FunctionParser<dim>::initialize(const std::string &,
                                const std::vector<std::string> &,
                                const std::map<std::string, double> &,
                                const bool,
                                const bool)
{
  Assert(false, ExcNeedsFunctionparser());
}


template <int dim>
void
FunctionParser<dim>::initialize(const std::string &,
                                const std::vector<std::string> &,
                                const std::map<std::string, double> &,
                                const std::map<std::string, double> &,
                                const bool,
                                const bool)
{
  Assert(false, ExcNeedsFunctionparser());
}


template <int dim>
void
FunctionParser<dim>::initialize(const std::string &,
                                const std::string &,
                                const std::map<std::string, double> &,
                                const bool,
                                const bool)
{
  Assert(false, ExcNeedsFunctionparser());
}


template <int dim>
void
FunctionParser<dim>::initialize(const std::string &,
                                const std::string &,
                                const std::map<std::string, double> &,
                                const std::map<std::string, double> &,
                                const bool,
                                const bool)
{
  Assert(false, ExcNeedsFunctionparser());
}


template <int dim>
double FunctionParser<dim>::value (
  const Point<dim> &, unsigned int) const
{
  Assert(false, ExcNeedsFunctionparser());
  return 0.;
}


template <int dim>
void FunctionParser<dim>::vector_value (
  const Point<dim> &, Vector<double> &) const
{
  Assert(false, ExcNeedsFunctionparser());
}


#endif

// Explicit Instantiations.

template class FunctionParser<1>;
template class FunctionParser<2>;
template class FunctionParser<3>;

DEAL_II_NAMESPACE_CLOSE
