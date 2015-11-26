#ifndef BASECONTACT_H
#define BASECONTACT_H

#include <SofaBaseCollision/DefaultContactManager.h>
#include <SofaBaseMechanics/MechanicalObject.h>
#include <sofa/core/collision/DetectionOutput.h>
#include <SofaBaseCollision/BaseContactMapper.h>

#include <Compliant/config.h>

#include "../constraint/Restitution.h"
#include "../constraint/HolonomicConstraintValue.h"
#include "../mapping/ContactMapping.h"
#include "../mapping/ContactMultiMapping.h"

//#include <sofa/simulation/common/DeactivatedNodeVisitor.h>

#include <sofa/helper/cast.h>


namespace sofa
{
namespace component
{
namespace collision
{

/// This is essentially a helper class to factor out all the painful
/// logic of contact classes in order to leave only interesting methods
/// for derived classes to implement.
template <class TCollisionModel1, class TCollisionModel2, class ResponseDataTypes = sofa::defaulttype::Vec3Types>
class BaseContact : public core::collision::Contact {
public:


    SOFA_ABSTRACT_CLASS(SOFA_TEMPLATE3(BaseContact, TCollisionModel1, TCollisionModel2, ResponseDataTypes), core::collision::Contact );



    typedef core::collision::Contact Inherit;
    typedef TCollisionModel1 CollisionModel1;
    typedef TCollisionModel2 CollisionModel2;
    typedef core::collision::Intersection Intersection;
    typedef ResponseDataTypes DataTypes1;
    typedef ResponseDataTypes DataTypes2;
    typedef core::behavior::MechanicalState<DataTypes1> MechanicalState1;
    typedef core::behavior::MechanicalState<DataTypes2> MechanicalState2;
    typedef typename CollisionModel1::Element CollisionElement1;
    typedef typename CollisionModel2::Element CollisionElement2;

    typedef core::collision::TDetectionOutputVector<CollisionModel1,CollisionModel2> DetectionOutputVector;
    typedef helper::vector< defaulttype::Vec<2, unsigned> > ContactPairVector;


    Data< SReal > damping_ratio;
    Data<bool> holonomic;
    Data<bool> keep;

protected:

    CollisionModel1* model1;
    CollisionModel2* model2;
    Intersection* intersectionMethod;
    bool selfCollision; ///< true if model1==model2 (in this case, only mapper1 is used)
    ContactMapper<CollisionModel1,DataTypes1> mapper1;
    ContactMapper<CollisionModel2,DataTypes2> mapper2;

    core::objectmodel::BaseContext* parent;


    DetectionOutputVector* contacts;
    ContactPairVector mappedContacts;

    BaseContact()
        : damping_ratio( this->initData(&damping_ratio, SReal(0.0), "damping", "contact damping (used for stabilization)") )
        , holonomic(this->initData(&holonomic, false, "holonomic", "only enforce null relative velocity, do not try to remove penetration during the dynamics pass"))
        , keep(this->initData(&keep, false, "keepAlive", "always keep contact nodes (deactivated when not colliding"))
    { }

    BaseContact(CollisionModel1* model1, CollisionModel2* model2, Intersection* intersectionMethod)
        : damping_ratio( this->initData(&damping_ratio, SReal(0.0), "damping", "contact damping (used for stabilization)") )
        , holonomic(this->initData(&holonomic, false, "holonomic", "only enforce null relative velocity, do not try to remove penetration during the dynamics pass"))
        , keep(this->initData(&keep, false, "keepAlive", "always keep contact nodes (deactivated when not colliding"))
        , model1(model1)
        , model2(model2)
        , intersectionMethod(intersectionMethod)
        , parent(NULL)
    {
        selfCollision = ((core::CollisionModel*)model1 == (core::CollisionModel*)model2);
        mapper1.setCollisionModel(model1);
        if (!selfCollision) mapper2.setCollisionModel(model2);
        mappedContacts.clear();
    }

