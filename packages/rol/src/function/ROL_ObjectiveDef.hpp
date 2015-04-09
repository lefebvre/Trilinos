// @HEADER
// ************************************************************************
//
//               Rapid Optimization Library (ROL) Package
//                 Copyright (2014) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact lead developers:
//              Drew Kouri   (dpkouri@sandia.gov) and
//              Denis Ridzal (dridzal@sandia.gov)
//
// ************************************************************************
// @HEADER

#ifndef ROL_OBJECTIVE_DEF_H
#define ROL_OBJECTIVE_DEF_H

/** \class ROL::Objective
    \brief Provides the definition of the objective function interface.
*/

namespace ROL {

template <class Real>
Real Objective<Real>::dirDeriv( const Vector<Real> &x, const Vector<Real> &d, Real &tol) {
  Real ftol = std::sqrt(ROL_EPSILON);

  Teuchos::RCP<Vector<Real> > xd = d.clone();
  xd->set(x);
  xd->axpy(tol, d);
  return (this->value(*xd,ftol) - this->value(x,ftol)) / tol;
}

template <class Real>
void Objective<Real>::gradient( Vector<Real> &g, const Vector<Real> &x, Real &tol ) {
  g.zero();
  Real deriv = 0.0;
  Real h     = 0.0;
  for (int i = 0; i < g.dimension(); i++) {
    h     = x.dot(*x.basis(i))*tol;
    deriv = this->dirDeriv(x,*x.basis(i),h);
    g.axpy(deriv,*g.basis(i));
  }
}

template <class Real>
void Objective<Real>::hessVec( Vector<Real> &hv, const Vector<Real> &v, const Vector<Real> &x, Real &tol ) {
  Real gtol = std::sqrt(ROL_EPSILON);

  // Get Step Length
  Real h = std::max(1.0,x.norm()/v.norm())*tol;
  //Real h = 2.0/(v.norm()*v.norm())*tol;

  // Compute Gradient at x
  Teuchos::RCP<Vector<Real> > g = hv.clone();
  this->gradient(*g,x,gtol);

  // Compute New Step x + h*v
  Teuchos::RCP<Vector<Real> > xnew = x.clone();
  xnew->set(x);
  xnew->axpy(h,v);  
  this->update(*xnew);

  // Compute Gradient at x + h*v
  hv.zero();
  this->gradient(hv,*xnew,gtol);
  
  // Compute Newton Quotient
  hv.axpy(-1.0,*g);
  hv.scale(1.0/h);
} 


template <class Real>
std::vector<std::vector<Real> > Objective<Real>::checkGradient( const Vector<Real> &x,
                                                                const Vector<Real> &g,
                                                                const Vector<Real> &d,
                                                                const bool printToStream,
                                                                std::ostream & outStream,
                                                                const int numSteps,
                                                                const int order ) {

  TEUCHOS_TEST_FOR_EXCEPTION( order<1 || order>4, std::invalid_argument, 
                              "Error: finite difference order must be 1,2,3, or 4" );

  // Finite difference steps in axpy form    
  int steps[4][4] = { {  1,  0,  0, 0 },  // First order
                      { -1,  2,  0, 0 },  // Second order
                      { -1,  2,  1, 0 },  // Third order
                      { -1, -1,  3, 1 }   // Fourth order
                    };

  // Finite difference weights     
  Real weights[4][5] = { { -1.0,          1.0, 0.0,      0.0,      0.0      },  // First order
                         {  0.0,     -1.0/2.0, 1.0/2.0,  0.0,      0.0      },  // Second order
                         { -1.0/2.0, -1.0/3.0, 1.0,     -1.0/6.0,  0.0      },  // Third order
                         {  0.0,     -2.0/3.0, 1.0/12.0, 2.0/3.0, -1.0/12.0 }   // Fourth order
                       };

  Real tol = std::sqrt(ROL_EPSILON);

  int numVals = 4;
  std::vector<Real> tmp(numVals);
  std::vector<std::vector<Real> > gCheck(numSteps, tmp);
  Real eta_factor = 1e-1;
  Real eta = 1.0;

  // Save the format state of the original outStream.
  Teuchos::oblackholestream oldFormatState;
  oldFormatState.copyfmt(outStream);


  // Evaluate objective value at x.
  this->update(x);

  
  // Compute gradient at x.
  Teuchos::RCP<Vector<Real> > gtmp = g.clone();
  this->gradient(*gtmp, x, tol);
  Real dtg = d.dot(gtmp->dual());

  // Temporary vectors.
  Teuchos::RCP<Vector<Real> > xnew = x.clone();

  for (int i=0; i<numSteps; i++) {

    xnew->set(x);

    // Compute gradient, finite-difference gradient, and absolute error.
    gCheck[i][0] = eta;
    gCheck[i][1] = dtg;

    gCheck[i][2] = weights[order-1][0] * this->value(x,tol);

    for(int j=0; j<order; ++j) {
        // Evaluate at x <- x+eta*c_i*d.
        xnew->axpy(eta*steps[order-1][j], d);

        // Only evaluate at steps where the weight is nonzero  
        if( weights[order-1][j+1] != 0 ) {        
            this->update(*xnew);
            gCheck[i][2] += weights[order-1][j+1] * this->value(*xnew,tol);
        }
    }

    gCheck[i][2] /= eta;

    gCheck[i][3] = std::abs(gCheck[i][2] - gCheck[i][1]);

    if (printToStream) {
      if (i==0) {
      outStream << std::right
                << std::setw(20) << "Step size"
                << std::setw(20) << "grad'*dir"
                << std::setw(20) << "FD approx"
                << std::setw(20) << "abs error"
                << "\n";
      }
      outStream << std::scientific << std::setprecision(11) << std::right
                << std::setw(20) << gCheck[i][0]
                << std::setw(20) << gCheck[i][1]
                << std::setw(20) << gCheck[i][2]
                << std::setw(20) << gCheck[i][3]
                << "\n";
    }

    // Update eta.
    eta = eta*eta_factor;
  }

  // Reset format state of outStream.
  outStream.copyfmt(oldFormatState);

  return gCheck;
} // checkGradient










template <class Real>
std::vector<std::vector<Real> > Objective<Real>::checkHessVec( const Vector<Real> &x,
                                                               const Vector<Real> &hv,
                                                               const Vector<Real> &v,
                                                               const bool printToStream,
                                                               std::ostream & outStream,
                                                               const int numSteps,
                                                               const int order ) {

  TEUCHOS_TEST_FOR_EXCEPTION( order<1 || order>4, std::invalid_argument, 
                              "Error: finite difference order must be 1,2,3, or 4" );

  // Finite difference steps in axpy form    
  int steps[4][4] = { {  1,  0,  0, 0 },  // First order
                      { -1,  2,  0, 0 },  // Second order
                      { -1,  2,  1, 0 },  // Third order
                      { -1, -1,  3, 1 }   // Fourth order
                    };

  // Finite difference weights     
  Real weights[4][5] = { { -1.0,          1.0, 0.0,      0.0,      0.0      },  // First order
                         {  0.0,     -1.0/2.0, 1.0/2.0,  0.0,      0.0      },  // Second order
                         { -1.0/2.0, -1.0/3.0, 1.0,     -1.0/6.0,  0.0      },  // Third order
                         {  0.0,     -2.0/3.0, 1.0/12.0, 2.0/3.0, -1.0/12.0 }   // Fourth order
                       };

  Real tol = std::sqrt(ROL_EPSILON);

  int numVals = 4;
  std::vector<Real> tmp(numVals);
  std::vector<std::vector<Real> > hvCheck(numSteps, tmp);
  Real eta_factor = 1e-1;
  Real eta = 1.0;

  // Save the format state of the original outStream.
  Teuchos::oblackholestream oldFormatState;
  oldFormatState.copyfmt(outStream);

  // Compute gradient at x.
  Teuchos::RCP<Vector<Real> > g = hv.clone();
  this->update(x);
  this->gradient(*g, x, tol);

  // Compute (Hessian at x) times (vector v).
  Teuchos::RCP<Vector<Real> > Hv = hv.clone();
  this->hessVec(*Hv, v, x, tol);
  Real normHv = Hv->norm();

  // Temporary vectors.
  Teuchos::RCP<Vector<Real> > gdif = hv.clone();
  Teuchos::RCP<Vector<Real> > gnew = hv.clone();
  Teuchos::RCP<Vector<Real> > xnew = x.clone();

  for (int i=0; i<numSteps; i++) {

    // Evaluate objective value at x+eta*d.
    xnew->set(x);

    gdif->set(*g);
    gdif->scale(weights[order-1][0]);

    for(int j=0; j<order; ++j) {

        // Evaluate at x <- x+eta*c_i*d.
        xnew->axpy(eta*steps[order-1][j], v);

        // Only evaluate at steps where the weight is nonzero  
        if( weights[order-1][j+1] != 0 ) {
            this->update(*xnew);
            this->gradient(*gnew, *xnew, tol); 
            gdif->axpy(weights[order-1][j+1],*gnew);
        }
       
    }

    gdif->scale(1.0/eta);    

    // Compute norms of hessvec, finite-difference hessvec, and error.
    hvCheck[i][0] = eta;
    hvCheck[i][1] = normHv;
    hvCheck[i][2] = gdif->norm();
    gdif->axpy(-1.0, *Hv);
    hvCheck[i][3] = gdif->norm();

    if (printToStream) {
      if (i==0) {
      outStream << std::right
                << std::setw(20) << "Step size"
                << std::setw(20) << "norm(Hess*vec)"
                << std::setw(20) << "norm(FD approx)"
                << std::setw(20) << "norm(abs error)"
                << "\n";
      }
      outStream << std::scientific << std::setprecision(11) << std::right
                << std::setw(20) << hvCheck[i][0]
                << std::setw(20) << hvCheck[i][1]
                << std::setw(20) << hvCheck[i][2]
                << std::setw(20) << hvCheck[i][3]
                << "\n";
    }

    // Update eta.
    eta = eta*eta_factor;
  }

  // Reset format state of outStream.
  outStream.copyfmt(oldFormatState);

  return hvCheck;
} // checkHessVec


template<class Real>
std::vector<Real> Objective<Real>::checkHessSym( const Vector<Real> &x,
                                                 const Vector<Real> &hv,
                                                 const Vector<Real> &v,
                                                 const Vector<Real> &w,
                                                 const bool printToStream,
                                                 std::ostream & outStream ) {

  Real tol = std::sqrt(ROL_EPSILON);
  
  // Compute (Hessian at x) times (vector v).
  Teuchos::RCP<Vector<Real> > h = hv.clone();
  this->hessVec(*h, v, x, tol);
  Real wHv = w.dot(h->dual());

  this->hessVec(*h, w, x, tol);
  Real vHw = v.dot(h->dual());

  std::vector<Real> hsymCheck(3, 0);

  hsymCheck[0] = wHv;
  hsymCheck[1] = vHw;
  hsymCheck[2] = std::abs(vHw-wHv);

  // Save the format state of the original outStream.
  Teuchos::oblackholestream oldFormatState;
  oldFormatState.copyfmt(outStream);

  if (printToStream) {
    outStream << std::right
              << std::setw(20) << "<w, H(x)v>"
              << std::setw(20) << "<v, H(x)w>"
              << std::setw(20) << "abs error"
              << "\n";
    outStream << std::scientific << std::setprecision(11) << std::right
              << std::setw(20) << hsymCheck[0]
              << std::setw(20) << hsymCheck[1]
              << std::setw(20) << hsymCheck[2]
              << "\n";
  }

  // Reset format state of outStream.
  outStream.copyfmt(oldFormatState);

  return hsymCheck;

} // checkHessSym



} // namespace ROL

#endif
