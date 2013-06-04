#ifndef stk_adapt_PredicateBasedElementAdapter_hpp
#define stk_adapt_PredicateBasedElementAdapter_hpp

#include <functional>

#include <stk_adapt/IAdapter.hpp>

namespace stk {
  namespace adapt {

    //========================================================================================================================
    //========================================================================================================================
    //========================================================================================================================
    /**
     *  Predicate-based marker
     *
     *  The functor @class RefinePredicate should supply an operator() that returns an entry from AdaptInstruction,
     *    either to do nothing, refine, unrefine, or both refine & unrefine (useful for unit testing, etc.)
     */
    typedef std::unary_function<stk::mesh::Entity , bool> AdapterPredicateFunctor;

    template<class RefinePredicate>
    class PredicateBasedElementAdapter : public IAdapter
    {
      RefinePredicate& m_predicate_refine;

    public:

      PredicateBasedElementAdapter(RefinePredicate& predicate_refine,
                           percept::PerceptMesh& eMesh, UniformRefinerPatternBase & bp, stk::mesh::FieldBase *proc_rank_field=0) :
        IAdapter(eMesh, bp, proc_rank_field), m_predicate_refine(predicate_refine)
      {
      }

      RefinePredicate& getRefinePredicate() { return m_predicate_refine; }

      virtual ElementUnrefineCollection  buildUnrefineList()
      {
        ElementUnrefineCollection elements_to_unref(*m_eMesh.get_bulk_data());

        const vector<stk::mesh::Bucket*> & buckets = m_eMesh.get_bulk_data()->buckets( stk::mesh::MetaData::ELEMENT_RANK );

        for ( vector<stk::mesh::Bucket*>::const_iterator k = buckets.begin() ; k != buckets.end() ; ++k )
          {
            {
              stk::mesh::Bucket & bucket = **k ;

              const unsigned num_entity_in_bucket = bucket.size();
              for (unsigned ientity = 0; ientity < num_entity_in_bucket; ientity++)
                {
                  stk::mesh::Entity element = bucket[ientity];

                  // FIXME
                  // skip elements that are already a parent (if there's no family tree yet, it's not a parent, so avoid throwing an error is isParentElement)
                  const bool check_for_family_tree = false;
                  bool isParent = m_eMesh.isParentElement(element, check_for_family_tree);

                  if (isParent)
                    continue;

                  const percept::MyPairIterRelation elem_nodes (m_eMesh, element, stk::mesh::MetaData::NODE_RANK);

                  if (elem_nodes.size())
                    {
                      if (m_predicate_refine(element) & DO_UNREFINE)
                        elements_to_unref.insert(element);
                    }
                }
            }
          }

        return elements_to_unref;
      }

    protected:

      virtual void
      refineMethodApply(NodeRegistry::ElementFunctionPrototype function, const stk::mesh::Entity element,
            vector<NeededEntityType>& needed_entity_ranks)
      {
        const CellTopologyData * const cell_topo_data = m_eMesh.get_cell_topology(element);

        CellTopology cell_topo(cell_topo_data);
        const percept::MyPairIterRelation elem_nodes (m_eMesh, element, stk::mesh::MetaData::NODE_RANK);

        //VectorFieldType* coordField = m_eMesh.get_coordinates_field();

        bool markInfo = (m_predicate_refine(element) & DO_REFINE);

        for (unsigned ineed_ent=0; ineed_ent < needed_entity_ranks.size(); ineed_ent++)
          {
            unsigned numSubDimNeededEntities = 0;
            stk::mesh::EntityRank needed_entity_rank = needed_entity_ranks[ineed_ent].first;

            if (needed_entity_rank == m_eMesh.edge_rank())
              {
                numSubDimNeededEntities = cell_topo_data->edge_count;
              }
            else if (needed_entity_rank == m_eMesh.face_rank())
              {
                numSubDimNeededEntities = cell_topo_data->side_count;
              }
            else if (needed_entity_rank == stk::mesh::MetaData::ELEMENT_RANK)
              {
                numSubDimNeededEntities = 1;
              }

            for (unsigned iSubDimOrd = 0; iSubDimOrd < numSubDimNeededEntities; iSubDimOrd++)
              {
                (m_nodeRegistry ->* function)(element, needed_entity_ranks[ineed_ent], iSubDimOrd, markInfo);
              }
          }
      }

    };



  }
}
#endif