    virtual ~BaseContact() {}



public:


    std::pair<core::CollisionModel*,core::CollisionModel*> getCollisionModels() { return std::make_pair(model1,model2); }

    void setDetectionOutputs(core::collision::DetectionOutputVector* o)
    {
        contacts = static_cast<DetectionOutputVector*>(o);
        
        // POSSIBILITY to have a filter on detected contacts, like removing duplicated/too close contacts
    }


    void createResponse(core::objectmodel::BaseContext* /*group*/ )
    {

        if( !contacts || contacts->size()==0)
        {
            // should only be called when keepAlive
            setActiveContact(false);
//            simulation::DeactivationVisitor v(sofa::core::ExecParams::defaultInstance(), false);
//            node->executeVisitor(&v);
            return; // keeping contact alive imposes a call with a null DetectionOutput
        }

        if( !contact_node )
        {
            //fancy names
            std::string name1 = this->model1->getClassName() + "_contact_points";
            std::string name2 = this->model2->getClassName() + "_contact_points";

            // obtain point mechanical models from mappers
            mstate1 = mapper1.createMapping( name1.c_str() );

            mstate2 = this->selfCollision ? mstate1 : mapper2.createMapping( name2.c_str() );
            mstate2->setName("dofs");
        }
       

        // resize mappers
        unsigned size = contacts->size();

        if ( this->selfCollision )
        {
            mapper1.resize( 2 * size );
        }
        else
        {
            mapper1.resize( size );
            mapper2.resize( size );
        }

        // setup mappers / mappedContacts

        // desired proximity contact distance
//        const double d0 = this->intersectionMethod->getContactDistance() + this->model1->getProximity() + this->model2->getProximity();

        mappedContacts.resize( size );

        for( unsigned i=0 ; i<contacts->size() ; ++i )
        {
            const core::collision::DetectionOutput& o = (*contacts)[i];

            CollisionElement1 elem1(o.elem.first);
            CollisionElement2 elem2(o.elem.second);

            int index1 = elem1.getIndex();
            int index2 = elem2.getIndex();

            // distance between the actual used dof and the geometrical contact point (must be initialized to 0, because if they are confounded, addPoint won't necessarily write the 0...)
            typename DataTypes1::Real r1 = 0.0;
            typename DataTypes2::Real r2 = 0.0;

            // Create mapping for first point
            mappedContacts[i][0] = mapper1.addPoint(o.point[0], index1, r1);

            // TODO local contact coords (o.baryCoords) are broken !!

            // max: this one is broken :-/
            // mappedContacts[i].index1 = mapper1.addPointB(o.point[0], index1, r1, o.baryCoords[0]);

            // Create mapping for second point
            mappedContacts[i][1] = this->selfCollision ?
                mapper1.addPoint(o.point[1], index2, r2):
                mapper2.addPoint(o.point[1], index2, r2);

                // max: same here :-/
                        // mapper1.addPointB(o.point[1], index2, r2, o.baryCoords[1]) :
                        // mapper2.addPointB(o.point[1], index2, r2, o.baryCoords[1]);

//            mappedContacts[i].distance = d0 + r1 + r2;

        }

        // poke mappings
        mapper1.update();
        if (!this->selfCollision) mapper2.update();

        if(!contact_node) {
            create_node();
        } else {
            setActiveContact(true);
//            simulation::DeactivationVisitor v(sofa::core::ExecParams::defaultInstance(), true);
//            node->executeVisitor(&v);
         	update_node();
        }
    }


    /// @internal for SOFA collision mechanism
    /// called before setting-up new collisions
    void removeResponse() {
        if( contact_node ) {
            mapper1.resize(0);
            mapper2.resize(0);
            mappedContacts.resize(0);
        }
    }

    /// @internal for SOFA collision mechanism
    /// called when the collision components must be removed from the scene graph
    virtual void cleanup() {
        
        // should be called only when !keep

        if( contact_node ) {
            mapper1.cleanup();
            if (!this->selfCollision) mapper2.cleanup();
            contact_node->detachFromGraph();
            contact_node.reset();
        }
        mappedContacts.clear();
    }

