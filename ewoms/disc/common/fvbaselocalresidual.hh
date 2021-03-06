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
 * \copydoc Ewoms::FvBaseLocalResidual
 */
#ifndef EWOMS_FV_BASE_LOCAL_RESIDUAL_HH
#define EWOMS_FV_BASE_LOCAL_RESIDUAL_HH

#include "fvbaseproperties.hh"

#include <ewoms/common/parametersystem.hh>
#include <ewoms/common/alignedallocator.hh>

#include <opm/material/common/Valgrind.hpp>

#include <dune/istl/bvector.hh>
#include <dune/grid/common/geometry.hh>

#include <dune/common/fvector.hh>

#include <opm/material/common/ClassName.hpp>

#include <cmath>

namespace Ewoms {
/*!
 * \ingroup FiniteVolumeDiscretizations
 *
 * \brief Element-wise caculation of the residual matrix for models based on a finite
 *        volume spatial discretization.
 *
 * \copydetails Doxygen::typeTagTParam
 */
template<class TypeTag>
class FvBaseLocalResidual
{
private:
    typedef typename GET_PROP_TYPE(TypeTag, LocalResidual) Implementation;

    typedef typename GET_PROP_TYPE(TypeTag, GridView) GridView;
    typedef typename GridView::template Codim<0>::Entity Element;

    typedef typename GET_PROP_TYPE(TypeTag, Problem) Problem;
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, Evaluation) Evaluation;
    typedef typename GET_PROP_TYPE(TypeTag, BoundaryRateVector) BoundaryRateVector;
    typedef typename GET_PROP_TYPE(TypeTag, RateVector) RateVector;
    typedef typename GET_PROP_TYPE(TypeTag, EqVector) EqVector;
    typedef typename GET_PROP_TYPE(TypeTag, PrimaryVariables) PrimaryVariables;
    enum { numEq = GET_PROP_VALUE(TypeTag, NumEq) };

    typedef typename GET_PROP_TYPE(TypeTag, ElementContext) ElementContext;
    typedef typename GET_PROP_TYPE(TypeTag, BoundaryContext) BoundaryContext;

    typedef Opm::MathToolbox<Evaluation> Toolbox;
    typedef Dune::FieldVector<Evaluation, numEq> EvalVector;

    // copying the local residual class is not a good idea
    FvBaseLocalResidual(const FvBaseLocalResidual &)
    {}

public:
    typedef Dune::BlockVector<EvalVector, Ewoms::aligned_allocator<EvalVector, alignof(EvalVector)> > LocalEvalBlockVector;

    FvBaseLocalResidual()
    { }

    ~FvBaseLocalResidual()
    { }

    /*!
     * \brief Register all run-time parameters for the local residual.
     */
    static void registerParameters()
    { }

    /*!
     * \brief Return the result of the eval() call using internal
     *        storage.
     */
    const LocalEvalBlockVector &residual() const
    { return internalResidual_; }

    /*!
     * \brief Return the result of the eval() call using internal
     *        storage.
     *
     * \copydetails Doxygen::ecfvScvIdxParam
     */
    const EvalVector &residual(unsigned dofIdx) const
    { return internalResidual_[dofIdx]; }

    /*!
     * \brief Compute the local residual, i.e. the deviation of the
     *        conservation equations from zero and store the results
     *        internally.
     *
     * The results can be requested afterwards using the residual() method.
     *
     * \copydetails Doxygen::problemParam
     * \copydetails Doxygen::elementParam
     */
    void eval(const Problem &problem, const Element &element)
    {
        ElementContext elemCtx(problem);
        elemCtx.updateAll(element);
        eval(elemCtx);
    }

    /*!
     * \brief Compute the local residual, i.e. the deviation of the
     *        conservation equations from zero and store the results
     *        internally.
     *
     * The results can be requested afterwards using the residual() method.
     *
     * \copydetails Doxygen::ecfvElemCtxParam
     */
    void eval(const ElementContext &elemCtx)
    {
        unsigned numDof = elemCtx.numDof(/*timeIdx=*/0);
        internalResidual_.resize(numDof);
        asImp_().eval(internalResidual_, elemCtx);
    }

