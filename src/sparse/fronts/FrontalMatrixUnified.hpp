//
// Created by tingxuan on 23-6-19.
//

#pragma once

#include <Eigen/Sparse>
#include "FrontalMatrixDense.hpp"

#if defined(STRUMPACK_USE_CUDA)
#include "dense/CUDAWrapper.hpp"
#endif
#if defined(STRUMPACK_USE_HIP)
#include "dense/HIPWrapper.hpp"
#endif
#if defined(STRUMPACK_USE_SYCL)
#include "dense/DPCPPWrapper.hpp"
#endif

namespace strumpack {

    template<typename scalar_t, typename integer_t> class LevelInfoUnified;

    namespace gpu {
        template<typename scalar_t> struct FrontData;
        // template<typename scalar_t> struct FwdSolveData;
    }


    template<typename scalar_t,typename integer_t> class FrontalMatrixUnified
            : public FrontalMatrix<scalar_t,integer_t> {
        using F_t = FrontalMatrix<scalar_t,integer_t>;
        using FG_t = FrontalMatrixUnified<scalar_t,integer_t>;
        using DenseM_t = DenseMatrix<scalar_t>;
        using DenseMW_t = DenseMatrixWrapper<scalar_t>;
        using SpMat_t = CompressedSparseMatrix<scalar_t,integer_t>;
        using LInfo_t = LevelInfoUnified<scalar_t,integer_t>;

    public:
        FrontalMatrixUnified(integer_t sep, integer_t sep_begin, integer_t sep_end,
                         std::vector<integer_t>& upd);
        ~FrontalMatrixUnified();

        long long dense_node_factor_nonzeros() const override;

        void release_work_memory() override;

        void extend_add_to_dense(DenseM_t& paF11, DenseM_t& paF12,
                                 DenseM_t& paF21, DenseM_t& paF22,
                                 const F_t* p, int task_depth) override;

        void extend_add_to_dense_symmetric(DenseM_t& paF11,
                                           DenseM_t& paF21, DenseM_t& paF22,
                                           const F_t* p, int task_depth) override;

        ReturnCode multifrontal_factorization(const SpMat_t& A,
                                              const SPOptions<scalar_t>& opts,
                                              int etree_level=0,
                                              int task_depth=0) override;

        ReturnCode multifrontal_factorization_symmetric(const SpMat_t& A,
                                              const SPOptions<scalar_t>& opts,
                                              int etree_level=0,
                                              int task_depth=0);

        void extract_CB_sub_matrix(const std::vector<std::size_t>& I,
                                   const std::vector<std::size_t>& J,
                                   DenseM_t& B, int task_depth) const override {}

        std::string type() const override { return "FrontalMatrixUnified"; }

#if defined(STRUMPACK_USE_MPI)
        void
        extend_add_copy_to_buffers(std::vector<std::vector<scalar_t>>& sbuf,
                                   const FrontalMatrixMPI<scalar_t,integer_t>* pa)
        const override;
#endif

    private:
        std::unique_ptr<scalar_t[]> host_factors_, host_Schur_;
        std::unique_ptr<scalar_t[], std::function<void(scalar_t*)>> host_factors_diagonal_{nullptr, std::default_delete<scalar_t[]>{}}, host_factors_off_diagonal_{nullptr, std::default_delete<scalar_t[]>{}};
        DenseMW_t F11_, F12_, F21_, F22_;
        Eigen::SparseMatrix<scalar_t,Eigen::ColMajor,integer_t> F11L_sparse_{};
        Eigen::SparseMatrix<scalar_t,Eigen::ColMajor,integer_t> F11U_sparse_{};
        std::vector<int> pivot_mem_;
        int* piv_ = nullptr;

        FrontalMatrixUnified(const FrontalMatrixUnified&) = delete;
        FrontalMatrixUnified& operator=(FrontalMatrixUnified const&) = delete;

        void front_assembly(const SpMat_t& A, LInfo_t& L,
                            char* hea_mem, char* dea_mem);
        void front_assembly_symmetric(const SpMat_t& A, LInfo_t& L,
                                      char* hea_mem, char* dea_mem);
        void factor_small_fronts(LInfo_t& L, gpu::FrontData<scalar_t>* fdata,
                                 int* dinfo, const SPOptions<scalar_t>& opts);

        void factor_small_fronts_symmetric(LInfo_t& L, gpu::FrontData<scalar_t>* fdata,
                                           int* dinfo, const SPOptions<scalar_t>& opts);

        ReturnCode split_smaller(const SpMat_t& A, const SPOptions<scalar_t>& opts,
                                 int etree_level=0, int task_depth=0);

        ReturnCode split_smaller_symmetric(const SpMat_t& A, const SPOptions<scalar_t>& opts,
                                           int etree_level=0, int task_depth=0);

        void fwd_solve_phase2(DenseM_t& b, DenseM_t& bupd,
                              int etree_level, int task_depth) const override;
        void bwd_solve_phase1(DenseM_t& y, DenseM_t& yupd,
                              int etree_level, int task_depth) const override;

        ReturnCode node_inertia(integer_t& neg,
                                integer_t& zero,
                                integer_t& pos) const override;

        using F_t::lchild_;
        using F_t::rchild_;
        using F_t::dim_sep;
        using F_t::dim_upd;

        template<typename T,typename I> friend class LevelInfoUnified;
    };

} // end namespace strumpack

