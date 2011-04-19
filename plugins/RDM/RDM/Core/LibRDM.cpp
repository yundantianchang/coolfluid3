// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include "Common/RegistLibrary.hpp"
#include "Common/CRoot.hpp"
#include "Common/CGroup.hpp"

#include "RDM/Core/LibRDM.hpp"
#include "RDM/Core/ScalarAdvection.hpp"

namespace CF {
namespace RDM {

using namespace CF::Common;

CF::Common::RegistLibrary<LibRDM> libRDM;

////////////////////////////////////////////////////////////////////////////////

void LibRDM::initiate_impl()
{
  CGroup::Ptr rdm_group =
    Core::instance().root()
      ->get_child_ptr("Tools")
      ->create_component<CGroup>( "RDM" );
  rdm_group->mark_basic();

  rdm_group->create_component<RDM::ScalarAdvection>( "SetupScalarSimulation" )
      ->mark_basic();
}

void LibRDM::terminate_impl()
{
  Core::instance().root()
      ->get_child_ptr("Tools")
      ->get_child_ptr("RDM")
      ->remove_component( "SetupScalarSimulation" );
  Core::instance().root()
      ->get_child_ptr("Tools")
      ->remove_component("RDM");
}

////////////////////////////////////////////////////////////////////////////////

} // RDM
} // CF