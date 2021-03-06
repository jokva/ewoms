// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*
  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.

  Consult the COPYING file in the top-level source directory of this
  module for the precise wording of the license and the list of
  copyright holders.
*/
/*!
 * \file
 *
 * \copydoc Ewoms::BlackOilProblem
 */
#ifndef EWOMS_BLACKOIL_PROBLEM_HH
#define EWOMS_BLACKOIL_PROBLEM_HH

#include "blackoilproperties.hh"

#include <ewoms/models/common/multiphasebaseproblem.hh>

namespace Ewoms {

/*!
 * \ingroup BlackOilModel
 * \brief Base class for all problems which use the black-oil model.
 */
template<class TypeTag>
class BlackOilProblem : public MultiPhaseBaseProblem<TypeTag>
{
private:
    typedef MultiPhaseBaseProblem<TypeTag> ParentType;
    typedef typename GET_PROP_TYPE(TypeTag, Problem) Implementation;
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, Simulator) Simulator;

public:
    /*!
     * \copydoc Doxygen::defaultProblemConstructor
     *
     * \param simulator The manager object of the simulation
     */
    BlackOilProblem(Simulator &simulator)
        : ParentType(simulator)
    {}

    /*!
     * \brief Returns the index of the relevant region for thermodynmic properties
     */
    template <class Context>
    int pvtRegionIndex(const Context &context, int spaceIdx, int timeIdx) const
    { return 0; }

    /*!
     * \brief Returns the compressibility of the porous medium of a cell
     */
    template <class Context>
    Scalar rockCompressibility(const Context &context, int spaceIdx, int timeIdx) const
    { return 0.0; }

    /*!
     * \brief Returns the reference pressure for rock the compressibility of a cell
     */
    template <class Context>
    Scalar rockReferencePressure(const Context &context, int spaceIdx, int timeIdx) const
    { return 1e5; }

private:
    //! Returns the implementation of the problem (i.e. static polymorphism)
    Implementation &asImp_()
    { return *static_cast<Implementation *>(this); }

    //! \copydoc asImp_()
    const Implementation &asImp_() const
    { return *static_cast<const Implementation *>(this); }
};

} // namespace Ewoms

#endif
