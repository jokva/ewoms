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
 * \copydoc Ewoms::StokesExtensiveQuantities
 */
#ifndef EWOMS_STOKES_EXTENSIVE_QUANTITIES_HH
#define EWOMS_STOKES_EXTENSIVE_QUANTITIES_HH

#include "stokesproperties.hh"

#include <ewoms/models/common/energymodule.hh>
#include <ewoms/models/common/quantitycallbacks.hh>

#include <opm/material/common/Valgrind.hpp>

#include <dune/common/fvector.hh>

namespace Ewoms {

/*!
 * \ingroup StokesModel
 * \ingroup ExtensiveQuantities
 *
 * \brief Contains the data which is required to calculate the mass and momentum fluxes
 *        over the face of a sub-control-volume for the Stokes model.
 *
 * This means pressure gradients, phase densities, viscosities, etc.
 * at the integration point of the sub-control-volume face
 */
template <class TypeTag>
class StokesExtensiveQuantities
    : public EnergyExtensiveQuantities<TypeTag, GET_PROP_VALUE(TypeTag, EnableEnergy)>
{
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, GridView) GridView;
    typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;
    typedef typename GET_PROP_TYPE(TypeTag, ElementContext) ElementContext;

    enum { dimWorld = GridView::dimensionworld };
    enum { phaseIdx = GET_PROP_VALUE(TypeTag, StokesPhaseIndex) };
    enum { enableEnergy = GET_PROP_VALUE(TypeTag, EnableEnergy) };

    typedef Dune::FieldVector<Scalar, dimWorld> DimVector;
    typedef Ewoms::EnergyExtensiveQuantities<TypeTag, enableEnergy> EnergyExtensiveQuantities;

public:
    /*!
     * \brief Register all run-time parameters for the extensive quantities.
     */
    static void registerParameters()
    {}

    /*!
     * \brief Update all quantities which are required on an intersection between two
     *        finite volumes.
     *
     * \param elemCtx The current execution context.
     * \param scvfIdx The local index of the sub-control volume face.
     * \param timeIdx The index relevant for the time discretization.
     * \param isBoundaryFace Specifies whether the sub-control-volume face is on the
     *                       domain boundary or not.
     */
    void update(const ElementContext &elemCtx, int scvfIdx, int timeIdx, bool isBoundaryFace=false)
    {
        const auto &stencil = elemCtx.stencil(timeIdx);
        const auto &scvf =
            isBoundaryFace?
            stencil.boundaryFace(scvfIdx):
            stencil.interiorFace(scvfIdx);

        insideIdx_ = scvf.interiorIndex();
        outsideIdx_ = scvf.exteriorIndex();

        onBoundary_ = isBoundaryFace;
        normal_ = scvf.normal();
        Valgrind::CheckDefined(normal_);

        // calculate gradients and secondary variables at IPs
        const auto& gradCalc = elemCtx.gradientCalculator();
        PressureCallback<TypeTag> pressureCallback(elemCtx, phaseIdx);
        DensityCallback<TypeTag> densityCallback(elemCtx, phaseIdx);
        MolarDensityCallback<TypeTag> molarDensityCallback(elemCtx, phaseIdx);
        ViscosityCallback<TypeTag> viscosityCallback(elemCtx, phaseIdx);
        VelocityCallback<TypeTag> velocityCallback(elemCtx);
        VelocityComponentCallback<TypeTag> velocityComponentCallback(elemCtx);

        pressure_ = gradCalc.calculateValue(elemCtx, scvfIdx, pressureCallback);
        gradCalc.calculateGradient(pressureGrad_, elemCtx, scvfIdx, pressureCallback);
        density_ = gradCalc.calculateValue(elemCtx, scvfIdx, densityCallback);
        molarDensity_ = gradCalc.calculateValue(elemCtx, scvfIdx, molarDensityCallback);
        viscosity_ = gradCalc.calculateValue(elemCtx, scvfIdx, viscosityCallback);
        velocity_ = gradCalc.calculateValue(elemCtx, scvfIdx, velocityCallback);

        for (int dimIdx = 0; dimIdx < dimWorld; ++dimIdx) {
            velocityComponentCallback.setDimIndex(dimIdx);
            gradCalc.calculateGradient(velocityGrad_[dimIdx],
                                       elemCtx,
                                       scvfIdx,
                                       velocityComponentCallback);
        }

        volumeFlux_ = (velocity_ * normal_);
        Valgrind::CheckDefined(volumeFlux_);

        // set the upstream and downstream DOFs
        upstreamIdx_ = scvf.interiorIndex();
        downstreamIdx_ = scvf.exteriorIndex();
        if (volumeFlux_ < 0)
            std::swap(upstreamIdx_, downstreamIdx_);

        EnergyExtensiveQuantities::update_(elemCtx, scvfIdx, timeIdx);

        Valgrind::CheckDefined(density_);
        Valgrind::CheckDefined(viscosity_);
        Valgrind::CheckDefined(velocity_);
        Valgrind::CheckDefined(pressureGrad_);
        Valgrind::CheckDefined(velocityGrad_);
    }

