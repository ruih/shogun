/*
 * This software is distributed under BSD 3-clause license (see LICENSE file).
 *
 * Authors: Heiko Strathmann, Soeren Sonnenburg, Soumyajit De, Pan Deng, 
 *          Sergey Lisitsyn, Khaled Nasr, Evgeniy Andreev, Evan Shelhamer, 
 *          Liang Pang
 */

#include <shogun/lib/common.h>
#include <shogun/kernel/CustomKernel.h>
#include <shogun/features/Features.h>
#include <shogun/features/DummyFeatures.h>
#include <shogun/features/IndexFeatures.h>
#include <shogun/io/SGIO.h>
#include <shogun/mathematics/linalg/LinalgNamespace.h>

using namespace shogun;
using namespace linalg;

void CCustomKernel::init()
{
	m_row_subset_stack=new CSubsetStack();
	SG_REF(m_row_subset_stack)
	m_col_subset_stack=new CSubsetStack();
	SG_REF(m_col_subset_stack)
	m_is_symmetric=false;
	m_free_km=true;

	SG_ADD((CSGObject**)&m_row_subset_stack, "row_subset_stack",
			"Subset stack of rows");
	SG_ADD((CSGObject**)&m_col_subset_stack, "col_subset_stack",
			"Subset stack of columns");
	SG_ADD(&m_free_km, "free_km", "Whether kernel matrix should be freed in "
			"destructor");
	SG_ADD(&m_is_symmetric, "is_symmetric", "Whether kernel matrix is symmetric");
	SG_ADD(&kmatrix, "kmatrix", "Kernel matrix.");
	SG_ADD(&upper_diagonal, "upper_diagonal", "Upper diagonal");
}

CCustomKernel::CCustomKernel()
: CKernel(10), kmatrix(), upper_diagonal(false)
{
	SG_DEBUG("created CCustomKernel")
	init();
}

CCustomKernel::CCustomKernel(CKernel* k)
: CKernel(10)
{
	SG_DEBUG("created CCustomKernel")
	init();

	/* if constructed from a custom kernel, use same kernel matrix */
	if (k->get_kernel_type()==K_CUSTOM)
	{
		CCustomKernel* casted=(CCustomKernel*)k;
		m_is_symmetric=casted->m_is_symmetric;
		set_full_kernel_matrix_from_full(casted->get_float32_kernel_matrix());
		m_free_km=false;
	}
	else
	{
		m_is_symmetric=k->get_lhs_equals_rhs();
		set_full_kernel_matrix_from_full(k->get_kernel_matrix());
	}
}

CCustomKernel::CCustomKernel(SGMatrix<float64_t> km)
: CKernel(10), upper_diagonal(false)
{
	SG_DEBUG("Entering")
	init();
	set_full_kernel_matrix_from_full(km, true);
	SG_DEBUG("Leaving")
}

CCustomKernel::CCustomKernel(SGMatrix<float32_t> km)
: CKernel(10), upper_diagonal(false)
{
	SG_DEBUG("Entering")
	init();
	set_full_kernel_matrix_from_full(km, true);
	SG_DEBUG("Leaving")
}

CCustomKernel::~CCustomKernel()
{
	SG_DEBUG("Entering")
	cleanup();
	SG_UNREF(m_row_subset_stack);
	SG_UNREF(m_col_subset_stack);
	SG_DEBUG("Leaving")
}

bool CCustomKernel::dummy_init(int32_t rows, int32_t cols)
{
	return init(new CDummyFeatures(rows), new CDummyFeatures(cols));
}

