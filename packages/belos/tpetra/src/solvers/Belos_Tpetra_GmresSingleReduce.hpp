#ifndef BELOS_TPETRA_GMRES_SINGLE_REDUCE_HPP
#define BELOS_TPETRA_GMRES_SINGLE_REDUCE_HPP
#define HAVE_TPETRA_DEBUG

#include "Belos_Tpetra_Gmres.hpp"
#include "Belos_Tpetra_UpdateNewton.hpp"

namespace BelosTpetra {
namespace Impl {

template<class SC = Tpetra::Operator<>::scalar_type,
         class MV = Tpetra::MultiVector<SC>,
         class OP = Tpetra::Operator<SC>>
class GmresSingleReduce : public Gmres<SC, MV, OP> {
private:
  using base_type = Gmres<SC, MV, OP>;
  using MVT = Belos::MultiVecTraits<SC, MV>;
  using LO = typename MV::local_ordinal_type;
  using STS = Teuchos::ScalarTraits<SC>;
  using real_type = typename STS::magnitudeType;
  using STM = Teuchos::ScalarTraits<real_type>;
  using complex_type = std::complex<real_type>;
  using dense_matrix_type = Teuchos::SerialDenseMatrix<LO, SC>;
  using dense_vector_type = Teuchos::SerialDenseVector<LO, SC>;
  using vec_type = typename Krylov<SC, MV, OP>::vec_type;

public:
  GmresSingleReduce () :
    base_type::Gmres (),
    stepSize_ (1)
  {
    this->input_.computeRitzValues = true;
  }

  GmresSingleReduce (const Teuchos::RCP<const OP>& A) :
    base_type::Gmres (A),
    stepSize_ (1)
  {
    this->input_.computeRitzValues = true;
  }

  virtual ~GmresSingleReduce () = default;

  virtual void
  setParameters (Teuchos::ParameterList& params) {
    Gmres<SC, MV, OP>::setParameters (params);

    int stepSize = params.get<int> ("Step Size", stepSize_);
    stepSize_ = stepSize;
  }

  void
  setStepSize(int stepSize) {
    stepSize_ = stepSize;
  }

  int
  getStepSize() {
    return stepSize_;
  }

protected:
  virtual void
  setOrthogonalizer (const std::string& ortho)
  {
    if (ortho == "MGS" || ortho == "CGS" || ortho == "CGS2") {
      this->input_.orthoType = ortho;
    } else {
      this->input_.orthoType = "CGS";
      //base_type::setOrthogonalizer (ortho);
    }
  }

private:
  //! Cleanup the orthogonalization on the "last" basis vector
  int
  projectAndNormalizeSingleReduce_cleanup (Teuchos::FancyOStream* outPtr,
                                           int n,
                                           const SolverInput<SC>& input, 
                                           MV& Q,
                                           dense_matrix_type& H,
                                           dense_matrix_type& WORK) const
  {
    const real_type eps = STS::eps ();
    const real_type tolFactor = static_cast<real_type> (0.0);
    const real_type tolOrtho  = tolFactor * eps;
    const SC zero = STS::zero ();
    const SC one  = STS::one ();

    Teuchos::RCP< Teuchos::Time > dotsTimer = Teuchos::TimeMonitor::getNewCounter ("GmresSingleReduce::LowSynch::dot-prod");

    int rank = 1;
   
    Teuchos::Range1D index_next (n, n);
    MV Qnext = * (Q.subView (index_next));

    Teuchos::Range1D index (0, n);
    Teuchos::RCP< const MV > Qi = MVT::CloneView (Q, index);
    
    Teuchos::RCP< dense_matrix_type > tj
           = Teuchos::rcp (new dense_matrix_type (Teuchos::View, WORK, n+1, 1, 0, n));
    
    {
    Teuchos::TimeMonitor LocalTimer (*dotsTimer);
    MVT::MvTransMv (one, *Qi, Qnext, *tj);
    }
    
    SC tmp = 0.0e+00;
    for( int i=0; i<n; i++ ){ tmp = tmp + WORK(i,n) * WORK(i,n); }
    H(n,n-1) = sqrt( WORK(n,n) - tmp );

    
    Teuchos::Range1D index_update (0, n-1);
    Teuchos::RCP< const MV > Qupdate = MVT::CloneView (Q, index_update);

    Teuchos::RCP< dense_matrix_type > tp
     = Teuchos::rcp (new dense_matrix_type (Teuchos::View, WORK, n, 1, 0, n));

    MVT::MvTimesMatAddMv( -one, *Qupdate, *tp, one, Qnext );
    MVT::MvScale( Qnext, ( one / H(n,n-1) ) );

    for( int i=0; i<n; i++){ H(i,n-1) = WORK(i,n-1) + WORK(i,n); }


    return rank;


  }



