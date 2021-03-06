#include <shogun/lib/common.h>
#include <shogun/lib/GCArray.h>
#include <shogun/kernel/Kernel.h>
#include <shogun/kernel/GaussianKernel.h>

#include <stdio.h>

using namespace shogun;

const int l=10;

int main(int argc, char** argv)
{
	// we need this scope, because exit_shogun() must not be called
	// before the destructor of CGCArray<CKernel*> kernels!
	{
		// create array of kernels
		CGCArray<CKernel*> kernels(l);

		// fill array with kernels
		for (int i=0; i<l; i++)
			kernels.set(new CGaussianKernel(10, 1.0), i);

		// print kernels
		for (int i=0; i<l; i++)
		{
			CKernel* kernel = kernels.get(i);
			printf("kernels[%d]=%p\n", i, kernel);
			SG_UNREF(kernel);
		}

	}

	return 0;
}