    /// @internal for SOFA collision mechanism
    /// to check if the collision components must be removed from the scene graph
    /// or if it should be kept but deactivated
    /// when the objects are no longer colliding
    virtual bool keepAlive() { return keep.getValue(); }


protected:

    typedef SReal real;

    // the node that will hold all the stuff
    typedef sofa::simulation::Node node_type;
    node_type::SPtr contact_node;

    // TODO correct real type
    typedef container::MechanicalObject<ResponseDataTypes> delta_dofs_type;
    typename delta_dofs_type::SPtr delta_dofs;

    typename MechanicalState1::SPtr mstate1;
    typename MechanicalState2::SPtr mstate2;

    typename odesolver::BaseConstraintValue::SPtr baseConstraintValue;

    // all you're left to implement \o/
    virtual void create_node() = 0;
    virtual void update_node() = 0;

    virtual void setActiveContact(bool active)
    {
        contact_node->setActive(active);
    }

    template <class OutType>
    core::BaseMapping::SPtr createContactMapping(node_type::SPtr node, typename container::MechanicalObject<OutType>::SPtr dofs,   vector<bool>* cvmask = NULL)
    {
        
        if( this->selfCollision ) {

            // contact mapping
            typedef sofa::component::mapping::ContactMapping<ResponseDataTypes, OutType> contact_mapping_type;
            typename contact_mapping_type::SPtr mapping  = core::objectmodel::New<contact_mapping_type>();
            mapping->setModels( this->mstate1.get(), dofs.get() );
            mapping->setDetectionOutput(this->contacts);
            mapping->setContactPairs(&this->mappedContacts);
            mapping->setName( this->getName() + "_contact_mapping" );

            node->addObject( mapping.get() );
            mapping->init();
            if(cvmask)
                mapping->mask = *cvmask;
            return core::objectmodel::SPtr_static_cast<core::BaseMapping>(mapping);
        }
        else
        {
            typedef sofa::component::mapping::ContactMultiMapping<ResponseDataTypes, OutType> contact_mapping_type;
            typename contact_mapping_type::SPtr mapping = core::objectmodel::New<contact_mapping_type>();
            mapping->addInputModel( this->mstate1.get() );
            mapping->addInputModel( this->mstate2.get() );
            mapping->addOutputModel( dofs.get() );
            mapping->setDetectionOutput(this->contacts);
            mapping->setContactPairs(&this->mappedContacts);
            mapping->setName( this->getName() + "_contact_multimapping" );

            node->addObject( mapping.get() );
            down_cast< node_type >(this->mstate2->getContext())->addChild( node.get() );
            mapping->init();
            if(cvmask)
                mapping->mask = *cvmask;
            return core::objectmodel::SPtr_static_cast<core::BaseMapping>(mapping);
        }
    }