  //! Apply the orthogonalization using a single all-reduce
  int
  projectAndNormalizeSingleReduce (Teuchos::FancyOStream* outPtr,
                                   int n,
                                   const SolverInput<SC>& input, 
                                   MV& Q,
                                   dense_matrix_type& H,
                                   dense_matrix_type& WORK) const
  {
    Teuchos::RCP< Teuchos::Time > orthTimer = Teuchos::TimeMonitor::getNewCounter ("GmresSingleReduce::LowSynch::Ortho");
    Teuchos::TimeMonitor OrthTimer (*orthTimer);

    int rank = 0;
    if (input.orthoType == "CGS") {
      // default, one-synch CGS1, optionally with reortho
      rank = projectAndNormalizeSingleReduce_CGS1 (outPtr, n, input, Q, H, WORK);
    } else {
      // one-synch MGS or CGS2, optionally with delayed renorm
      rank = projectAndNormalizeSingleReduce_GS (outPtr, n, input, Q, H, WORK);
    }
    return rank;
  }


  //! MGS/CGS2 specialization
  int
  projectAndNormalizeSingleReduce_GS (Teuchos::FancyOStream* outPtr,
                                      int n,
                                      const SolverInput<SC>& input, 
                                      MV& Q,
                                      dense_matrix_type& H,
                                      dense_matrix_type& T) const
  {
    const SC zero = STS::zero ();
    const SC one  = STS::one  ();
    const SC two  = one + one;
    const real_type eps = STS::eps ();
    const real_type tolFactor = static_cast<real_type> (10.0);
    const real_type tolOrtho  = tolFactor * eps;

    Teuchos::RCP< Teuchos::Time > dotsTimer = Teuchos::TimeMonitor::getNewCounter ("GmresSingleReduce::LowSynch::dot-prod");
    Teuchos::RCP< Teuchos::Time > projTimer = Teuchos::TimeMonitor::getNewCounter ("GmresSingleReduce::LowSynch::project");

    int rank = 1;



// Case for starting the delayed orthogonalization step
if( n==0 ){

    Teuchos::Range1D index_prev (n, n);
    Teuchos::Range1D index_next (n+1, n+1);

    MV q = * (Q.subView (index_next));
    Teuchos::RCP< const MV > Qi = MVT::CloneView(Q, index_prev);

    Teuchos::RCP< dense_matrix_type > tj
      = Teuchos::rcp (new dense_matrix_type (Teuchos::View, T, 1, 1, 0, 0));

    {
      Teuchos::TimeMonitor LocalTimer (*dotsTimer);
      MVT::MvTransMv (one, *Qi, q, *tj);
    }
    {
    Teuchos::TimeMonitor LocalTimer (*projTimer);
    MVT::MvTimesMatAddMv( -one, *Qi, *tj, one, q );
    }

}

// The bulk of the algorithm
if( n>0 ){

    // Getting columns n and n+1
    Teuchos::Range1D index_next (n, n+1);
    MV Qnext = * (Q.subView (index_next));

    // Getting columns 0 : n
    Teuchos::Range1D index (0, n);
    Teuchos::RCP< const MV > Qi = MVT::CloneView (Q, index);

    // Working in T(0:n+1,n:n+1)
    Teuchos::RCP< dense_matrix_type > tj
      = Teuchos::rcp (new dense_matrix_type (Teuchos::View, T, n+1, 2, 0, n));

    // This operation is Q(1:m,1:n)' Q(1:m,n:n+1) = T(0:n+1,n:n+1) 
    {
      Teuchos::TimeMonitor LocalTimer (*dotsTimer);
      MVT::MvTransMv (one, *Qi, Qnext, *tj);
    }

    // Correction term alpha = sqrt( w'w - c'c ) where c = Q'w
    SC tmp=0.0e+00;
    for(int i = 0; i < n; i++){
       tmp = tmp + T(i,n) * T(i,n);
    }
    H(n,n-1) = sqrt( T(n,n) - tmp );

    // Second correction step, correcting inner product 
    tmp=0.0e+00;
    for(int i = 0; i < n; i++){
        tmp = tmp + T(i,n) * T(i,n+1);
    }
    T(n,n+1) = ( T(n,n+1) - tmp ) / ( H(n,n-1) * H(n,n-1) );

    // Third correction step, correcting using un-normalized vector
    for(int i = 0; i < n; i++){
        T(i,n+1) = T(i,n+1) / H(n,n-1);
    }

    // Clean-up for constructing previous q_j-1 and w_j
    Teuchos::Range1D index_update (0, n-1);
    Teuchos::Range1D index_Wj (n+1, n+1);
    Teuchos::Range1D index_qj (n, n);

    Teuchos::RCP< dense_matrix_type > tp
      = Teuchos::rcp (new dense_matrix_type (Teuchos::View, T, n, 2, 0, n));
                        
    Teuchos::RCP< const MV > Qupdate = MVT::CloneView (Q, index_update);

    MV Wj = * (Q.subView (index_Wj));                   
    MVT::MvScale( Wj, ( one / H(n,n-1) ) );

    MV Qj = * (Q.subView (index_next));
    {
        Teuchos::TimeMonitor LocalTimer (*projTimer);
        MVT::MvTimesMatAddMv( -one, *Qupdate, *tp, one, Qj );
    }
    MV qj = * (Q.subView (index_qj));
    MVT::MvScale( qj, ( one / H(n,n-1) ) );
                     
    MVT::MvAddMv( one, Wj, -T(n,n+1), qj, Wj );

    // Getting the previous column of H correct and setting up a workspace
    for(int i = 0; i < n; i++ ){
        H(i,n-1) = T(i,n-1) + T(i,n);
        T(i,n-1) = H(i,n-1);
    }
    T(n,n-1) = H(n,n-1);

    // Arnoldi repres trick
    dense_matrix_type Hnew (Teuchos::View, H,   n, n-1, 0, 0);
    dense_matrix_type Tcol (Teuchos::View, T,   n,   1, 0, n);
    dense_matrix_type work (Teuchos::View, H,   n,   1, 0, n);

    Teuchos::BLAS<LO ,SC> blas;
    blas.GEMV ( Teuchos::NO_TRANS, n, n-1, 
              one, Hnew.values(), Hnew.stride(),
                   Tcol.values(), Tcol.stride(),
             zero, work.values(), work.stride() 
    );    	

    // Step to update H correctly at the next iteration
    for(int i = 0; i < n+1; i++ ){
        T(i,n) = T(i,n+1) - ( H(i,n) / H(n,n-1) );
    }

}




/*
/// CGS2
if( n==0 ){
    
    std::vector<SC> dot(1);

    Teuchos::Range1D index_prev (0, 0);
    Teuchos::Range1D index_next (1, 1);

    MV q = * (Q.subView (index_next));
    Teuchos::RCP< const MV > Qi = MVT::CloneView(Q, index_prev);

    Teuchos::RCP< dense_matrix_type > tj
      = Teuchos::rcp (new dense_matrix_type (Teuchos::View, T, n+1, 1, 0, n));

    {
      Teuchos::TimeMonitor LocalTimer (*dotsTimer);
      MVT::MvTransMv (one, *Qi, q, *tj);
    }
    MVT::MvTimesMatAddMv( -one, *Qi, *tj, one, q );  

    H(0,0) = T(0,0);
    MVT::MvDot( q, q, dot );
    H(1,0) = sqrt( dot[0] );
    MVT::MvScale( q, ( one / H(1,0) ) );

}


if( n>0 ){

    std::vector<SC> dot(1);

    Teuchos::Range1D index_next (n+1, n+1);
    MV Qnext = * (Q.subView (index_next));

    Teuchos::Range1D index (0, n);
    Teuchos::RCP< const MV > Qi = MVT::CloneView (Q, index);

    Teuchos::RCP< dense_matrix_type > tj
      = Teuchos::rcp (new dense_matrix_type (Teuchos::View, T, n+1, 1, 0, n));

    {
      Teuchos::TimeMonitor LocalTimer (*dotsTimer);
      MVT::MvTransMv (one, *Qi, Qnext, *tj);
    }
    MVT::MvTimesMatAddMv( -one, *Qi, *tj, one, Qnext );  

    for(int i = 0; i < n+1; i++){
      H(i,n) = T(i,n);
    }

    {
      Teuchos::TimeMonitor LocalTimer (*dotsTimer);
      MVT::MvTransMv (one, *Qi, Qnext, *tj);
    }
    MVT::MvTimesMatAddMv( -one, *Qi, *tj, one, Qnext );  

    for(int i = 0; i < n+1; i++){
      H(i,n) += T(i,n);
    }

    MVT::MvDot( Qnext, Qnext, dot );    
    H(n+1,n) = sqrt( dot[0] );
    MVT::MvScale( Qnext, ( one / H(n+1,n) ) );

}




////// ICHI's MGS and CGS2 1-synch
*/
/*    // ----------------------------------------------------------
    // dot-product for single-reduce orthogonalization
    // ----------------------------------------------------------
    Teuchos::Range1D index_next (n, n+1);
    MV Qnext = * (Q.subView (index_next));

    // vectors to be orthogonalized against
    Teuchos::Range1D index (0, n+1);
    Teuchos::RCP< const MV > Qi = MVT::CloneView (Q, index);

    // compute coefficient, T(:,n:n+1) = Q(:,0:n+1)'*Q(n:n+1)
    Teuchos::RCP< dense_matrix_type > tj
      = Teuchos::rcp (new dense_matrix_type (Teuchos::View, T, n+2, 2, 0, n));
    {
      Teuchos::TimeMonitor LocalTimer (*dotsTimer);
      MVT::MvTransMv (one, *Qi, Qnext, *tj);
    }

    // ----------------------------------------------------------
    // lagged/delayed re-orthogonalization
    // ----------------------------------------------------------
    Teuchos::Range1D index_new (n+1, n+1);
    MV Qnew = * (Q.subView (index_new));
    if (input.needToReortho) {
      // check
      real_type prevNorm = STS::real (T(n, n));
      TEUCHOS_TEST_FOR_EXCEPTION
        (prevNorm < STM::zero (), std::runtime_error, "At iteration "
         << n << ", T(" << n << ", " << n << ") = " << T(n, n) << " < 0.");

      // lagged-normalize Q(:,n)
      SC Tnn = zero;
      real_type oldNorm (0.0);
      if (prevNorm > oldNorm * tolOrtho) {
        prevNorm = STM::squareroot (prevNorm);
        Tnn = SC {prevNorm};

        Teuchos::Range1D index_old (n, n);
        MV Qold = * (Q.subView (index_old));
        MVT::MvScale (Qold, one / Tnn); 

        // --------------------------------------
        // update coefficients after reortho
        for (int i = 0; i <= n; i++) {
          T (i, n) /= Tnn;
        }
        T(n, n) /= Tnn;
        T(n, n+1) /= Tnn;

        // --------------------------------------
        // lagged-normalize Q(:,n+1) := A*Q(:,n)
        MVT::MvScale (Qnew, one / Tnn);

        // update coefficients after reortho
        for (int i = 0; i <= n+1; i++) {
          T (i, n+1) /= Tnn;
        }
        T(n+1, n+1) /= Tnn;
      } else {
        if (outPtr != nullptr) {
          *outPtr << " > prevNorm = " << prevNorm << " -> T(" << n << ", " << n << ") = zero"
                  << ", tol = " << tolOrtho << " x oldNorm = " << oldNorm
                  << std::endl;
        }
      }
      // update coefficients after reortho
      if (n > 0) {
        H (n, n-1) *= Tnn;
      }
    }

    // ----------------------------------------------------------
    // comopute new coefficients (one-synch MGS/CGS2)
    // ----------------------------------------------------------
    // expand from triangular to full, conjugate (needed only for CGS2)
    for (int j = 0; j < n; j++) {
      T(n, j) = STS::conjugate(T(j, n));
    }

    // extract new coefficients 
    for (int i = 0; i <= n+1; i++) {
      H (i, n) = T (i, n+1);
    }

    // update new coefficients 
    Teuchos::BLAS<LO ,SC> blas;
    dense_matrix_type Hnew (Teuchos::View, H, n+1, 1, 0, n);
    if (input.orthoType == "MGS") {
      // H := (I+T)^(-T) H, where T is upper-triangular
      blas.TRSM (Teuchos::LEFT_SIDE, Teuchos::LOWER_TRI,
                 Teuchos::NO_TRANS, Teuchos::UNIT_DIAG,
                 n+1, 1,
                 one, T.values(), T.stride(),
                      Hnew.values(), Hnew.stride());
    } else {
      // H := (2*I-L) H, where T is full symmetrix Q'*Q
      dense_matrix_type Told (Teuchos::View, T, n+1, 1, 0, n+1);
      blas.COPY (n+1, Hnew.values(), 1, Told.values(), 1);
      blas.GEMV(Teuchos::NO_TRANS,
                n+1, n+1,
                -one, T.values(), T.stride(),
                      Told.values(), 1,
                 two, Hnew.values(), 1);
    }

    // ----------------------------------------------------------
    // orthogonalize the new vectors against the previous columns
    // ----------------------------------------------------------
    Teuchos::Range1D index_prev (0, n);
    Teuchos::RCP< const MV > Qprev = MVT::CloneView (Q, index_prev);
    {
      Teuchos::TimeMonitor LocalTimer (*projTimer);
      MVT::MvTimesMatAddMv (-one, *Qprev, Hnew, one, Qnew);
    }

    // ----------------------------------------------------------
    // normalize the new vector
    // ----------------------------------------------------------
    // fix the norm
    real_type oldNorm = STS::real (H(n+1, n));
    for (int i = 0; i <= n; ++i) {
      H(n+1, n) -= (H(i, n)*H(i, n));
    }

    real_type newNorm = STS::real (H(n+1, n));
    if (newNorm <= oldNorm * tolOrtho) {
      if (input.needToReortho) {
        // something might have gone wrong, and let re-norm take care of
        if (outPtr != nullptr) {
          *outPtr << " > newNorm = " << newNorm << " -> H(" << n+1 << ", " << n << ") = one"
                  << ", tol = " << tolOrtho << " x oldNorm = " << oldNorm
                  << std::endl;
        }
        H(n+1, n) = STS::one ();
        rank = 1;
      } else {
        if (outPtr != nullptr) {
          *outPtr << " > newNorm = " << newNorm << " -> H(" << n+1 << ", " << n << ") = zero"
                  << ", tol = " << tolOrtho << " x oldNorm = " << oldNorm
                  << std::endl;
        }
        H(n+1, n) = zero;
        rank = 0;
      }
    } else {
      // check
      TEUCHOS_TEST_FOR_EXCEPTION
        (newNorm < STM::zero (), std::runtime_error, "At iteration "
         << n << ", H(" << n+1 << ", " << n << ") = " << newNorm << " < 0.");

      // compute norm
      newNorm = STM::squareroot (newNorm);
      H(n+1, n) = SC {newNorm};

      // scale
      MVT::MvScale (Qnew, one / H (n+1, n)); // normalize
      rank = 1;
    }

*/
    /*printf( " T = [\n" );
    for (int i = 0; i <= n; i++) {
      for (int j = 0; j <= n; j++) {
        printf( "%e ", T (i, j) );
      }
      printf( "\n" );
    }
    printf( "];\n" );*/
    /* printf( " H = [\n" );
    for (int i = 0; i <= n+1; i++) {
      for (int j = 0; j <= n; j++) {
        printf( "%e ", H (i, j) );
      }
      printf( "\n" );
    }
    printf( "];\n" );*/

//    printf("\n %3.2d \n",n);

/*      if(n>1){  
      Teuchos::Range1D index_prev(0, n-2);
      const MV QQ = * (Q.subView(index_prev));

      real_type orth;
      Teuchos::RCP< dense_matrix_type > orth_check = Teuchos::rcp (new dense_matrix_type (n-1,n-1,true));
      orth_check->putScalar();
      MVT::MvTransMv( (+1.0e+00), QQ, QQ, *orth_check );
      //for(int i=0;i<n-1;i++){(*orth_check)(i,i) =  1.0e+00 - (*orth_check)(i,i);}
      orth = orth_check->normFrobenius();
      for(int i=0;i<n-1;i++){printf("%3.2e, ",(*orth_check)(i,i));}
      printf("  orth check --> %3.2e\n",orth);

      Teuchos::RCP<Tpetra::Map<>> map;// = getMap(Q);
      //Teuchos::RCP<MV> repres_check = rcp( new MV(m,n) );

      //MVT::MvTimesMatAddMv( (1.0e+00), QQ, H, (0.0e+00), *repres_check );
      //MVT::MvAddMv( (1.0e+00), *A, (-1.0e+00), *repres_check, *Q );
      //MVT::MvNorm(QQ,dot,Belos::TwoNorm);
      //for(i=0;i<n;i++){ dot[i] = dot[i] * dot[i]; if(i!=0){ dot[0] += dot[i]; } } 
      //repres = sqrt(dot[0]);
      //printf("repres check --> %3.2e\n",repres);

      }
*/
    return rank;
  }