    /*!
     * \brief Compute the local residual, i.e. the deviation of the
     *        conservation equations from zero.
     *
     * \copydetails Doxygen::residualParam
     * \copydetails Doxygen::ecfvElemCtxParam
     */
    void eval(LocalEvalBlockVector &residual,
              const ElementContext &elemCtx) const
    {
        assert(residual.size() == elemCtx.numDof(/*timeIdx=*/0));

        residual = 0.0;

        // evaluate the flux terms
        asImp_().evalFluxes(residual, elemCtx, /*timeIdx=*/0);

        // evaluate the storage and the source terms
        asImp_().evalVolumeTerms_(residual, elemCtx);

        // evaluate the boundary conditions
        asImp_().evalBoundary_(residual, elemCtx, /*timeIdx=*/0);

        // make the residual volume specific (i.e., make it incorrect mass per cubic
        // meter instead of total mass)
        unsigned numDof = elemCtx.numDof(/*timeIdx=*/0);
        for (unsigned dofIdx=0; dofIdx < numDof; ++dofIdx) {
            if (elemCtx.dofTotalVolume(dofIdx, /*timeIdx=*/0) > 0) {
                // interior DOF
                Scalar dofVolume = elemCtx.dofTotalVolume(dofIdx, /*timeIdx=*/0);

                assert(std::isfinite(dofVolume));
                Valgrind::CheckDefined(dofVolume);

                for (unsigned eqIdx = 0; eqIdx < numEq; ++ eqIdx)
                    residual[dofIdx][eqIdx] /= dofVolume;
            }
        }
    }

    /*!
     * \brief Calculate the amount of all conservation quantities stored in all element's
     *        sub-control volumes for a given history index.
     *
     * This is used to figure out how much of each conservation quantity is inside the
     * element.
     *
     * \copydetails Doxygen::storageParam
     * \copydetails Doxygen::ecfvElemCtxParam
     * \copydetails Doxygen::timeIdxParam
     */
    void evalStorage(LocalEvalBlockVector &storage,
                     const ElementContext &elemCtx,
                     unsigned timeIdx) const
    {
        if (timeIdx == 0) {
            // for the most current solution, the storage term depends on the current
            // primary variables

            // calculate the amount of conservation each quantity inside
            // all primary sub control volumes
            unsigned numPrimaryDof = elemCtx.numPrimaryDof(/*timeIdx=*/0);
            for (unsigned dofIdx=0; dofIdx < numPrimaryDof; dofIdx++) {
                storage[dofIdx] = 0.0;
                asImp_().computeStorage(storage[dofIdx], elemCtx, dofIdx, timeIdx);

                // the volume of the associated DOF
                Scalar alpha =
                    elemCtx.stencil(timeIdx).subControlVolume(dofIdx).volume()
                    * elemCtx.intensiveQuantities(dofIdx, timeIdx).extrusionFactor();
                for (unsigned eqIdx = 0; eqIdx < numEq; ++ eqIdx)
                    storage[dofIdx][eqIdx] *= alpha;
            }
        }
        else {
            // for all previous solutions, the storage term does _not_ depend on the
            // current primary variables, so we use scalars to store it.
            if (elemCtx.enableStorageCache()) {
                unsigned numPrimaryDof = elemCtx.numPrimaryDof(timeIdx);
                for (unsigned dofIdx=0; dofIdx < numPrimaryDof; dofIdx++) {
                    unsigned globalDofIdx = elemCtx.globalSpaceIndex(dofIdx, timeIdx);
                    const auto& cachedStorage = elemCtx.model().cachedStorage(globalDofIdx, timeIdx);
                    for (unsigned eqIdx=0; eqIdx < numEq; eqIdx++)
                        storage[dofIdx][eqIdx] = cachedStorage[eqIdx];
                }
            }
            else {
                // calculate the amount of conservation each quantity inside
                // all primary sub control volumes
                Dune::FieldVector<Scalar, numEq> tmp;
                unsigned numPrimaryDof = elemCtx.numPrimaryDof(/*timeIdx=*/0);
                for (unsigned dofIdx=0; dofIdx < numPrimaryDof; dofIdx++) {
                    tmp = 0.0;
                    asImp_().computeStorage(tmp,
                                            elemCtx,
                                            dofIdx,
                                            timeIdx);
                    tmp *=
                        elemCtx.stencil(timeIdx).subControlVolume(dofIdx).volume()
                        * elemCtx.intensiveQuantities(dofIdx, timeIdx).extrusionFactor();

                    for (unsigned eqIdx = 0; eqIdx < numEq; ++eqIdx)
                        storage[dofIdx][eqIdx] = tmp[eqIdx];
                }
            }
        }
    }

