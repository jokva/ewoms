// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*****************************************************************************
 *   Copyright (C) 2008-2009 by Markus Wolff                                 *
 *   Copyright (C) 2008-2009 by Andreas Lauser                               *
 *   Copyright (C) 2008 by Bernd Flemisch                                    *
 *   Institute for Modelling Hydraulic and Environmental Systems             *
 *   University of Stuttgart, Germany                                        *
 *   email: <givenname>.<name>@iws.uni-stuttgart.de                          *
 *                                                                           *
 *   This program is free software: you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation, either version 2 of the License, or       *
 *   (at your option) any later version.                                     *
 *                                                                           *
 *   This program is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *   GNU General Public License for more details.                            *
 *                                                                           *
 *   You should have received a copy of the GNU General Public License       *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 *****************************************************************************/

/*!
 * \ingroup IMPES
 * \ingroup Properties
 */
/*!
 * \file
 *
 * \brief Defines the properties required for (immiscible) twophase sequential models.
 */

#ifndef DUMUX_2PPROPERTIES_DECOUPLED_HH
#define DUMUX_2PPROPERTIES_DECOUPLED_HH

//Dumux-includes
#include <dumux/decoupled/common/decoupledproperties.hh>
#include "2pindices.hh"

namespace Dumux
{
////////////////////////////////
// forward declarations
////////////////////////////////
template <class TypeTag, bool enableCompressibility>
class CellData2P;

////////////////////////////////
// properties
////////////////////////////////
namespace Properties
{
//////////////////////////////////////////////////////////////////
// Type tags
//////////////////////////////////////////////////////////////////

//! The type tag for the two-phase problems
NEW_TYPE_TAG(DecoupledTwoP, INHERITS_FROM(DecoupledModel))
;

//////////////////////////////////////////////////////////////////
// Property tags
//////////////////////////////////////////////////////////////////

NEW_PROP_TAG( SpatialParameters )
; //!< The type of the spatial parameters object
NEW_PROP_TAG(MaterialLaw);   //!< The material law which ought to be used (extracted from the spatial parameters)
NEW_PROP_TAG(MaterialLawParams); //!< The context material law (extracted from the spatial parameters)
NEW_PROP_TAG( EnableGravity)
; //!< Returns whether gravity is considered in the problem
NEW_PROP_TAG( Formulation);
NEW_PROP_TAG( PressureFormulation)
; //!< The formulation of the model
NEW_PROP_TAG( SaturationFormulation)
; //!< The formulation of the model
NEW_PROP_TAG( VelocityFormulation)
; //!< The formulation of the model
NEW_PROP_TAG( EnableCompressibility)
;// !< Returns whether compressibility is allowed
NEW_PROP_TAG( WettingPhase)
; //!< The wetting phase for two-phase models
NEW_PROP_TAG( NonwettingPhase)
; //!< The non-wetting phase for two-phase models
NEW_PROP_TAG( FluidSystem )//!< Defines the fluid system
;
NEW_PROP_TAG( FluidState )//!< Defines the fluid state
;
NEW_PROP_TAG( TwoPIndices ); //!< keep for compatibility with box models

NEW_PROP_TAG( ErrorTermFactor );
NEW_PROP_TAG( ErrorTermLowerBound );
NEW_PROP_TAG( ErrorTermUpperBound );
}
}

#include <dumux/decoupled/common/variableclass.hh>
#include <dumux/decoupled/2p/cellData2p.hh>
#include <dumux/material/fluidsystems/2pimmisciblefluidsystem.hh>
#include <dumux/material/fluidstates/isoimmisciblefluidstate.hh>

namespace Dumux
{
namespace Properties
{
//////////////////////////////////////////////////////////////////
// Properties
//////////////////////////////////////////////////////////////////
SET_INT_PROP(DecoupledTwoP, NumEq, 2);

SET_INT_PROP(DecoupledTwoP, NumPhases, 2);//!< The number of phases in the 2p model is 2

SET_INT_PROP(DecoupledTwoP, NumComponents, 1); //!< Each phase consists of 1 pure component


SET_INT_PROP(DecoupledTwoP,
    Formulation,
    DecoupledTwoPCommonIndices::pwSw);

SET_PROP(DecoupledTwoP, Indices)
{
typedef DecoupledTwoPIndices<GET_PROP_VALUE(TypeTag, Formulation), 0> type;
};
SET_TYPE_PROP(DecoupledTwoP, TwoPIndices, typename GET_PROP_TYPE(TypeTag, Indices));

//! Set the default formulation
SET_INT_PROP(DecoupledTwoP,
    PressureFormulation,
    GET_PROP_TYPE(TypeTag, Indices)::pressureType);

SET_INT_PROP(DecoupledTwoP,
    SaturationFormulation,
    GET_PROP_TYPE(TypeTag, Indices)::saturationType);

SET_INT_PROP(DecoupledTwoP,
    VelocityFormulation,
    GET_PROP_TYPE(TypeTag, Indices)::velocityDefault);

SET_BOOL_PROP(DecoupledTwoP, EnableCompressibility, false);

SET_TYPE_PROP(DecoupledTwoP, Variables, VariableClass<TypeTag>);

SET_TYPE_PROP(DecoupledTwoP, CellData, CellData2P<TypeTag, GET_PROP_VALUE(TypeTag, EnableCompressibility)>);

SET_TYPE_PROP(DecoupledTwoP, FluidSystem, TwoPImmiscibleFluidSystem<TypeTag>);

SET_PROP(DecoupledTwoP, FluidState)
{
private:
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;
public:
    typedef IsothermalImmiscibleFluidState<Scalar, FluidSystem> type;
};

/*!
 * \brief Set the property for the material parameters by extracting
 *        it from the material law.
 */
SET_PROP(DecoupledTwoP, MaterialLawParams)
{
private:
    typedef typename GET_PROP_TYPE(TypeTag, MaterialLaw) MaterialLaw;

public:
    typedef typename MaterialLaw::Params type;
};

SET_SCALAR_PROP(DecoupledTwoP, ErrorTermFactor, 0.5);
SET_SCALAR_PROP(DecoupledTwoP, ErrorTermLowerBound, 0.1);
SET_SCALAR_PROP(DecoupledTwoP, ErrorTermUpperBound, 0.9);

// \}
}

}

#endif