bool CCustomKernel::init(CFeatures* l, CFeatures* r)
{
	/* make it possible to call with NULL values since features are useless
	 * for custom kernel matrix */
	if (!l)
		l=lhs;

	if (!r)
		r=rhs;

	/* Make sure l and r should not be NULL */
	require(l, "CFeatures l should not be NULL");
	require(r, "CFeatures r should not be NULL");

	/* Make sure l and r have the same type of CFeatures */
	require(l->get_feature_class()==r->get_feature_class(),
			"Different FeatureClass: l is {}, r is {}",
			l->get_feature_class(),r->get_feature_class());
	require(l->get_feature_type()==r->get_feature_type(),
			"Different FeatureType: l is {}, r is {}",
			l->get_feature_type(),r->get_feature_type());

	/* If l and r are the type of CIndexFeatures,
	 * the init function adds a subset to kernel matrix.
	 * Then call get_kernel_matrix will get the submatrix
	 * of the kernel matrix.
	 */
	if (l->get_feature_class()==C_INDEX && r->get_feature_class()==C_INDEX)
	{
		CIndexFeatures* l_idx = (CIndexFeatures*)l;
		CIndexFeatures* r_idx = (CIndexFeatures*)r;

		remove_all_col_subsets();
		remove_all_row_subsets();

		add_row_subset(l_idx->get_feature_index());
		add_col_subset(r_idx->get_feature_index());

		lhs_equals_rhs=m_is_symmetric;

		return true;
	}

	/* For other types of CFeatures do the default actions below */
	CKernel::init(l, r);

	lhs_equals_rhs=m_is_symmetric;

	SG_DEBUG("num_vec_lhs: {} vs num_rows {}", l->get_num_vectors(), kmatrix.num_rows)
	SG_DEBUG("num_vec_rhs: {} vs num_cols {}", r->get_num_vectors(), kmatrix.num_cols)
	ASSERT(l->get_num_vectors()==kmatrix.num_rows)
	ASSERT(r->get_num_vectors()==kmatrix.num_cols)
	return init_normalizer();
}

float64_t CCustomKernel::sum_symmetric_block(index_t block_begin,
		index_t block_size, bool no_diag)
{
	SG_DEBUG("Entering");

	if (m_row_subset_stack->has_subsets() || m_col_subset_stack->has_subsets())
	{
		io::info("Row/col subsets initialized! Falling back to "
				"CKernel::sum_symmetric_block (slower)!");
		return CKernel::sum_symmetric_block(block_begin, block_size, no_diag);
	}

	require(kmatrix.matrix, "The kernel matrix is not initialized!");
	require(m_is_symmetric, "The kernel matrix is not symmetric!");
	require(block_begin>=0 && block_begin<kmatrix.num_cols,
			"Invalid block begin index ({}, {})!", block_begin, block_begin);
	require(block_begin+block_size<=kmatrix.num_cols,
			"Invalid block size ({}) at starting index ({}, {})! "
			"Please use smaller blocks!", block_size, block_begin, block_begin);
	require(block_size>=1, "Invalid block size ({})!", block_size);

	SG_DEBUG("Leaving");

	return sum_symmetric(block(kmatrix, block_begin,
				block_begin, block_size, block_size), no_diag);
}

float64_t CCustomKernel::sum_block(index_t block_begin_row,
		index_t block_begin_col, index_t block_size_row,
		index_t block_size_col, bool no_diag)
{
	SG_DEBUG("Entering");

	if (m_row_subset_stack->has_subsets() || m_col_subset_stack->has_subsets())
	{
		io::info("Row/col subsets initialized! Falling back to "
				"CKernel::sum_block (slower)!");
		return CKernel::sum_block(block_begin_row, block_begin_col,
				block_size_row, block_size_col, no_diag);
	}

	require(kmatrix.matrix, "The kernel matrix is not initialized!");
	require(block_begin_row>=0 && block_begin_row<kmatrix.num_rows &&
			block_begin_col>=0 && block_begin_col<kmatrix.num_cols,
			"Invalid block begin index ({}, {})!",
			block_begin_row, block_begin_col);
	require(block_begin_row+block_size_row<=kmatrix.num_rows &&
			block_begin_col+block_size_col<=kmatrix.num_cols,
			"Invalid block size ({}, {}) at starting index ({}, {})! "
			"Please use smaller blocks!", block_size_row, block_size_col,
			block_begin_row, block_begin_col);
	require(block_size_row>=1 && block_size_col>=1,
			"Invalid block size ({}, {})!", block_size_row, block_size_col);

	// check if removal of diagonal is required/valid
	if (no_diag && block_size_row!=block_size_col)
	{
		io::warn("Not removing the main diagonal since block is not square!");
		no_diag=false;
	}

	SG_DEBUG("Leaving");

	return sum(block(kmatrix, block_begin_row, block_begin_col,
				block_size_row, block_size_col), no_diag);
}