    /*!
     * \brief Add the flux term to a local residual.
     *
     * \copydetails Doxygen::residualParam
     * \copydetails Doxygen::ecfvElemCtxParam
     * \copydetails Doxygen::timeIdxParam
     */
    void evalFluxes(LocalEvalBlockVector &residual,
                    const ElementContext &elemCtx,
                    unsigned timeIdx) const
    {
        RateVector flux;

        const auto &stencil = elemCtx.stencil(timeIdx);
        // calculate the mass flux over the sub-control volume faces
        unsigned numInteriorFaces = elemCtx.numInteriorFaces(timeIdx);
        for (unsigned scvfIdx = 0; scvfIdx < numInteriorFaces; scvfIdx++) {
            const auto &face = stencil.interiorFace(scvfIdx);
            unsigned i = face.interiorIndex();
            unsigned j = face.exteriorIndex();

            Valgrind::SetUndefined(flux);
            asImp_().computeFlux(flux, /*context=*/elemCtx, scvfIdx, timeIdx);
            Valgrind::CheckDefined(flux);

            Scalar alpha = elemCtx.extensiveQuantities(scvfIdx, timeIdx).extrusionFactor();
            alpha *= face.area();
            Valgrind::CheckDefined(alpha);
            for (unsigned eqIdx = 0; eqIdx < numEq; ++ eqIdx)
                flux[eqIdx] *= alpha;

            // The balance equation for a finite volume is given by
            //
            // dStorage/dt + Flux = Source
            //
            // where the 'Flux' and the 'Source' terms represent the
            // mass per second which leaves the finite
            // volume. Re-arranging this, we get
            //
            // dStorage/dt + Flux - Source = 0
            //
            // Since the mass flux as calculated by computeFlux() goes out of sub-control
            // volume i and into sub-control volume j, we need to add the flux to finite
            // volume i and subtract it from finite volume j
            for (unsigned eqIdx = 0; eqIdx < numEq; ++eqIdx) {
                residual[i][eqIdx] += flux[eqIdx];
                residual[j][eqIdx] -= flux[eqIdx];
            }
        }

#if !defined NDEBUG
        // in debug mode, ensure that the residual is well-defined
        unsigned numDof = elemCtx.numDof(timeIdx);
        for (unsigned i=0; i < numDof; i++) {
            for (unsigned j = 0; j < numEq; ++ j) {
                assert(std::isfinite(Toolbox::value(residual[i][j])));
                Valgrind::CheckDefined(residual[i][j]);
            }
        }
#endif

    }

    /////////////////////////////
    // The following methods _must_ be overloaded by the actual flow
    // models!
    /////////////////////////////

    /*!
     * \brief Evaluate the amount all conservation quantities
     *        (e.g. phase mass) within a finite sub-control volume.
     *
     * \copydetails Doxygen::storageParam
     * \copydetails Doxygen::ecfvScvCtxParams
     */
    void computeStorage(EqVector &storage,
                        const ElementContext &elemCtx,
                        unsigned dofIdx,
                        unsigned timeIdx) const
    {
        OPM_THROW(std::logic_error,
                   "Not implemented: The local residual " << Opm::className<Implementation>()
                   << " does not implement the required method 'computeStorage()'");
    }

