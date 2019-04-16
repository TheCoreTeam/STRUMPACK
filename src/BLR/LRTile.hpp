/*
 * STRUMPACK -- STRUctured Matrices PACKage, Copyright (c) 2014, The
 * Regents of the University of California, through Lawrence Berkeley
 * National Laboratory (subject to receipt of any required approvals
 * from the U.S. Dept. of Energy).  All rights reserved.
 *
 * If you have questions about your rights to use or distribute this
 * software, please contact Berkeley Lab's Technology Transfer
 * Department at TTD@lbl.gov.
 *
 * NOTICE. This software is owned by the U.S. Department of Energy. As
 * such, the U.S. Government has been granted for itself and others
 * acting on its behalf a paid-up, nonexclusive, irrevocable,
 * worldwide license in the Software to reproduce, prepare derivative
 * works, and perform publicly and display publicly.  Beginning five
 * (5) years after the date permission to assert copyright is obtained
 * from the U.S. Department of Energy, and subject to any subsequent
 * five (5) year renewals, the U.S. Government is granted for itself
 * and others acting on its behalf a paid-up, nonexclusive,
 * irrevocable, worldwide license in the Software to reproduce,
 * prepare derivative works, distribute copies to the public, perform
 * publicly and display publicly, and to permit others to do so.
 *
 * Developers: Pieter Ghysels, Francois-Henry Rouet, Xiaoye S. Li.
 *             (Lawrence Berkeley National Lab, Computational Research
 *             Division).
 *
 */
/*! \file LRTile.hpp
 * \brief Contains LRTile class, subclass of BLRTile.
 */
#ifndef LR_TILE_HPP
#define LR_TILE_HPP

#include <cassert>

#include "BLRTile.hpp"
#include "dense/ACA.hpp"
#include "dense/BACA.hpp"

namespace strumpack {
  namespace BLR {