SGVector<float64_t> CCustomKernel::row_wise_sum_symmetric_block(index_t
		block_begin, index_t block_size, bool no_diag)
{
	SG_DEBUG("Entering");

	if (m_row_subset_stack->has_subsets() || m_col_subset_stack->has_subsets())
	{
		io::info("Row/col subsets initialized! Falling back to "
				"CKernel::row_wise_sum_symmetric_block (slower)!");
		return CKernel::row_wise_sum_symmetric_block(block_begin, block_size,
				no_diag);
	}

	require(kmatrix.matrix, "The kernel matrix is not initialized!");
	require(m_is_symmetric, "The kernel matrix is not symmetric!");
	require(block_begin>=0 && block_begin<kmatrix.num_cols,
			"Invalid block begin index ({}, {})!", block_begin, block_begin);
	require(block_begin+block_size<=kmatrix.num_cols,
			"Invalid block size ({}) at starting index ({}, {})! "
			"Please use smaller blocks!", block_size, block_begin, block_begin);
	require(block_size>=1, "Invalid block size ({})!", block_size);

	SGVector<float32_t> s=rowwise_sum(block(kmatrix, block_begin,
				block_begin, block_size, block_size), no_diag);

	// casting to float64_t vector
	SGVector<float64_t> sum(s.vlen);
	for (index_t i=0; i<s.vlen; ++i)
		sum[i]=s[i];

	SG_DEBUG("Leaving");

	return sum;
}

SGMatrix<float64_t> CCustomKernel::row_wise_sum_squared_sum_symmetric_block(
		index_t block_begin, index_t block_size, bool no_diag)
{
	SG_DEBUG("Entering");

	if (m_row_subset_stack->has_subsets() || m_col_subset_stack->has_subsets())
	{
		io::info("Row/col subsets initialized! Falling back to "
				"CKernel::row_wise_sum_squared_sum_symmetric_block (slower)!");
		return CKernel::row_wise_sum_squared_sum_symmetric_block(block_begin,
				block_size, no_diag);
	}

	require(kmatrix.matrix, "The kernel matrix is not initialized!");
	require(m_is_symmetric, "The kernel matrix is not symmetric!");
	require(block_begin>=0 && block_begin<kmatrix.num_cols,
			"Invalid block begin index ({}, {})!", block_begin, block_begin);
	require(block_begin+block_size<=kmatrix.num_cols,
			"Invalid block size ({}) at starting index ({}, {})! "
			"Please use smaller blocks!", block_size, block_begin, block_begin);
	require(block_size>=1, "Invalid block size ({})!", block_size);

	// initialize the matrix that accumulates the row/col-wise sum
	// the first column stores the sum of kernel values
	// the second column stores the sum of squared kernel values
	SGMatrix<float64_t> row_sum(block_size, 2);

	SGVector<float32_t> sum=rowwise_sum(block(kmatrix,
				block_begin, block_begin, block_size, block_size), no_diag);

	auto kmatrix_block = block(kmatrix, block_begin, block_begin, block_size, block_size);
	SGVector<float32_t> sq_sum=rowwise_sum(
		element_prod(kmatrix_block, kmatrix_block), no_diag);

	for (index_t i=0; i<sum.vlen; ++i)
		row_sum(i, 0)=sum[i];

	for (index_t i=0; i<sq_sum.vlen; ++i)
		row_sum(i, 1)=sq_sum[i];

	SG_DEBUG("Leaving");

	return row_sum;
}