    /*!
     * \brief Evaluates the total mass flux of all conservation
     *        quantities over a face of a sub-control volume.
     *
     * \copydetails Doxygen::areaFluxParam
     * \copydetails Doxygen::ecfvScvfCtxParams
     */
    void computeFlux(RateVector &flux,
                     const ElementContext &elemCtx,
                     unsigned scvfIdx,
                     unsigned timeIdx) const
    {
        OPM_THROW(std::logic_error,
                  "Not implemented: The local residual " << Opm::className<Implementation>()
                  << " does not implement the required method 'computeFlux()'");
    }

    /*!
     * \brief Calculate the source term of the equation
     *
     * \copydoc Doxygen::sourceParam
     * \copydoc Doxygen::ecfvScvCtxParams
     */
    void computeSource(RateVector &source,
                       const ElementContext &elemCtx,
                       unsigned dofIdx,
                       unsigned timeIdx) const
    {
        OPM_THROW(std::logic_error,
                  "Not implemented: The local residual " << Opm::className<Implementation>()
                  << " does not implement the required method 'computeSource()'");
    }

protected:
    /*!
     * \brief Evaluate the boundary conditions of an element.
     */
    void evalBoundary_(LocalEvalBlockVector &residual,
                       const ElementContext &elemCtx,
                       unsigned timeIdx) const
    {
        if (!elemCtx.onBoundary())
            return;

        BoundaryContext boundaryCtx(elemCtx);

        // evaluate the boundary for all boundary faces of the current context
        unsigned numBoundaryFaces = boundaryCtx.numBoundaryFaces(/*timeIdx=*/0);
        for (unsigned faceIdx = 0; faceIdx < numBoundaryFaces; ++faceIdx) {
            // add the residual of all vertices of the boundary
            // segment
            evalBoundarySegment_(residual,
                                 boundaryCtx,
                                 faceIdx,
                                 timeIdx);
        }

#if !defined NDEBUG
        // in debug mode, ensure that the residual and the storage terms are well-defined
        unsigned numDof = elemCtx.numDof(/*timeIdx=*/0);
        for (unsigned i=0; i < numDof; i++) {
            for (unsigned j = 0; j < numEq; ++ j) {
                assert(std::isfinite(Toolbox::value(residual[i][j])));
                Valgrind::CheckDefined(residual[i][j]);
            }
        }
#endif

    }

    /*!
     * \brief Evaluate all boundary conditions for a single
     *        sub-control volume face to the local residual.
     */
    void evalBoundarySegment_(LocalEvalBlockVector &residual,
                              const BoundaryContext &boundaryCtx,
                              unsigned boundaryFaceIdx,
                              unsigned timeIdx) const
    {
        BoundaryRateVector values;

        Valgrind::SetUndefined(values);
        boundaryCtx.problem().boundary(values,
                                       boundaryCtx,
                                       boundaryFaceIdx,
                                       timeIdx);
        Valgrind::CheckDefined(values);

        const auto &stencil = boundaryCtx.stencil(timeIdx);
        unsigned dofIdx = stencil.boundaryFace(boundaryFaceIdx).interiorIndex();
        const auto &insideIntQuants = boundaryCtx.elementContext().intensiveQuantities(dofIdx, timeIdx);
        for (unsigned eqIdx = 0; eqIdx < values.size(); ++eqIdx)
            values[eqIdx] *=
                stencil.boundaryFace(boundaryFaceIdx).area()
                * insideIntQuants.extrusionFactor();

        for (unsigned eqIdx = 0; eqIdx < numEq; ++eqIdx) {
            residual[dofIdx][eqIdx] += values[eqIdx];
        }
    }