    /// @internal
    /// insert a ConstraintValue component in the given graph depending on restitution/damping values
    /// return possible pointer to the activated constraint mask
    template<class contact_dofs_type>
    vector<bool>* addConstraintValue( node_type* node, contact_dofs_type* dofs/*, real damping*/, real restitution=0 )
    {
        assert( restitution>=0 && restitution<=1 );

        if( restitution ) // elastic contact
        {
            odesolver::Restitution::SPtr constraintValue = sofa::core::objectmodel::New<odesolver::Restitution>( dofs );
            node->addObject( constraintValue.get() );
//            constraintValue->dampingRatio.setValue( damping );
            constraintValue->restitution.setValue( restitution );

            // don't activate non-penetrating contacts
            odesolver::Restitution::mask_type& mask = *constraintValue->mask.beginWriteOnly();
            mask.resize( this->mappedContacts.size() );
            for(unsigned i = 0; i < this->mappedContacts.size(); ++i) {
                mask[i] = ( (*this->contacts)[i].value <= 0 );
            }
            constraintValue->mask.endEdit();

            constraintValue->init();
            baseConstraintValue = constraintValue;
            return &mask;
        }
//        else //if( damping ) // damped constraint
//        {
//            odesolver::ConstraintValue::SPtr constraintValue = sofa::core::objectmodel::New<odesolver::ConstraintValue>( dofs );
//            node->addObject( constraintValue.get() );
////            constraintValue->dampingRatio.setValue( damping );
//            constraintValue->init();
//        }
        else if( holonomic.getValue() ) // holonomic constraint (cancel relative velocity, w/o stabilization, contact penetration is not canceled)
        {
            // with stabilization holonomic and stabilization constraint values are equivalent

            typedef odesolver::HolonomicConstraintValue stab_type;
            stab_type::SPtr stab = sofa::core::objectmodel::New<stab_type>( dofs );
            node->addObject( stab.get() );

            // don't stabilize non-penetrating contacts (normal component only)
            odesolver::HolonomicConstraintValue::mask_type& mask = *stab->mask.beginWriteOnly();
            mask.resize(  this->mappedContacts.size() );
            for(unsigned i = 0; i < this->mappedContacts.size(); ++i) {
                mask[i] = ( (*this->contacts)[i].value <= 0 );
            }
            stab->mask.endEdit();

            stab->init();
            baseConstraintValue = stab;
            return &mask;
        }
        else // stabilized constraint
        {
            // stabilizer
            typedef odesolver::Stabilization stab_type;
            stab_type::SPtr stab = sofa::core::objectmodel::New<stab_type>( dofs );
            node->addObject( stab.get() );

            // don't stabilize non-penetrating contacts (normal component only)
            odesolver::Stabilization::mask_type& mask = *stab->mask.beginWriteOnly();
            mask.resize( this->mappedContacts.size() );
            for(unsigned i = 0; i < this->mappedContacts.size(); ++i) {
                mask[i] = ( (*this->contacts)[i].value <= 0 );
            }
            stab->mask.endEdit();

            stab->init();
            baseConstraintValue = stab;
            return &mask;
        }

        return NULL;
    }

};




//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////




/// a base class for compliant constraint based contact
template <class TCollisionModel1, class TCollisionModel2, class ResponseDataTypes = sofa::defaulttype::Vec3Types>
class BaseCompliantConstraintContact : public BaseContact<TCollisionModel1,TCollisionModel2,ResponseDataTypes>
{
public:

    SOFA_ABSTRACT_CLASS(SOFA_TEMPLATE3(BaseCompliantConstraintContact, TCollisionModel1, TCollisionModel2, ResponseDataTypes),SOFA_TEMPLATE3(BaseContact, TCollisionModel1, TCollisionModel2, ResponseDataTypes));

    typedef BaseContact<TCollisionModel1,TCollisionModel2,ResponseDataTypes> Inherit;
    typedef TCollisionModel1 CollisionModel1;
    typedef TCollisionModel2 CollisionModel2;
    typedef core::collision::Intersection Intersection;


    Data< SReal > compliance_value;
    Data< SReal > restitution_coef;

protected:

    BaseCompliantConstraintContact()
        : Inherit()
        , compliance_value( this->initData(&compliance_value, SReal(0.0), "compliance", "contact compliance: use model contact stiffnesses when < 0, use given value otherwise"))
        , restitution_coef( initData(&restitution_coef, SReal(0.0), "restitution", "global restitution coef") )
    {}

    BaseCompliantConstraintContact(CollisionModel1* model1, CollisionModel2* model2, Intersection* intersectionMethod)
        : Inherit( model1, model2, intersectionMethod )
        , compliance_value( this->initData(&compliance_value, SReal(0.0), "compliance", "contact compliance: use model contact stiffnesses when < 0, use given value otherwise"))
        , restitution_coef( initData(&restitution_coef, SReal(0.0), "restitution", "global restitution coef") )
    {}

};



}
}
}


#endif
