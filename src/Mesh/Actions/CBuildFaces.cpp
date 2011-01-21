// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include <boost/foreach.hpp>

#include "Common/Log.hpp"
#include "Common/CBuilder.hpp"
#include "Common/ComponentPredicates.hpp"
#include "Common/Foreach.hpp"
#include "Common/StreamHelpers.hpp"

#include "Mesh/Actions/CBuildFaces.hpp"
#include "Mesh/CElements.hpp"
#include "Mesh/CRegion.hpp"
#include "Mesh/CField.hpp"
#include "Mesh/CFaceCellConnectivity.hpp"

#include "Math/MathFunctions.hpp"

//////////////////////////////////////////////////////////////////////////////

namespace CF {
namespace Mesh {
namespace Actions {
  
  using namespace Common;
  using namespace Math::MathFunctions;
  
////////////////////////////////////////////////////////////////////////////////

Common::ComponentBuilder < CBuildFaces, CMeshTransformer, LibActions> CBuildFaces_Builder;

//////////////////////////////////////////////////////////////////////////////

CBuildFaces::CBuildFaces( const std::string& name )
: CMeshTransformer(name)
{
   
	properties()["brief"] = std::string("Print information of the mesh");
	std::string desc;
	desc = 
  "  Usage: Info \n\n"
  "          Information given: internal mesh hierarchy,\n"
  "      element distribution for each region, and element type"; 
	properties()["description"] = desc;
}

/////////////////////////////////////////////////////////////////////////////

std::string CBuildFaces::brief_description() const
{
  return properties()["brief"].value<std::string>();
}

/////////////////////////////////////////////////////////////////////////////

  
std::string CBuildFaces::help() const
{
  return "  " + properties()["brief"].value<std::string>() + "\n" + properties()["description"].value<std::string>();
}  
  
/////////////////////////////////////////////////////////////////////////////

void CBuildFaces::transform(const CMesh::Ptr& mesh, const std::vector<std::string>& args)
{

  m_mesh = mesh;

  // traverse regions and make interface region between connected regions recursively
  //make_interfaces(m_mesh);
  build_inner_faces_bottom_up(m_mesh);
}

//////////////////////////////////////////////////////////////////////////////

void CBuildFaces::make_interfaces(Component::Ptr parent)
{
  cf_assert_desc("parent must be a CRegion or CMesh", 
    is_not_null( parent->as_type<CMesh>() ) || is_not_null( parent->as_type<CRegion>() ) );
 
  CElements::Ptr comp;
  Uint idx_in_comp;
  
 
  
  std::vector<CRegion::Ptr> regions = range_to_vector(find_components<CRegion>(*parent));
  const Uint n=regions.size();
  if (n>1)
  {
    //Uint nb_interfaces = factorial(n) / (2*factorial(n-2));
    for (Uint i=0; i<n; ++i)
    for (Uint j=i+1; j<n; ++j)
    {
      CRegion& interface = *parent->create_component<CRegion>("interface_"+regions[i]->name()+"_to_"+regions[j]->name());
      interface.add_tag("interface");
      
      CFaceCellConnectivity::Ptr face_to_cell = allocate_component<CFaceCellConnectivity>("face_to_cell");
      //face_to_cell->configure_property("StoreIsBdry",true);
      //face_to_cell->configure_property("FilterBdry",true); //
      CFinfo << "creating face to cell for interfaces for " << interface.full_path().path() << CFendl;
      face_to_cell->setup(*regions[i],*regions[j]);
      // for (Uint i=0; i<face_to_cell->size(); ++i)
      // {
      //   CFinfo << "face ["<<i<<"] :  ";
      //   boost_foreach( const Uint elem_idx, face_to_cell->connectivity()[i] )
      //   { 
      //     boost::tie(comp,idx_in_comp) = face_to_cell->element_location(elem_idx);
      //     CFinfo << "   " << comp->glb_idx()[idx_in_comp];
      //   }
      //   CFinfo << CFendl;
      // }
      if (face_to_cell->size() == 0) // no faces found --> no interface
        *parent->remove_component(interface.name()); 
      else
      {
        build_inner_face_elements(interface,*face_to_cell);
        
        boost_foreach( CElements& elements, find_components<CElements>(interface) )
        {
          elements.add_tag("interface_faces");
        }
        
      }
    }
    // boost_foreach(CElements& elements, find_components_recursively_with_filter<CElements>(*parent,IsElementsVolume()))
    // {
    //   CList<bool>& is_bdry = *elements.get_child<CList<bool> >("is_bdry");
    //   for (Uint i=0; i<elements.size(); ++i)
    //   {
    //     CFinfo << "elem ["<<elements.glb_idx()[i] << "] : " << is_bdry[i] << CFendl;
    //   }
    // }
  }
}

////////////////////////////////////////////////////////////////////////////////

void CBuildFaces::build_inner_faces_bottom_up(Component::Ptr parent)
{
  cf_assert_desc("parent must be a CRegion or CMesh", 
    is_not_null( parent->as_type<CMesh>() ) || is_not_null( parent->as_type<CRegion>() ) );
  
  CElements::Ptr comp;
  Uint idx_in_comp;
  
  boost_foreach( CRegion& region, find_components<CRegion>(*parent) )
  {
    build_inner_faces_bottom_up(region.self());
    
    if ( count( find_components_with_filter<CElements>(region,IsElementsVolume()) ) != 0 )
    {
      // this region is the bottom region with volume elements

      // create region for cells
      CRegion& cells = region.create_region("cells");
      boost_foreach(CElements& elements, find_components_with_filter<CElements>(region,IsElementsVolume()))
      {
        elements.move_to(cells.self());
      }

      CFaceCellConnectivity::Ptr face_to_cell = allocate_component<CFaceCellConnectivity>("face_to_cell");
      CFinfo << "creating face to cell for inner cells of " << region.full_path().path() << CFendl;
      face_to_cell->setup(cells);
      
      // for (Uint i=0; i<face_to_cell->size(); ++i)
      // {
      //   CFinfo << "face ["<<i<<"] :  ";
      //   boost_foreach( const Uint elem_idx, face_to_cell->connectivity()[i] )
      //   { 
      //     boost::tie(comp,idx_in_comp) = face_to_cell->element_location(elem_idx);
      //     CFinfo << "   " << comp->glb_idx()[idx_in_comp];
      //   }
      //   CFinfo << CFendl;
      // }
      // boost_foreach(CElements& elements, find_components<CElements>(cells))
      // {
      //   CList<bool>& is_bdry = *elements.get_child<CList<bool> >("is_bdry");
      //   for (Uint i=0; i<elements.size(); ++i)
      //   {
      //     CFinfo << "elem ["<<elements.glb_idx()[i]<<"] : " << is_bdry[i] << CFendl;
      //   }
      // }
      
      CRegion& inner_faces = region.create_region("inner_faces");
      build_inner_face_elements(inner_faces,*face_to_cell);
      
      CRegion& outer_faces = region.create_region("outer_faces");
      build_outer_face_elements(outer_faces,*face_to_cell);
      
      
    }
    else
    { 
      // must use different way, checking from 1 region to another for a match, not just in bdry elements
      // this region is connected to another region
      make_interfaces(region.self());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void CBuildFaces::build_inner_face_elements(CRegion& region, CFaceCellConnectivity& face_to_cell)
{  
  std::set<std::string> face_types;
  std::map<std::string,boost::shared_ptr<CTable<Uint>::Buffer> > f2c_buffer_map;
  std::map<std::string,boost::shared_ptr<CList<Uint>::Buffer> > fnb_buffer_map; 
  std::map<std::string,boost::shared_ptr<CTable<Uint>::Buffer> > f2n_buffer_map; 
  
  CElements::Ptr elem_comp;
  Uint elem_idx;
  
  CList<Uint>& face_number = *face_to_cell.get_child<CList<Uint> >("face_number");
  
  for (Uint f=0; f<face_to_cell.size(); ++f)
  {
    boost::tie(elem_comp,elem_idx) = face_to_cell.element_location(face_to_cell.connectivity()[f][0]);
    const Uint face_nb = face_number[f];
    face_types.insert(elem_comp->element_type().face_type(face_nb).element_type_name());
  }
  
  boost_foreach( const std::string& face_type , face_types)
  {
    CElements& elements = region.create_elements(face_type,m_mesh->nodes());
    elements.add_tag("inner_faces");
    
    CTable<Uint>& f2n = elements.connectivity_table();
    f2n.set_row_size(2);
    f2n_buffer_map[face_type] = boost::shared_ptr<CTable<Uint>::Buffer> ( new CTable<Uint>::Buffer(f2n.create_buffer()) );
    
    CFaceCellConnectivity& f2c = *elements.create_component<CFaceCellConnectivity>("cell_connectivity");
    f2c.set_elements(face_to_cell.get_child<CUnifiedData<CElements> >("elements"));
    f2c_buffer_map[face_type] = boost::shared_ptr<CTable<Uint>::Buffer> ( new CTable<Uint>::Buffer(f2c.get_child<CTable<Uint> >("connectivity_table")->create_buffer()) );

    fnb_buffer_map[face_type] = boost::shared_ptr<CList<Uint>::Buffer>  ( new CList<Uint>::Buffer (f2c.get_child<CList<Uint> > ("face_number")->create_buffer()) );
  }
  
  for (Uint f=0; f<face_to_cell.size(); ++f)
  {
    boost::tie(elem_comp,elem_idx) = face_to_cell.element_location(face_to_cell.connectivity()[f][0]);
    const Uint face_nb = face_number[f];
    const std::string face_type = elem_comp->element_type().face_type(face_nb).element_type_name();

    if (face_to_cell.connectivity()[f].size()==2)
    {
      f2c_buffer_map[face_type]->add_row(face_to_cell.connectivity()[f]);
      fnb_buffer_map[face_type]->add_row(face_number[f]);
      f2n_buffer_map[face_type]->add_row(face_to_cell.nodes(f));
    }
  }
  
}

////////////////////////////////////////////////////////////////////////////////

void CBuildFaces::build_outer_face_elements(CRegion& region, CFaceCellConnectivity& face_to_cell)
{  
  std::set<std::string> face_types;
  std::map<std::string,boost::shared_ptr<CList<Uint>::Buffer> > f2c_buffer_map;
  std::map<std::string,boost::shared_ptr<CList<Uint>::Buffer> > fnb_buffer_map; 
  std::map<std::string,boost::shared_ptr<CTable<Uint>::Buffer> > f2n_buffer_map; 
  
  CElements::Ptr elem_comp;
  Uint elem_idx;
  
  CList<Uint>& outer_faces = *face_to_cell.get_child<CList<Uint> >("bdry_face_connectivity");
  CList<Uint>& bdry_face_number = *face_to_cell.get_child<CList<Uint> >("bdry_face_number");
  
  for (Uint f=0; f<outer_faces.size(); ++f)
  {
    boost::tie(elem_comp,elem_idx) = face_to_cell.element_location(outer_faces[f]);
    const Uint face_nb = bdry_face_number[f];
    face_types.insert(elem_comp->element_type().face_type(face_nb).element_type_name());
  }
  
  boost_foreach( const std::string& face_type , face_types)
  {
    CElements& elements = region.create_elements(face_type,m_mesh->nodes());
    elements.add_tag("outer_faces");
    
    CTable<Uint>& f2n = elements.connectivity_table();
    f2n_buffer_map[face_type] = boost::shared_ptr<CTable<Uint>::Buffer> ( new CTable<Uint>::Buffer(f2n.create_buffer()) );
    
    CFaceCellConnectivity& f2c = *elements.create_component<CFaceCellConnectivity>("cell_connectivity");
    f2c.set_elements(face_to_cell.get_child<CUnifiedData<CElements> >("elements"));
    f2c_buffer_map[face_type] = boost::shared_ptr<CList<Uint>::Buffer> ( new CList<Uint>::Buffer(f2c.get_child<CList<Uint> >("bdry_face_connectivity")->create_buffer()) );

    fnb_buffer_map[face_type] = boost::shared_ptr<CList<Uint>::Buffer>  ( new CList<Uint>::Buffer (f2c.get_child<CList<Uint> > ("bdry_face_number")->create_buffer()) );
  }
  
  for (Uint f=0; f<outer_faces.size(); ++f)
  {
    boost::tie(elem_comp,elem_idx) = face_to_cell.element_location(outer_faces[f]);
    const Uint face_nb = bdry_face_number[f];
    const std::string face_type = elem_comp->element_type().face_type(face_nb).element_type_name();

    f2c_buffer_map[face_type]->add_row(outer_faces[f]);
    fnb_buffer_map[face_type]->add_row(bdry_face_number[f]);
    f2n_buffer_map[face_type]->add_row(face_to_cell.nodes_using_bdry_face_connectivity(f));
  }
  
}

//////////////////////////////////////////////////////////////////////////////

} // Actions
} // Mesh
} // CF