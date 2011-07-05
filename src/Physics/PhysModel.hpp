// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#ifndef CF_Physics_PhysModel_hpp
#define CF_Physics_PhysModel_hpp

#include "Common/Component.hpp"

#include "Physics/LibPhysics.hpp"

namespace CF {

namespace Physics {

  class Variables; // forward declaration

  /// base type for the physical properties
  /// @note class is non copyable becase it might contain Eigen matrices
  /// @note this class and its derived classes should not ave any virtual functions
  struct Properties : public boost::noncopyable {};

////////////////////////////////////////////////////////////////////////////////

/// Component providing information about the physics
/// @author Tiago Quintino
/// @author Willem Deconinck
/// @author Bart Janssens
class Physics_API PhysModel : public Common::Component {

public: //typedefs

  typedef boost::shared_ptr<PhysModel> Ptr;
  typedef boost::shared_ptr<PhysModel const> ConstPtr;

public: // functions

  /// Contructor
  /// @param name of the component
  PhysModel ( const std::string& name );

  /// Virtual destructor
  virtual ~PhysModel();

  /// Get the class name
  static std::string type_name () { return "PhysModel"; }

  /// @name INTERFACE
  //@{

  /// @return dimensionality of the problem, which is
  ///         the number of spatial coordinates used in the PDEs
  virtual Uint ndim() const = 0;
  
  /// @return number of independent variables in the PDE
  virtual Uint neqs() const = 0;
  
  /// @return the physical model type
  virtual std::string type() const = 0;

  /// create a physical properties
  virtual std::auto_ptr<Physics::Properties> create_properties() = 0;

  //@} END INTERFACE

}; // PhysModel

////////////////////////////////////////////////////////////////////////////////


} // Physics
} // CF

#endif // CF_Physics_PhysModel_hpp
