#pragma once
#ifndef __GEMM_T_NT_HPP__
#define __GEMM_T_NT_HPP__

/// \file gemm_t_nt.hpp
/// \brief Sparse matrix-matrix multiplication on given sparse patterns.
/// \author Kyungjoo Kim (kyukim@sandia.gov)

namespace Example { 

  using namespace std;
  
  template<>
  template<typename ScalarType, 
           typename CrsMatViewType>
  KOKKOS_INLINE_FUNCTION 
  int
  Gemm<Trans::ConjTranspose,Trans::NoTranspose,
       AlgoGemm::ForRightBlocked>
  ::invoke(const ScalarType alpha,
           const CrsMatViewType A,
           const CrsMatViewType B,
           const ScalarType beta,
           const CrsMatViewType C) {
    typedef typename CrsMatViewType::ordinal_type  ordinal_type;
    typedef typename CrsMatViewType::value_type    value_type;
    typedef typename CrsMatViewType::row_view_type row_view_type;

    scale(beta, C);

    //row_view_type a, b, c;
    for (ordinal_type k=0;k<A.NumRows();++k) {
      //a.setView(A, k);
      row_view_type &a = A.RowView(k);
      const ordinal_type nnz_a = a.NumNonZeros();

      //b.setView(B, k);
      row_view_type &b = B.RowView(k);
      const ordinal_type nnz_b = b.NumNonZeros();

      for (ordinal_type i=0;i<nnz_a;++i) {
        const ordinal_type row_at_i = a.Col(i);
        const value_type   val_at_i = conj(a.Value(i));

        //c.setView(C, row_at_i);
        row_view_type &c = C.RowView(row_at_i);
        
        ordinal_type idx = 0;
        for (ordinal_type j=0;j<nnz_b && (idx > -2);++j) {
          ordinal_type col_at_j = b.Col(j);
          value_type   val_at_j = b.Value(j);

          idx = c.Index(col_at_j, idx);
          if (idx >= 0) 
            c.Value(idx) += alpha*val_at_i*val_at_j;
        }
      }
    }
    
    return 0;
  }
  
}

#endif