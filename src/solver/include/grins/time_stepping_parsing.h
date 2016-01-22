//-----------------------------------------------------------------------bl-
//--------------------------------------------------------------------------
//
// GRINS - General Reacting Incompressible Navier-Stokes
//
// Copyright (C) 2014-2015 Paul T. Bauman, Roy H. Stogner
// Copyright (C) 2010-2013 The PECOS Development Team
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the Version 2.1 GNU Lesser General
// Public License as published by the Free Software Foundation.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc. 51 Franklin Street, Fifth Floor,
// Boston, MA  02110-1301  USA
//
//-----------------------------------------------------------------------el-

#ifndef GRINS_TIME_STEPPING_PARSING_H
#define GRINS_TIME_STEPPING_PARSING_H

// C++
#include <string>

// Forward declarations
class GetPot;

namespace GRINS
{
  class TimeSteppingParsing
  {
  public:

    TimeSteppingParsing(){};

    ~TimeSteppingParsing(){};

    static unsigned int parse_n_timesteps( const GetPot& input );

    //! Parse option to retry failed time steps with smaller \f$ \Delta t \f$
    /*! backtrack_deltat is the number of time the TimeSolver will try
        to resolve the timestep with a smaller \f$ \Delta t \f$. Default is 0. */
    static unsigned int parse_backtrack_deltat( const GetPot& input );

    static double parse_theta( const GetPot& input );

    static double parse_deltat( const GetPot& input );
  };

} // end namespace GRINS

#endif // GRINS_TIME_STEPPING_PARSING_H
