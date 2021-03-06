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
 * \copydoc Ewoms::PowerInjectionProblem
 */
#ifndef EWOMS_POWER_INJECTION_PROBLEM_HH
#define EWOMS_POWER_INJECTION_PROBLEM_HH

#include <opm/material/fluidmatrixinteractions/RegularizedVanGenuchten.hpp>
#include <opm/material/fluidmatrixinteractions/LinearMaterial.hpp>
#include <opm/material/fluidmatrixinteractions/EffToAbsLaw.hpp>
#include <opm/material/fluidmatrixinteractions/MaterialTraits.hpp>
#include <opm/material/fluidsystems/TwoPhaseImmiscibleFluidSystem.hpp>
#include <opm/material/fluidstates/ImmiscibleFluidState.hpp>
#include <opm/material/components/SimpleH2O.hpp>
#include <opm/material/components/Air.hpp>
#include <ewoms/models/immiscible/immisciblemodel.hh>

#include <dune/grid/yaspgrid.hh>
#include <ewoms/io/cubegridmanager.hh>

#include <dune/common/version.hh>
#include <dune/common/fvector.hh>
#include <dune/common/fmatrix.hh>

#include <sstream>
#include <string>
#include <type_traits>
#include <iostream>

namespace Ewoms {
template <class TypeTag>
class PowerInjectionProblem;
}

namespace Ewoms {
namespace Properties {
NEW_TYPE_TAG(PowerInjectionBaseProblem);

// Set the grid implementation to be used
SET_TYPE_PROP(PowerInjectionBaseProblem, Grid, Dune::YaspGrid</*dim=*/1>);

// set the GridManager property
SET_TYPE_PROP(PowerInjectionBaseProblem, GridManager,
              Ewoms::CubeGridManager<TypeTag>);

// Set the problem property
SET_TYPE_PROP(PowerInjectionBaseProblem, Problem,
              Ewoms::PowerInjectionProblem<TypeTag>);

// Set the wetting phase
SET_PROP(PowerInjectionBaseProblem, WettingPhase)
{
private:
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;

public:
    typedef Opm::LiquidPhase<Scalar, Opm::SimpleH2O<Scalar> > type;
};

// Set the non-wetting phase
SET_PROP(PowerInjectionBaseProblem, NonwettingPhase)
{
private:
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;

public:
    typedef Opm::GasPhase<Scalar, Opm::Air<Scalar> > type;
};

// Set the material Law
SET_PROP(PowerInjectionBaseProblem, MaterialLaw)
{
private:
    typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;
    enum { wettingPhaseIdx = FluidSystem::wettingPhaseIdx };
    enum { nonWettingPhaseIdx = FluidSystem::nonWettingPhaseIdx };

    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef Opm::TwoPhaseMaterialTraits<Scalar,
                                        /*wettingPhaseIdx=*/FluidSystem::wettingPhaseIdx,
                                        /*nonWettingPhaseIdx=*/FluidSystem::nonWettingPhaseIdx>
        Traits;

    // define the material law which is parameterized by effective
    // saturations
    typedef Opm::RegularizedVanGenuchten<Traits> EffectiveLaw;

public:
    // define the material law parameterized by absolute saturations
    typedef Opm::EffToAbsLaw<EffectiveLaw> type;
};

// Write out the filter velocities for this problem
SET_BOOL_PROP(PowerInjectionBaseProblem, VtkWriteFilterVelocities, true);

// Disable gravity
SET_BOOL_PROP(PowerInjectionBaseProblem, EnableGravity, false);

// define the properties specific for the power injection problem
SET_SCALAR_PROP(PowerInjectionBaseProblem, DomainSizeX, 100.0);
SET_SCALAR_PROP(PowerInjectionBaseProblem, DomainSizeY, 1.0);
SET_SCALAR_PROP(PowerInjectionBaseProblem, DomainSizeZ, 1.0);

SET_INT_PROP(PowerInjectionBaseProblem, CellsX, 250);
SET_INT_PROP(PowerInjectionBaseProblem, CellsY, 1);
SET_INT_PROP(PowerInjectionBaseProblem, CellsZ, 1);

// The default for the end time of the simulation
SET_SCALAR_PROP(PowerInjectionBaseProblem, EndTime, 100);

// The default for the initial time step size of the simulation
SET_SCALAR_PROP(PowerInjectionBaseProblem, InitialTimeStepSize, 1e-3);
} // namespace Properties
} // namespace Ewoms