  //! CGS1 specialization
  int
  projectAndNormalizeSingleReduce_CGS1 (Teuchos::FancyOStream* outPtr,
                                        int n,
                                        const SolverInput<SC>& input, 
                                        MV& Q,
                                        dense_matrix_type& H,
                                        dense_matrix_type& WORK) const
  {
    Teuchos::BLAS<LO, SC> blas;
    const SC one = STS::one ();
    const real_type eps = STS::eps ();
    const real_type tolFactor = static_cast<real_type> (10.0);
    const real_type tolOrtho  = tolFactor * eps;

    int rank = 0;
    Teuchos::Range1D index_all(0, n+1);
    Teuchos::Range1D index_prev(0, n);
    const MV Qall  = * (Q.subView(index_all));
    const MV Qprev = * (Q.subView(index_prev));

    dense_matrix_type h_all (Teuchos::View, H, n+2, 1, 0, n);
    dense_matrix_type h_prev (Teuchos::View, H, n+1, 1, 0, n);

    // Q(:,0:j+1)'*Q(:,j+1)
    vec_type AP = * (Q.getVectorNonConst (n+1));
    MVT::MvTransMv(one, Qall, AP, h_all);

    // orthogonalize (project)
    MVT::MvTimesMatAddMv (-one, Qprev, h_prev, one, AP);

    // save the norm before ortho
    real_type oldNorm = STS::real (H(n+1, n));

    // reorthogonalize if requested
    if (input.needToReortho) {
      // Q(:,0:j+1)'*Q(:,j+1)
      dense_matrix_type w_all (Teuchos::View, WORK, n+2, 1, 0, n);
      dense_matrix_type w_prev (Teuchos::View, WORK, n+1, 1, 0, n);
      MVT::MvTransMv(one, Qall, AP, w_all);
      // orthogonalize (project)
      MVT::MvTimesMatAddMv (-one, Qprev, w_prev, one, AP);
      // recompute the norm
      oldNorm = STS::real (w_all(n+1, 0));
      for (int i = 0; i <= n; ++i) {
        w_all(n+1, 0) -= (w_prev(i, 0)*w_prev(i, 0));
      }

      // accumulate results
      blas.AXPY (n+1, one, w_prev.values (), 1, h_prev.values (), 1);
      H(n+1, n) = w_all(n+1, 0); 
    } else {
      for (int i = 0; i <= n; ++i) {
        H(n+1, n) -= (H(i, n)*H(i, n));
      }
    }

    // check for negative norm
    TEUCHOS_TEST_FOR_EXCEPTION
      (STS::real (H(n+1, n)) < STM::zero (), std::runtime_error, "At iteration "
       << n << ", H(" << n+1 << ", " << n << ") = " << H(n+1, n) << " < 0.");
    // Check for zero norm.  OK to take real part of H(n+1, n), since
    // this is a magnitude (cosine) anyway and therefore should always
    // be real.
    const real_type H_np1_n = STS::real (H(n+1, n));
    if (H_np1_n > oldNorm * tolOrtho) {
      const real_type H_np1_n_sqrt = STM::squareroot (H_np1_n);
      H(n+1, n) = SC {H_np1_n_sqrt};
      MVT::MvScale (AP, STS::one () / H(n+1, n)); // normalize
      rank = 1;
    }
    else {
      if (outPtr != nullptr) {
        *outPtr << " > newNorm = " << H_np1_n << " -> H(" << n+1 << ", " << ") = zero"
                << std::endl;
      }
      H(n+1, n) = STS::zero ();
      rank = 0;
    }
    return rank;
  }


