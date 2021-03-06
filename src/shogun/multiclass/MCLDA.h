/*
 * This software is distributed under BSD 3-clause license (see LICENSE file).
 *
 * Authors: Kevin Hughes, Heiko Strathmann, Thoralf Klein, Soeren Sonnenburg, 
 *          Bjoern Esser
 */

#ifndef _MCLDA_H__
#define _MCLDA_H__

#include <shogun/lib/config.h>


#include <shogun/features/DotFeatures.h>
#include <shogun/features/DenseFeatures.h>
#include <shogun/machine/NativeMulticlassMachine.h>
#include <shogun/lib/SGNDArray.h>

namespace shogun
{

//#define DEBUG_MCLDA

/** @brief Class MCLDA implements multiclass Linear Discriminant Analysis.
 *
 * MCLDA learns a linear classifier and requires examples to be CDenseFeatures.
 * The learned linear classification rule is optimal under the assumption that
 * the classes are gaussian distributed with equal co-variance
 *
 */

class CMCLDA : public CNativeMulticlassMachine
{
	public:
		MACHINE_PROBLEM_TYPE(PT_MULTICLASS)

		/** constructor
		 *
		 * @param tolerance tolerance used in training
		 * @param store_cov whether to store the within class covariances
		 */
		CMCLDA(float64_t tolerance = 1e-4, bool store_cov = false);

		/** constructor
		 *
		 * @param traindat training features
		 * @param trainlab labels for training features
		 * @param tolerance tolerance used in training
		 * @param store_cov whether to store the within class covariances
		 */
		CMCLDA(CDenseFeatures<float64_t>* traindat, CLabels* trainlab, float64_t tolerance = 1e-4, bool store_cov = false);

		virtual ~CMCLDA();

		/** apply CMCLDA to data
		 *
		 * @param data (test) data to be classified
		 * @return labels result of classification
		 */
		virtual CMulticlassLabels* apply_multiclass(CFeatures* data=NULL);

		/** set tolerance
		 *
		 * @param tolerance tolerance used during training
		 */
		inline void set_tolerance(float64_t tolerance) { m_tolerance = tolerance; }

		/** get tolerance
		 *
		 * @return tolerance used during training
		 */
		inline bool get_tolerance() { return m_tolerance; }

		/** get classifier type
		 *
		 * @return classifier type MCLDA
		 */
		virtual EMachineType get_classifier_type() { return CT_LDA; } // for now add to machine typers properly later

		/** set features
		 *
		 * @param feat features to set
		 */
		virtual void set_features(CDotFeatures* feat)
		{
			if (feat->get_feature_class() != C_DENSE ||
				feat->get_feature_type() != F_DREAL)
				error("MCLDA requires SIMPLE REAL valued features");

			SG_REF(feat);
			SG_UNREF(m_features);
			m_features = feat;
		}

		/** get features
		 *
		 * @return features
		 */
		virtual CDotFeatures* get_features() { SG_REF(m_features); return m_features; }

		/** get object name
		 *
		 * @return object name
		 */
		virtual const char* get_name() const { return "MCLDA"; }

		/** get a class' mean vector
		 *
		 * @param c class index
		 *
		 * @return mean vector of class c
		 */
		inline SGVector< float64_t > get_mean(int32_t c) const
		{
			return SGVector< float64_t >(m_means.get_column_vector(c), m_dim, false);
		}

		/** get covariance matrix
		 *
		 * @return covariance matrix
		 */
		inline SGMatrix< float64_t > get_cov() const
		{
			return m_cov;
		}

		protected:
		/** train MCLDA classifier
		 *
		 * @param data training data
		 *
		 * @return whether training was successful
		 */
		virtual bool train_machine(CFeatures* data = NULL);

	private:
		void init();

		void cleanup();

		private:
			/** feature vectors */
		CDotFeatures* m_features;

		/** tolerance used during training */
		float64_t m_tolerance;

			/** whether to store the within class covariances */
		bool m_store_cov;

		/** number of classes */
		int32_t m_num_classes;

		/** dimension of the features space */
		int32_t m_dim;

		/** feature covariances for each of the classes in the training data
		 *  stored if store_cov
		 */
		SGMatrix< float64_t > m_cov;

		/** feature means for each of the classes in the training data */
		SGMatrix< float64_t > m_means;

		/** total mean */
		SGVector< float64_t > m_xbar;

		/** rank */
		int32_t m_rank;

		/** scalings */
		SGMatrix< float64_t > m_scalings;

		/** weight vectors / coefs */
		SGMatrix< float64_t > m_coef;

		/** intercept */
		SGVector< float64_t > m_intercept;

}; /* class MCLDA */
}  /* namespace shogun */

#endif /* _MCLDA_H__ */
