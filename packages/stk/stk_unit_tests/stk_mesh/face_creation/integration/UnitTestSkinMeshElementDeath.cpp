
#include <gtest/gtest.h>                // for AssertHelper, ASSERT_TRUE, etc
#include <stddef.h>                     // for size_t
#include <algorithm>                    // for find
#include <iostream>                     // for operator<<, ostream, cerr, etc
#include <stk_io/IossBridge.hpp>        // for put_io_part_attribute
#include <stk_mesh/base/BulkData.hpp>   // for BulkData, etc
#include <stk_mesh/base/Comm.hpp>       // for comm_mesh_counts
#include <stk_mesh/base/CreateFaces.hpp>  // for create_faces
#include <stk_mesh/base/ElemElemGraph.hpp>  // for process_killed_elements, etc
#include <stk_mesh/base/GetEntities.hpp>  // for get_selected_entities, etc
#include <stk_mesh/base/MetaData.hpp>   // for MetaData
#include <stk_topology/topology.hpp>    // for topology, etc
#include <stk_unit_test_utils/ioUtils.hpp>  // for fill_mesh_using_stk_io, etc
#include <stk_util/parallel/Parallel.hpp>  // for parallel_machine_size, etc
#include <vector>                       // for vector
#include "stk_unit_test_utils/ElemGraphTestUtils.hpp"  // for deactivate_elements, etc
#include "gtest/gtest-message.h"        // for Message
#include "mpi.h"                        // for ompi_communicator_t, etc
#include "stk_mesh/base/Bucket.hpp"     // for Bucket
#include "stk_mesh/base/BulkDataInlinedMethods.hpp"
#include "stk_mesh/base/Entity.hpp"     // for Entity, operator<<
#include "stk_mesh/base/EntityKey.hpp"  // for EntityKey, operator<<
#include "stk_mesh/base/Part.hpp"       // for Part
#include "stk_mesh/base/Selector.hpp"   // for Selector, etc
#include "stk_mesh/base/Types.hpp"      // for EntityVector, PartVector, etc
#include "stk_unit_test_utils/unittestMeshUtils.hpp"

namespace
{

class BulkDataTester : public stk::mesh::BulkData
{
public:
    BulkDataTester(stk::mesh::MetaData &mesh_meta_data, MPI_Comm comm) :
            stk::mesh::BulkData(mesh_meta_data, comm)
    {
    }

    virtual ~BulkDataTester()
    {
    }

    void set_sorting_by_face()
    {
        m_shouldSortFacesByNodeIds = true;
    }
};


void kill_element(stk::mesh::Entity element, stk::mesh::BulkData& bulkData, stk::mesh::Part& active, stk::mesh::Part& skin)
{
    stk::mesh::ElemElemGraph graph(bulkData, active);
    stk::mesh::EntityVector deactivated_elems;
    if(bulkData.is_valid(element) && bulkData.parallel_owner_rank(element) == bulkData.parallel_rank())
    {
        deactivated_elems.push_back(element);
    }

    stk::mesh::PartVector boundary_mesh_parts={&active, &skin};
    ElemGraphTestUtils::deactivate_elements(deactivated_elems, bulkData,  active);
    stk::mesh::process_killed_elements(bulkData, graph, deactivated_elems, active, boundary_mesh_parts, &boundary_mesh_parts);
}

stk::mesh::EntityVector get_entities(stk::mesh::BulkData& bulkData, const stk::mesh::ConstPartVector& parts)
{
    stk::mesh::EntityVector entities;
    stk::mesh::Selector sel = stk::mesh::selectIntersection(parts);
    stk::mesh::get_selected_entities(sel, bulkData.buckets(stk::topology::FACE_RANK), entities);
    return entities;
}

void compare_faces(const stk::mesh::BulkData& bulkData, const std::vector<size_t> &num_gold_skinned_faces, const stk::mesh::EntityVector& skinned_faces, const stk::mesh::EntityVector &active_faces)
{
    EXPECT_EQ(num_gold_skinned_faces[bulkData.parallel_rank()], skinned_faces.size());
    EXPECT_EQ(num_gold_skinned_faces[bulkData.parallel_rank()], active_faces.size());

    for(size_t i=0;i<skinned_faces.size();++i)
    {
        if (bulkData.identifier(skinned_faces[i]) != bulkData.identifier(active_faces[i]))
        {
            std::cerr << "Skinned faces: ";
            for(size_t j=0;j<skinned_faces.size();++j)
            {
                std::cerr << skinned_faces[j] << "\t";
            }
            std::cerr << std::endl;

            std::cerr << "active faces: ";
            for(size_t j=0;j<active_faces.size();++j)
            {
                std::cerr << active_faces[j] << "\t";
            }
            std::cerr << std::endl;
            break;
        }
    }
}

void compare_skin(const std::vector<size_t>& num_gold_skinned_faces, stk::mesh::BulkData& bulkData, const stk::mesh::Part& skin, const stk::mesh::Part& active)
{
    stk::mesh::EntityVector skinned_faces = get_entities(bulkData, {&skin, &active});
    stk::mesh::EntityVector active_faces  = get_entities(bulkData, {&active} );
    compare_faces(bulkData, num_gold_skinned_faces, skinned_faces, active_faces);
}

TEST(ElementDeath, compare_death_and_skin_mesh)
{
    stk::ParallelMachine comm = MPI_COMM_WORLD;

     if(stk::parallel_machine_size(comm) == 1)
     {
         unsigned spatialDim = 3;

         stk::mesh::MetaData meta(spatialDim);
         stk::mesh::Part& skin  = meta.declare_part_with_topology("skin", stk::topology::QUAD_4);
         stk::io::put_io_part_attribute(skin);
         BulkDataTester bulkData(meta, comm);
         bulkData.set_sorting_by_face();

         stk::mesh::Part& active = meta.declare_part("active"); // can't specify rank, because it gets checked against size of rank_names
         stk::unit_test_util::fill_mesh_using_stk_io("generated:1x1x4", bulkData, comm);
         stk::unit_test_util::put_mesh_into_part(bulkData, active);

         ElemGraphTestUtils::skin_boundary(bulkData, active, {&skin, &active});

         std::vector<size_t> num_gold_skinned_faces = { 18 };
         compare_skin(num_gold_skinned_faces, bulkData, skin, active);

         stk::mesh::Entity element1 = bulkData.get_entity(stk::topology::ELEM_RANK, 1);
         kill_element(element1, bulkData, active, skin);

         ElemGraphTestUtils::skin_part(bulkData, active, {&skin, &active});

         num_gold_skinned_faces[0] = 14;
         compare_skin(num_gold_skinned_faces, bulkData, skin, active);
     }
}


} // end namespace