    /*!
     * \copydoc MultiPhaseBaseExtensiveQuantities::updateBoundary
     */
    template <class Context, class FluidState>
    void updateBoundary(const Context &context, int bfIdx, int timeIdx,
                        const FluidState &fluidState,
                        typename FluidSystem::template ParameterCache<typename FluidState::Scalar> &paramCache)
    {
        update(context, bfIdx, timeIdx, fluidState, paramCache,
               /*isOnBoundary=*/true);
    }

    /*!
     * \brief Return the pressure \f$\mathrm{[Pa]}\f$ at the integration
     *        point.
     */
    Scalar pressure() const
    { return pressure_; }

    /*!
     * \brief Return the mass density \f$ \mathrm{[kg/m^3]} \f$ at the
     * integration
     *        point.
     */
    Scalar density() const
    { return density_; }

    /*!
     * \brief Return the molar density \f$ \mathrm{[mol/m^3]} \f$ at the
     * integration point.
     */
    Scalar molarDensity() const
    { return molarDensity_; }

    /*!
     * \brief Return the viscosity \f$ \mathrm{[m^2/s]} \f$ at the integration
     *        point.
     */
    Scalar viscosity() const
    { return viscosity_; }

    /*!
     * \brief Return the pressure gradient at the integration point.
     */
    const DimVector &pressureGrad() const
    { return pressureGrad_; }

    /*!
     * \brief Return the velocity vector at the integration point.
     */
    const DimVector &velocity() const
    { return velocity_; }

    /*!
     * \brief Return the velocity gradient at the integration
     *        point of a face.
     */
    const DimVector &velocityGrad(int axisIdx) const
    { return velocityGrad_[axisIdx]; }

    /*!
     * \brief Return the eddy viscosity (if implemented).
     */
    Scalar eddyViscosity() const
    { return 0; }

    /*!
    * \brief Return the eddy diffusivity (if implemented).
    */
    Scalar eddyDiffusivity() const
    { return 0; }

    /*!
     * \brief Return the volume flux of mass
     */
    Scalar volumeFlux(int phaseIdx) const
    { return volumeFlux_; }

    /*!
     * \brief Return the weight of the upstream index
     */
    Scalar upstreamWeight(int phaseIdx) const
    { return 1.0; }

    /*!
     * \brief Return the weight of the downstream index
     */
    Scalar downstreamWeight(int phaseIdx) const
    { return 0.0; }

    /*!
     * \brief Return the local index of the upstream sub-control volume.
     */
    int upstreamIndex(int phaseIdx) const
    { return upstreamIdx_; }

    /*!
     * \brief Return the local index of the downstream sub-control volume.
     */
    int downstreamIndex(int phaseIdx) const
    { return downstreamIdx_; }

    /*!
     * \brief Return the local index of the sub-control volume which is located
     * in negative normal direction.
     */
    int interiorIndex() const
    { return insideIdx_; }

    /*!
     * \brief Return the local index of the sub-control volume which is located
     * in negative normal direction.
     */
    int exteriorIndex() const
    { return outsideIdx_; }

    /*!
     * \brief Indicates if a face is on a boundary. Used for in the
     *        face() method (e.g. for outflow boundary conditions).
     */
    bool onBoundary() const
    { return onBoundary_; }

    /*!
     * \brief Returns the extrusionFactor of the face.
     */
    Scalar extrusionFactor() const
    { return 1.0; }

    /*!
     * \brief Returns normal vector of the face of the extensive quantities.
     */
    const DimVector &normal() const
    { return normal_; }

private:
    bool onBoundary_;

    // values at the integration point
    Scalar density_;
    Scalar molarDensity_;
    Scalar viscosity_;
    Scalar pressure_;
    Scalar volumeFlux_;
    DimVector velocity_;
    DimVector normal_;

    // gradients at the IPs
    DimVector pressureGrad_;
    DimVector velocityGrad_[dimWorld];

    // local index of the upstream dof
    int upstreamIdx_;
    // local index of the downstream dof
    int downstreamIdx_;

    int insideIdx_;
    int outsideIdx_;
};

} // namespace Ewoms

#endif