namespace Ewoms {
/*!
 * \ingroup TestProblems
 * \brief 1D Problem with very fast injection of gas on the left.
 *
 * The velocity model is chosen in the .cc file in this problem. The
 * spatial parameters are inspired by the ones given by
 *
 * V. Jambhekar: "Forchheimer Porous-media Flow models -- Numerical
 * Investigation and Comparison with Experimental Data", Master's
 * Thesis at Institute for Modelling Hydraulic and Environmental
 * Systems, University of Stuttgart, 2011
 */
template <class TypeTag>
class PowerInjectionProblem : public GET_PROP_TYPE(TypeTag, BaseProblem)
{
    typedef typename GET_PROP_TYPE(TypeTag, BaseProblem) ParentType;

    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, GridView) GridView;
    typedef typename GET_PROP_TYPE(TypeTag, Indices) Indices;
    typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;
    typedef typename GET_PROP_TYPE(TypeTag, WettingPhase) WettingPhase;
    typedef typename GET_PROP_TYPE(TypeTag, NonwettingPhase) NonwettingPhase;
    typedef typename GET_PROP_TYPE(TypeTag, PrimaryVariables) PrimaryVariables;
    typedef typename GET_PROP_TYPE(TypeTag, EqVector) EqVector;
    typedef typename GET_PROP_TYPE(TypeTag, RateVector) RateVector;
    typedef typename GET_PROP_TYPE(TypeTag, BoundaryRateVector) BoundaryRateVector;
    typedef typename GET_PROP_TYPE(TypeTag, Simulator) Simulator;

    enum {
        // number of phases

        // phase indices
        wettingPhaseIdx = FluidSystem::wettingPhaseIdx,
        nonWettingPhaseIdx = FluidSystem::nonWettingPhaseIdx,

        // equation indices
        contiNEqIdx = Indices::conti0EqIdx + nonWettingPhaseIdx,

        // Grid and world dimension
        dim = GridView::dimension,
        dimWorld = GridView::dimensionworld
    };

    typedef typename GET_PROP_TYPE(TypeTag, MaterialLaw) MaterialLaw;
    typedef typename GET_PROP_TYPE(TypeTag, MaterialLawParams) MaterialLawParams;

    typedef typename GridView::ctype CoordScalar;
    typedef Dune::FieldVector<CoordScalar, dimWorld> GlobalPosition;

    typedef Dune::FieldMatrix<Scalar, dimWorld, dimWorld> DimMatrix;

public:
    /*!
     * \copydoc Doxygen::defaultProblemConstructor
     */
    PowerInjectionProblem(Simulator &simulator)
        : ParentType(simulator)
    { }

    /*!
     * \copydoc FvBaseProblem::finishInit
     */
    void finishInit()
    {
        ParentType::finishInit();

        eps_ = 3e-6;
        FluidSystem::init();

        temperature_ = 273.15 + 26.6;

        // parameters for the Van Genuchten law
        // alpha and n
        materialParams_.setVgAlpha(0.00045);
        materialParams_.setVgN(7.3);
        materialParams_.finalize();

        K_ = this->toDimMatrix_(5.73e-08); // [m^2]

        setupInitialFluidState_();
    }

    /*!
     * \name Auxiliary methods
     */
    //! \{

    /*!
     * \copydoc FvBaseProblem::name
     */
    std::string name() const
    {
        std::ostringstream oss;
        oss << "powerinjection_";
        if (std::is_same<typename GET_PROP_TYPE(TypeTag, FluxModule),
                         Ewoms::DarcyFluxModule<TypeTag> >::value)
            oss << "darcy";
        else
            oss << "forchheimer";
        return oss.str();
    }

    /*!
     * \copydoc FvBaseProblem::endTimeStep
     */
    void endTimeStep()
    {
#ifndef NDEBUG
        this->model().checkConservativeness();

        // Calculate storage terms
        EqVector storage;
        this->model().globalStorage(storage);

        // Write mass balance information for rank 0
        if (this->gridView().comm().rank() == 0) {
            std::cout << "Storage: " << storage << std::endl << std::flush;
        }
#endif // NDEBUG
    }
    //! \}