SGVector<float64_t> CCustomKernel::row_col_wise_sum_block(index_t
		block_begin_row, index_t block_begin_col, index_t block_size_row,
		index_t block_size_col, bool no_diag)
{
	SG_DEBUG("Entering");

	if (m_row_subset_stack->has_subsets() || m_col_subset_stack->has_subsets())
	{
		io::info("Row/col subsets initialized! Falling back to "
				"CKernel::row_col_wise_sum_block (slower)!");
		return CKernel::row_col_wise_sum_block(block_begin_row, block_begin_col,
				block_size_row, block_size_col, no_diag);
	}

	require(kmatrix.matrix, "The kernel matrix is not initialized!");
	require(block_begin_row>=0 && block_begin_row<kmatrix.num_rows &&
			block_begin_col>=0 && block_begin_col<kmatrix.num_cols,
			"Invalid block begin index ({}, {})!",
			block_begin_row, block_begin_col);
	require(block_begin_row+block_size_row<=kmatrix.num_rows &&
			block_begin_col+block_size_col<=kmatrix.num_cols,
			"Invalid block size ({}, {}) at starting index ({}, {})! "
			"Please use smaller blocks!", block_size_row, block_size_col,
			block_begin_row, block_begin_col);
	require(block_size_row>=1 && block_size_col>=1,
			"Invalid block size ({}, {})!", block_size_row, block_size_col);

	// check if removal of diagonal is required/valid
	if (no_diag && block_size_row!=block_size_col)
	{
		io::warn("Not removing the main diagonal since block is not square!");
		no_diag=false;
	}

	// initialize the vector that accumulates the row/col-wise sum
	// the first block_size_row entries store the row-wise sum of kernel values
	// the nextt block_size_col entries store the col-wise sum of kernel values
	SGVector<float64_t> sum(block_size_row+block_size_col);

	SGVector<float32_t> rowwise=rowwise_sum(block(kmatrix,
				block_begin_row, block_begin_col, block_size_row,
				block_size_col), no_diag);

	SGVector<float32_t> colwise=colwise_sum(block(kmatrix,
				block_begin_row, block_begin_col, block_size_row,
				block_size_col), no_diag);

	for (index_t i=0; i<rowwise.vlen; ++i)
		sum[i]=rowwise[i];

	for (index_t i=0; i<colwise.vlen; ++i)
		sum[i+rowwise.vlen]=colwise[i];

	SG_DEBUG("Leaving");

	return sum;
}

void CCustomKernel::cleanup_custom()
{
	SG_DEBUG("Entering")
	remove_all_row_subsets();
	remove_all_col_subsets();

	kmatrix=SGMatrix<float32_t>();
	upper_diagonal=false;

	SG_DEBUG("Leaving")
}

void CCustomKernel::cleanup()
{
	cleanup_custom();
	CKernel::cleanup();
}

void CCustomKernel::add_row_subset(SGVector<index_t> subset)
{
	m_row_subset_stack->add_subset(subset);
	row_subset_changed_post();
}

void CCustomKernel::add_row_subset_in_place(SGVector<index_t> subset)
{
	m_row_subset_stack->add_subset_in_place(subset);
	row_subset_changed_post();
}

void CCustomKernel::remove_row_subset()
{
	m_row_subset_stack->remove_subset();
	row_subset_changed_post();
}

void CCustomKernel::remove_all_row_subsets()
{
	m_row_subset_stack->remove_all_subsets();
	row_subset_changed_post();
}

void CCustomKernel::row_subset_changed_post()
{
	if (m_row_subset_stack->has_subsets())
		num_lhs=m_row_subset_stack->get_size();
	else
		num_lhs=kmatrix.num_rows;
}

void CCustomKernel::add_col_subset(SGVector<index_t> subset)
{
	m_col_subset_stack->add_subset(subset);
	col_subset_changed_post();
}

void CCustomKernel::add_col_subset_in_place(SGVector<index_t> subset)
{
	m_col_subset_stack->add_subset_in_place(subset);
	col_subset_changed_post();
}

void CCustomKernel::remove_col_subset()
{
	m_col_subset_stack->remove_subset();
	col_subset_changed_post();
}

void CCustomKernel::remove_all_col_subsets()
{
	m_col_subset_stack->remove_all_subsets();
	col_subset_changed_post();
}

void CCustomKernel::col_subset_changed_post()
{
	if (m_col_subset_stack->has_subsets())
		num_rhs=m_col_subset_stack->get_size();
	else
		num_rhs=kmatrix.num_cols;
}