    /**
     * Low rank U*V tile
     */
    template<typename scalar_t> class LRTile
      : public BLRTile<scalar_t> {
      using DenseM_t = DenseMatrix<scalar_t>;
      using DenseMW_t = DenseMatrixWrapper<scalar_t>;
      using Opts_t = BLROptions<scalar_t>;

    public:

      LRTile(const DenseM_t& T, const Opts_t& opts) {
        if (opts.low_rank_algorithm() == LowRankAlgorithm::RRQR) {
          T.low_rank(U_, V_, opts.rel_tol(), opts.abs_tol(), opts.max_rank(),
                     params::task_recursion_cutoff_level);
        } else if (opts.low_rank_algorithm() == LowRankAlgorithm::ACA) {
          adaptive_cross_approximation<scalar_t>
            (U_, V_, T.rows(), T.cols(),
             [&](std::size_t i, std::size_t j) -> scalar_t {
              assert(i < T.rows());
              assert(j < T.cols());
              return T(i, j); },
             opts.rel_tol(), opts.abs_tol(), opts.max_rank());
        }
      }

      /**
       * .. by extracting individual elements
       */
      LRTile(std::size_t m, std::size_t n,
             const std::function<scalar_t(std::size_t,std::size_t)>& Telem,
             const Opts_t& opts) {
        adaptive_cross_approximation<scalar_t>
          (U_, V_, m, n, Telem, opts.rel_tol(), opts.abs_tol(),
           opts.max_rank());
      }

      /**
       * .. by extracting 1 column or 1 row at a time
       */
      LRTile(std::size_t m, std::size_t n,
             const std::function<void(std::size_t,scalar_t*)>& Trow,
             const std::function<void(std::size_t,scalar_t*)>& Tcol,
             const Opts_t& opts) {
        adaptive_cross_approximation<scalar_t>
          (U_, V_, m, n, Trow, Tcol, opts.rel_tol(), opts.abs_tol(),
           opts.max_rank());
      }

      /**
       * .. by extracting multiple columns or rows at a time
       */
      LRTile(std::size_t m, std::size_t n,
             const std::function
             <void(const std::vector<std::size_t>&,
                   DenseMatrix<scalar_t>&)>& Trow,
             const std::function
             <void(const std::vector<std::size_t>&,
                   DenseMatrix<scalar_t>&)>& Tcol,
             const Opts_t& opts) {
        blocked_adaptive_cross_approximation<scalar_t>
          (U_, V_, m, n, Trow, Tcol, opts.BACA_blocksize(),
           opts.rel_tol(), opts.abs_tol(), opts.max_rank(),
           params::task_recursion_cutoff_level);
      }

      std::size_t rows() const override { return U_.rows(); }
      std::size_t cols() const override { return V_.cols(); }
      std::size_t rank() const override { return U_.cols(); }
      bool is_low_rank() const override { return true; };

      std::size_t memory() const override { return U_.memory() + V_.memory(); }
      std::size_t nonzeros() const override { return U_.nonzeros() + V_.nonzeros(); }
      std::size_t maximum_rank() const override { return U_.cols(); }

      void dense(DenseM_t& A) const override {
        gemm(Trans::N, Trans::N, scalar_t(1.), U_, V_, scalar_t(0.), A,
             params::task_recursion_cutoff_level);
      }

      void draw
      (std::ostream& of, std::size_t roff, std::size_t coff) const override {
        char prev = std::cout.fill('0');
        int minmn = std::min(rows(), cols());
        int red = std::floor(255.0 * rank() / minmn);
        int blue = 255 - red;
        of << "set obj rect from "
           << roff << ", " << coff << " to "
           << roff+rows() << ", " << coff+cols()
           << " fc rgb '#"
           << std::hex << std::setw(2) << std::setfill('0') << red
           << "00" << std::setw(2)  << std::setfill('0') << blue
           << "'" << std::dec << std::endl;
        std::cout.fill(prev);
      }

      DenseM_t& D() override { assert(false); return U_; }
      DenseM_t& U() override { return U_; }
      DenseM_t& V() override { return V_; }
      const DenseM_t& D() const override { assert(false); return U_; }
      const DenseM_t& U() const override { return U_; }
      const DenseM_t& V() const override { return V_; }

      scalar_t operator()(std::size_t i, std::size_t j) const override {
        return blas::dotu(rank(), U_.ptr(i, 0), U_.ld(), V_.ptr(0, j), 1);
      }

      void laswp(const std::vector<int>& piv, bool fwd) override {
        U_.laswp(piv, fwd);
      }

      void trsm_b(Side s, UpLo ul, Trans ta, Diag d,
                  scalar_t alpha, const DenseM_t& a) override {
        strumpack::trsm
          (s, ul, ta, d, alpha, a, (s == Side::L) ? U_ : V_,
           params::task_recursion_cutoff_level);
      }
      void gemv_a(Trans ta, scalar_t alpha, const DenseM_t& x,
                  scalar_t beta, DenseM_t& y) const override {
        DenseM_t tmp(rank(), x.cols());
        gemv(ta, scalar_t(1.), ta==Trans::N ? V() : U(), x, scalar_t(0.), tmp,
             params::task_recursion_cutoff_level);
        gemv(ta, alpha, ta==Trans::N ? U() : V(), tmp, beta, y,
             params::task_recursion_cutoff_level);
      }
      void gemm_a(Trans ta, Trans tb, scalar_t alpha,
                  const BLRTile<scalar_t>& b,
                  scalar_t beta, DenseM_t& c) const override {
        b.gemm_b(ta, tb, alpha, *this, beta, c);
      }
      void gemm_a(Trans ta, Trans tb, scalar_t alpha,
                  const DenseM_t& b, scalar_t beta,
                  DenseM_t& c, int task_depth) const override {
        DenseM_t tmp(rank(), c.cols());
        gemm(ta, tb, scalar_t(1.), ta==Trans::N ? V() : U(), b,
             scalar_t(0.), tmp, task_depth);
        gemm(ta, Trans::N, alpha, ta==Trans::N ? U() : V(), tmp,
             beta, c, task_depth);
      }
      void gemm_b(Trans ta, Trans tb, scalar_t alpha,
                  const LRTile<scalar_t>& a, scalar_t beta,
                  DenseM_t& c) const override {
        DenseM_t tmp1(a.rank(), rank());
        gemm(ta, tb, scalar_t(1.), ta==Trans::N ? a.V() : a.U(),
             tb==Trans::N ? U() : V(), scalar_t(0.), tmp1,
             params::task_recursion_cutoff_level);
        DenseM_t tmp2(c.rows(), tmp1.cols());
        gemm(ta, Trans::N, scalar_t(1.), ta==Trans::N ? a.U() : a.V(), tmp1,
             scalar_t(0.), tmp2, params::task_recursion_cutoff_level);
        gemm(Trans::N, tb, alpha, tmp2, tb==Trans::N ? V() : U(),
             beta, c, params::task_recursion_cutoff_level);
      }
      void gemm_b(Trans ta, Trans tb, scalar_t alpha,
                  const DenseTile<scalar_t>& a, scalar_t beta,
                  DenseM_t& c) const override {
        gemm_b(ta, tb, alpha, a.D(), beta, c,
               params::task_recursion_cutoff_level);
      }
      void gemm_b(Trans ta, Trans tb, scalar_t alpha,
                  const DenseM_t& a, scalar_t beta,
                  DenseM_t& c, int task_depth) const override {
        DenseM_t tmp(c.rows(), rank());
        gemm(ta, tb, scalar_t(1.), a, tb==Trans::N ? U() : V(),
             scalar_t(0.), tmp, task_depth);
        gemm(Trans::N, tb, alpha, tmp, tb==Trans::N ? V() : U(),
             beta, c, task_depth);
      }

      void Schur_update_col_a
      (std::size_t i, const BLRTile<scalar_t>& b, scalar_t* c)
        const override {
        b.Schur_update_col_b(i, *this, c);
      }
      void Schur_update_row_a
      (std::size_t i, const BLRTile<scalar_t>& b, scalar_t* c)
        const override {
        b.Schur_update_row_b(i, *this, c);
      }

      void Schur_update_col_b
      (std::size_t i, const LRTile<scalar_t>& a, scalar_t* c)
        const override {
        DenseM_t temp1(rows(), 1), temp2(a.rank(), 1);
        gemv(Trans::N, scalar_t(1.), U_, V_.ptr(0, i), 1,
             scalar_t(0.), temp1, params::task_recursion_cutoff_level);
        gemv(Trans::N, scalar_t(1.), a.V(), temp1, scalar_t(0.), temp2,
             params::task_recursion_cutoff_level);
        gemv(Trans::N, scalar_t(-1.), a.U(), temp2, scalar_t(1.), c, 1,
             params::task_recursion_cutoff_level);
      }
      void Schur_update_col_b
      (std::size_t i, const DenseTile<scalar_t>& a, scalar_t* c)
        const override {
        DenseM_t temp(rows(), 1);
        gemv(Trans::N, scalar_t(1.), U_, V_.ptr(0, i), 1,
             scalar_t(0.), temp, params::task_recursion_cutoff_level);
        gemv(Trans::N, scalar_t(-1.), a.D(), temp, scalar_t(1.), c, 1,
             params::task_recursion_cutoff_level);
      }
      void Schur_update_row_b
      (std::size_t i, const LRTile<scalar_t>& a, scalar_t* c)
        const override {
        DenseM_t temp1(1, a.cols()), temp2(1, rank());
        gemv(Trans::C, scalar_t(1.), a.V(), a.U().ptr(i, 0), a.U().ld(),
             scalar_t(0.), temp1.data(), temp1.ld(),
             params::task_recursion_cutoff_level);
        gemv(Trans::C, scalar_t(1.), U_, temp1.data(), temp1.ld(),
             scalar_t(0.), temp2.data(), temp2.ld(),
             params::task_recursion_cutoff_level);
        gemv(Trans::C, scalar_t(-1.), V_, temp2.data(), temp2.ld(),
             scalar_t(1.), c, 1, params::task_recursion_cutoff_level);
      }
      void Schur_update_row_b
      (std::size_t i, const DenseTile<scalar_t>& a, scalar_t* c)
        const override {
        DenseM_t temp(1, rank());
        gemv(Trans::C, scalar_t(1.), U(), a.D().ptr(i, 0), a.D().ld(),
             scalar_t(0.), temp.data(), temp.ld(),
             params::task_recursion_cutoff_level);
        gemv(Trans::C, scalar_t(-1.), V(), temp.data(), temp.ld(),
             scalar_t(1.), c, 1, params::task_recursion_cutoff_level);
      }


      void Schur_update_cols_a
      (const std::vector<std::size_t>& cols, const BLRTile<scalar_t>& b,
       DenseMatrix<scalar_t>& c) const override {
        b.Schur_update_cols_b(cols, *this, c);
      }
      void Schur_update_rows_a
      (const std::vector<std::size_t>& rows, const BLRTile<scalar_t>& b,
       DenseMatrix<scalar_t>& c) const override {
        b.Schur_update_rows_b(rows, *this, c);
      }

      // TODO take a workarray, store temp, and the extracted
      // rows/columns in a wrapper matrix on the work array
      void Schur_update_cols_b
      (const std::vector<std::size_t>& cols, const LRTile<scalar_t>& a,
       DenseMatrix<scalar_t>& c) const override {
        DenseM_t temp1(rows(), cols.size()), temp2(a.rank(), cols.size());
        gemm(Trans::N, Trans::N, scalar_t(1.), U_, V_.extract_cols(cols),
             scalar_t(0.), temp1, params::task_recursion_cutoff_level);
        gemm(Trans::N, Trans::N, scalar_t(1.), a.V(), temp1, scalar_t(0.),
             temp2, params::task_recursion_cutoff_level);
        gemm(Trans::N, Trans::N, scalar_t(-1.), a.U(), temp2, scalar_t(1.),
             c, params::task_recursion_cutoff_level);
      }
      void Schur_update_cols_b
      (const std::vector<std::size_t>& cols, const DenseTile<scalar_t>& a,
       DenseMatrix<scalar_t>& c) const override {
        DenseM_t temp(rows(), cols.size());
        gemm(Trans::N, Trans::N, scalar_t(1.), U_, V_.extract_cols(cols),
             scalar_t(0.), temp, params::task_recursion_cutoff_level);
        gemm(Trans::N, Trans::N, scalar_t(-1.), a.D(), temp, scalar_t(1.),
             c, params::task_recursion_cutoff_level);
      }
      void Schur_update_rows_b
      (const std::vector<std::size_t>& rows, const LRTile<scalar_t>& a,
       DenseMatrix<scalar_t>& c) const override {
        DenseM_t temp1(rows.size(), a.cols()), temp2(rows.size(), rank());
        gemm(Trans::N, Trans::N, scalar_t(1.), a.U().extract_rows(rows),
             a.V(), scalar_t(0.), temp1, params::task_recursion_cutoff_level);
        gemm(Trans::N, Trans::N, scalar_t(1.), temp1, U_, scalar_t(0.),
             temp2, params::task_recursion_cutoff_level);
        gemm(Trans::N, Trans::N, scalar_t(-1.), temp2, V_, scalar_t(1.),
             c, params::task_recursion_cutoff_level);
      }
      void Schur_update_rows_b
      (const std::vector<std::size_t>& rows, const DenseTile<scalar_t>& a,
       DenseMatrix<scalar_t>& c) const override {
        DenseM_t temp(rows.size(), rank());
        gemm(Trans::N, Trans::N, scalar_t(1.), a.D().extract_rows(rows), U(),
             scalar_t(0.), temp, params::task_recursion_cutoff_level);
        gemm(Trans::N, Trans::N, scalar_t(-1.), temp, V(),
             scalar_t(1.), c, params::task_recursion_cutoff_level);
      }

    private:
      DenseM_t U_, V_;
    };


  } // end namespace BLR
} // end namespace strumpack

#endif // LR_TILE_HPP