    /*!
     * \name Soil parameters
     */
    //! \{

    /*!
     * \copydoc FvBaseMultiPhaseProblem::intrinsicPermeability
     */
    template <class Context>
    const DimMatrix &intrinsicPermeability(const Context &context, unsigned spaceIdx,
                                           unsigned timeIdx) const
    { return K_; }

    /*!
     * \copydoc ForchheimerBaseProblem::ergunCoefficient
     */
    template <class Context>
    Scalar ergunCoefficient(const Context &context, unsigned spaceIdx,
                            unsigned timeIdx) const
    { return 0.3866; }

    /*!
     * \copydoc FvBaseMultiPhaseProblem::porosity
     */
    template <class Context>
    Scalar porosity(const Context &context, unsigned spaceIdx, unsigned timeIdx) const
    { return 0.558; }

    /*!
     * \copydoc FvBaseMultiPhaseProblem::materialLawParams
     */
    template <class Context>
    const MaterialLawParams &materialLawParams(const Context &context,
                                               unsigned spaceIdx, unsigned timeIdx) const
    { return materialParams_; }

    /*!
     * \copydoc FvBaseMultiPhaseProblem::temperature
     */
    template <class Context>
    Scalar temperature(const Context &context, unsigned spaceIdx, unsigned timeIdx) const
    { return temperature_; }

    //! \}

    /*!
     * \name Boundary conditions
     */
    //! \{

    /*!
     * \copydoc FvBaseProblem::boundary
     *
     * This problem sets a very high injection rate of nitrogen on the
     * left and a free-flow boundary on the right.
     */
    template <class Context>
    void boundary(BoundaryRateVector &values, const Context &context,
                  unsigned spaceIdx, unsigned timeIdx) const
    {
        const GlobalPosition &pos = context.pos(spaceIdx, timeIdx);

        if (onLeftBoundary_(pos)) {
            RateVector massRate(0.0);
            massRate = 0.0;
            massRate[contiNEqIdx] = -1.00; // kg / (m^2 * s)

            // impose a forced flow boundary
            values.setMassRate(massRate);
        }
        else if (onRightBoundary_(pos))
            // free flow boundary with initial condition on the right
            values.setFreeFlow(context, spaceIdx, timeIdx, initialFluidState_);
        else
            values.setNoFlow();
    }

    //! \}

    /*!
     * \name Volumetric terms
     */
    //! \{

    /*!
     * \copydoc FvBaseProblem::initial
     */
    template <class Context>
    void initial(PrimaryVariables &values, const Context &context, unsigned spaceIdx,
                 unsigned timeIdx) const
    {
        // assign the primary variables
        values.assignNaive(initialFluidState_);
    }

    /*!
     * \copydoc FvBaseProblem::source
     *
     * For this problem, the source term of all components is 0
     * everywhere.
     */
    template <class Context>
    void source(RateVector &rate, const Context &context, unsigned spaceIdx,
                unsigned timeIdx) const
    { rate = Scalar(0.0); }

    //! \}

private:
    bool onLeftBoundary_(const GlobalPosition &pos) const
    { return pos[0] < this->boundingBoxMin()[0] + eps_; }

    bool onRightBoundary_(const GlobalPosition &pos) const
    { return pos[0] > this->boundingBoxMax()[0] - eps_; }

    void setupInitialFluidState_()
    {
        initialFluidState_.setTemperature(temperature_);

        Scalar Sw = 1.0;
        initialFluidState_.setSaturation(wettingPhaseIdx, Sw);
        initialFluidState_.setSaturation(nonWettingPhaseIdx, 1 - Sw);

        Scalar p = 1e5;
        initialFluidState_.setPressure(wettingPhaseIdx, p);
        initialFluidState_.setPressure(nonWettingPhaseIdx, p);
    }

    DimMatrix K_;
    MaterialLawParams materialParams_;

    Opm::ImmiscibleFluidState<Scalar, FluidSystem> initialFluidState_;
    Scalar temperature_;
    Scalar eps_;
};

} // namespace Ewoms

#endif