  SolverOutput<SC>
  solveOneVec (Teuchos::FancyOStream* outPtr,
               vec_type& X, // in X/out X
               vec_type& B, // in B
               const OP& A,
               const OP& M,
               const SolverInput<SC>& input)
  {
    using std::endl;
    int restart = input.resCycle;
    const SC zero = STS::zero ();
    const SC one  = STS::one ();
    const bool computeRitzValues = input.computeRitzValues;


    // timers
    Teuchos::RCP< Teuchos::Time > spmvTimer = Teuchos::TimeMonitor::getNewCounter ("GmresSingleReduce::Sparse Mat-Vec");

    SolverOutput<SC> output {};
    // initialize output parameters
    output.numRests = 0;
    output.numIters = 0;
    output.converged = false;

    real_type b_norm;  // initial residual norm
    real_type b0_norm; // initial residual norm, not left-preconditioned
    real_type r_norm;
    real_type r_norm_imp;

    bool zeroOut = false; 
    MV Q (B.getMap (), restart+1, zeroOut);
    vec_type R  (B.getMap (), zeroOut);
    vec_type Y  (B.getMap (), zeroOut);
    vec_type MP (B.getMap (), zeroOut);
    vec_type P = * (Q.getVectorNonConst (0));

    Teuchos::BLAS<LO ,SC> blas;
    dense_matrix_type H (restart+1, restart,   true);
    dense_matrix_type T (restart+1, restart+2, true);
    dense_vector_type y (restart+1, true);
    dense_vector_type z (restart+1, true);
    std::vector<real_type> cs (restart);
    std::vector<SC> sn (restart);
    
    #ifdef HAVE_TPETRA_DEBUG
    dense_matrix_type H2 (restart+1, restart,   true);
    dense_matrix_type H3 (restart+1, restart,   true);
    #endif

    // initial residual (making sure R = B - Ax)
    {
      Teuchos::TimeMonitor LocalTimer (*spmvTimer);
      A.apply (X, R);
    }
    R.update (one, B, -one);
    b0_norm = R.norm2 (); // initial residual norm, no-preconditioned
    if (input.precoSide == "left") {
      M.apply (R, P);
      r_norm = P.norm2 (); // initial residual norm, left-preconditioned
    } else {
      r_norm = b0_norm;
    }
    b_norm = r_norm;

    real_type metric = this->getConvergenceMetric (b0_norm, b0_norm, input);
    if (metric <= input.tol) {
      if (outPtr != nullptr) {
        *outPtr << "Initial guess' residual norm " << b0_norm
                << " meets tolerance " << input.tol << endl;
      }
      output.absResid = b0_norm;
      output.relResid = STM::one ();
      output.converged = true;
      return output;
    } else if (outPtr != nullptr) {
      *outPtr << "Initial guess' residual norm " << b0_norm << endl;
    }

    // compute Ritz values as Newton shifts
    if (computeRitzValues) {
      // Invoke ordinary GMRES for the first restart
      SolverInput<SC> input_gmres = input;
      input_gmres.maxNumIters = input.resCycle;
      input_gmres.computeRitzValues = computeRitzValues;
      if (input.orthoType == "MGS") {
        // MGS1 or MGS2
        input_gmres.orthoType = "IMGS";
        if (input.needToReortho) {
          input_gmres.maxOrthoSteps = 2;
        } else {
          input_gmres.maxOrthoSteps = 1;
        }
      } else if (input.orthoType == "CGS") {
        // CGS1 or CGS2
        input_gmres.orthoType = "ICGS";
        if (input.needToReortho) {
          input_gmres.maxOrthoSteps = 2;
        } else {
          input_gmres.maxOrthoSteps = 1;
        }
      } else if (input.orthoType == "CGS2") {
        // CGS2
        input_gmres.orthoType = "ICGS";
        input_gmres.maxOrthoSteps = 2;
      }
      if (outPtr != nullptr) {
        *outPtr << "Run standard GMRES for first restart cycle" << endl;
      }

      Tpetra::deep_copy (R, B);
      output = Gmres<SC, MV, OP>::solveOneVec (outPtr, X, R, A, M,
                                               input_gmres);
      if (output.converged) {
        return output; // standard GMRES converged
      }

      if (input.precoSide == "left") {
        M.apply (R, P);
        r_norm = P.norm2 (); // residual norm
      }
      else {
        r_norm = output.absResid;
      }
      output.numRests++;
    }

    // initialize starting vector
    if (input.precoSide != "left") {
      Tpetra::deep_copy (P, R);
    }
    P.scale (one / r_norm);
    y[0] = SC {r_norm};
    const int s = getStepSize ();
    // main loop
    bool delayed_ortho = ((input.orthoType == "MGS" && input.needToReortho) ||
                          (input.orthoType == "CGS2"));
    int iter = 0;
    while (output.numIters < input.maxNumIters && ! output.converged) {
      if (input.maxNumIters < output.numIters+restart) {
        restart = input.maxNumIters-output.numIters;
      }
      // restart cycle
      for (; iter < restart && (metric > input.tol && !STS::isnaninf (metric)); ++iter) {
        // AP = A*P
        vec_type P  = * (Q.getVectorNonConst (iter));
        vec_type AP = * (Q.getVectorNonConst (iter+1));
        if (input.precoSide == "none") { // no preconditioner
          Teuchos::TimeMonitor LocalTimer (*spmvTimer);
          A.apply (P, AP);
        }
        else if (input.precoSide == "right") {
          M.apply (P, MP);
          {
            Teuchos::TimeMonitor LocalTimer (*spmvTimer);
            A.apply (MP, AP);
          }
        }
        else {
          {
            Teuchos::TimeMonitor LocalTimer (*spmvTimer);
            A.apply (P, MP);
          }
          M.apply (MP, AP);
        }
        // Shift for Newton basis
        if (computeRitzValues) {
          //AP.update (-output.ritzValues[iter],  P, one);
          const complex_type theta = output.ritzValues[iter % s];
          UpdateNewton<SC, MV>::updateNewtonV (iter, Q, theta);
        }
        output.numIters++; 

        // Orthogonalization

        projectAndNormalizeSingleReduce (outPtr, iter, input, Q, H, T);

        // Convergence check
        if (!delayed_ortho || iter > 0) {
          int check = (delayed_ortho ? iter-1 : iter);
          if (outPtr != nullptr) {
            *outPtr << "Current iteration: iter=" << iter
                    << ", restart=" << restart
                    << ", metric=" << metric << endl;
            Indent indent3 (outPtr);
          }

          // Shift back for Newton basis
          if (computeRitzValues) {
            const complex_type theta = output.ritzValues[check % s];
            UpdateNewton<SC, MV>::updateNewtonH (check, H, theta);
          }
          #ifdef HAVE_TPETRA_DEBUG
          this->checkNumerics (outPtr, iter, check, A, M, Q, X, B, y,
                               H, H2, H3, cs, sn, input);

          if (outPtr != nullptr) {
            dense_matrix_type T2 (check+1, check+1, true);
            for (int j = 0; j < check+1; j++) {
              blas.COPY (check+1, &(T(0, j)), 1, &(T2(0, j)), 1);
              T2 (j, j) -= one;
            }
            real_type ortho_error = this->computeNorm(T2);
            *outPtr << " > norm(T-I) = " << ortho_error << endl;
          }
          #endif

          if (H(check+1, check) != zero) {
            // Apply Givens rotations to new column of H and y
            this->reduceHessenburgToTriangular (check, H, cs, sn, y.values ());
            // Convergence check
            metric = this->getConvergenceMetric (STS::magnitude (y[check+1]),
                                                 b_norm, input);
          }
          else {
            H(check+1, check) = zero;
            metric = STM::zero ();
          }
        }
      } // end of restart cycle 

      if (delayed_ortho) {
        // Orthogonalization, cleanup
        projectAndNormalizeSingleReduce_cleanup (outPtr, iter, input, Q, H, T);

        int check = iter-1;
        // Shift back for Newton basis
        if (computeRitzValues) {
          const complex_type theta = output.ritzValues[check % s];
          UpdateNewton<SC, MV>::updateNewtonH (check, H, theta);
        }
        #ifdef HAVE_TPETRA_DEBUG
        this->checkNumerics (outPtr, iter, check, A, M, Q, X, B, y,
                             H, H2, H3, cs, sn, input);
        #endif

        if (H(check+1, check) != zero) {
          // Apply Givens rotations to new column of H and y
          this->reduceHessenburgToTriangular (check, H, cs, sn, y.values ());
          // Convergence check
          metric = this->getConvergenceMetric (STS::magnitude (y[check+1]),
                                               b_norm, input);
        }
        else {
          H(check+1, check) = zero;
          metric = STM::zero ();
        }
      }

      if (iter < restart) {
        // save the old solution, just in case explicit residual norm failed the convergence test
        Tpetra::deep_copy (Y, X);
        blas.COPY (1+iter, y.values(), 1, z.values(), 1);
      }
      r_norm_imp = STS::magnitude (y (iter)); // save implicit residual norm
      if (iter > 0) {
        // Update solution
        blas.TRSM (Teuchos::LEFT_SIDE, Teuchos::UPPER_TRI, Teuchos::NO_TRANS,
                   Teuchos::NON_UNIT_DIAG, iter, 1, one,
                   H.values(), H.stride(), y.values (), y.stride ());
        Teuchos::Range1D cols(0, iter-1);
        Teuchos::RCP<const MV> Qj = Q.subView(cols);
        y.resize (iter);
        if (input.precoSide == "right") {
          MVT::MvTimesMatAddMv (one, *Qj, y, zero, R);
          M.apply (R, MP);
          X.update (one, MP, one);
        }
        else {
          MVT::MvTimesMatAddMv (one, *Qj, y, one, X);
        }
        y.resize (restart+1);
      }

      // Compute explicit residual vector in preparation for restart.
      {
        Teuchos::TimeMonitor LocalTimer (*spmvTimer);
        A.apply (X, R);
      }
      R.update (one, B, -one);
      r_norm = R.norm2 (); // residual norm
      output.absResid = r_norm;
      output.relResid = r_norm / b0_norm;
      if (outPtr != nullptr) {
        *outPtr << "Implicit and explicit residual norms at restart: " << r_norm_imp << ", " << r_norm << endl;
      }

      metric = this->getConvergenceMetric (r_norm, b0_norm, input);
      if (metric <= input.tol) {
        output.converged = true;
        return output;
      }
      else if (output.numIters < input.maxNumIters) {
        // Restart, only if max inner-iteration was reached.
        // Otherwise continue the inner-iteration.
        if (iter >= restart || H(iter,iter-1) == zero) { // done with restart cycyle, or probably lost ortho
          // Initialize starting vector for restart
          iter = 0;
          P = * (Q.getVectorNonConst (0));
          if (input.precoSide == "left") {
            M.apply (R, P);
            // FIXME (mfh 14 Aug 2018) Didn't we already compute this above?
            r_norm = P.norm2 ();
          }
          else {
            // set the starting vector
            Tpetra::deep_copy (P, R);
          }
          P.scale (one / r_norm);
          y[0] = SC {r_norm};
          for (int i=1; i < restart+1; i++) {
            y[i] = zero;
          }
          output.numRests++; // restart
        }
        else {
          // reset to the old solution
          if (outPtr != nullptr) {
            *outPtr << " > not-restart with iter=" << iter << endl;
          }
          Tpetra::deep_copy (X, Y);
          blas.COPY (1+iter, z.values(), 1, y.values(), 1);
        }
      }
    }

    return output;
  }
  
  int stepSize_; // "step size" for Newton basis
};

template<class SC, class MV, class OP,
         template<class, class, class> class KrylovSubclassType>
class SolverManager;

// This is the Belos::SolverManager subclass that gets registered with
// Belos::SolverFactory.
template<class SC, class MV, class OP>
using GmresSingleReduceSolverManager = SolverManager<SC, MV, OP, GmresSingleReduce>;

/// \brief Register GmresSingleReduceSolverManager for all enabled Tpetra
///   template parameter combinations.
void register_GmresSingleReduce (const bool verbose);

} // namespace Impl
} // namespace BelosTpetra

#endif // BELOS_TPETRA_GMRES_SINGLE_REDUCE_HPP