    /*!
     * \brief Add the change in the storage terms and the source term
     *        to the local residual of all sub-control volumes of the
     *        current element.
     */
    void evalVolumeTerms_(LocalEvalBlockVector &residual,
                          const ElementContext &elemCtx) const
    {
        EvalVector tmp;
        EqVector tmp2;
        RateVector sourceRate;

        tmp = 0.0;
        tmp2 = 0.0;

        // evaluate the volumetric terms (storage + source terms)
        unsigned numPrimaryDof = elemCtx.numPrimaryDof(/*timeIdx=*/0);
        for (unsigned dofIdx=0; dofIdx < numPrimaryDof; dofIdx++) {
            Scalar extrusionFactor =
                elemCtx.intensiveQuantities(dofIdx, /*timeIdx=*/0).extrusionFactor();
            Scalar scvVolume =
               elemCtx.stencil(/*timeIdx=*/0).subControlVolume(dofIdx).volume() * extrusionFactor;
            Valgrind::CheckDefined(scvVolume);

            // mass balance within the element. this is the \f$\frac{m}{\partial t}\f$
            // term if using implicit euler as time discretization.
            //
            // TODO (?): we might need a more explicit way for doing the time
            // discretization...
            asImp_().computeStorage(tmp,
                                    elemCtx,
                                    dofIdx,
                                    /*timeIdx=*/0);
            Valgrind::CheckDefined(tmp);

            if (elemCtx.enableStorageCache()) {
                const auto& model = elemCtx.model();
                unsigned globalDofIdx = elemCtx.globalSpaceIndex(dofIdx, /*timeIdx=*/0);
                if (model.newtonMethod().numIterations() == 0 &&
                    !elemCtx.haveStashedIntensiveQuantities())
                {
                    // if the storage term is cached and we're in the first iteration of
                    // the time step, update the cache of the storage term (this assumes
                    // that the initial guess for the solution at the end of the time
                    // step is the same as the solution at the beginning of the time
                    // step. This is usually true, but some fancy preprocessing scheme
                    // might invalidate that assumption.)
                    for (unsigned eqIdx = 0; eqIdx < numEq; ++ eqIdx)
                        tmp2[eqIdx] = Toolbox::value(tmp[eqIdx]);
                    Valgrind::CheckDefined(tmp2);

                    model.updateCachedStorage(globalDofIdx, /*timeIdx=*/1, tmp2);
                }
                else {
                    // if the storage term is cached and we're not looking at the first
                    // iteration of the time step, we take the cached data.
                    tmp2 = model.cachedStorage(globalDofIdx, /*timeIdx=*/1);
                    Valgrind::CheckDefined(tmp2);
                }
            }
            else {
                // if the mass storage at the beginning of the time step is not cached,
                // we re-calculate it from scratch.
                tmp2 = 0.0;
                asImp_().computeStorage(tmp2,
                                        elemCtx,
                                        dofIdx,
                                        /*timeIdx=*/1);
                Valgrind::CheckDefined(tmp2);
            }

            for (unsigned eqIdx = 0; eqIdx < numEq; ++eqIdx) {
                tmp[eqIdx] -= tmp2[eqIdx];
                tmp[eqIdx] *= scvVolume / elemCtx.simulator().timeStepSize();

                residual[dofIdx][eqIdx] += tmp[eqIdx];
            }

            Valgrind::CheckDefined(residual[dofIdx]);

            // deal with the source term
            asImp_().computeSource(sourceRate, elemCtx, dofIdx, /*timeIdx=*/0);
            for (unsigned eqIdx = 0; eqIdx < numEq; ++eqIdx) {
                sourceRate[eqIdx] *= scvVolume;
                residual[dofIdx][eqIdx] -= sourceRate[eqIdx];
            }

            Valgrind::CheckDefined(residual[dofIdx]);
        }

#if !defined NDEBUG
        // in debug mode, ensure that the residual is well-defined
        unsigned numDof = elemCtx.numDof(/*timeIdx=*/0);
        for (unsigned i=0; i < numDof; i++) {
            for (unsigned j = 0; j < numEq; ++ j) {
                assert(std::isfinite(Toolbox::value(residual[i][j])));
                Valgrind::CheckDefined(residual[i][j]);
            }
        }
#endif
    }


private:
    Implementation &asImp_()
    { return *static_cast<Implementation*>(this); }

    const Implementation &asImp_() const
    { return *static_cast<const Implementation*>(this); }

    LocalEvalBlockVector internalResidual_;
};

} // namespace Ewoms

#endif
