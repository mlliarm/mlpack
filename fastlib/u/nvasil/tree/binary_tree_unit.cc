/*
 * =====================================================================================
 *
 *       Filename:  binary_tree_unit.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/27/2007 10:20:40 AM EDT
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Nikolaos Vasiloglou (NV), nvasil@ieee.org
 *        Company:  Georgia Tech Fastlab-ESP Lab
 *
 * =====================================================================================
 */

#include <unistd.h>
#include <sys/mman.h>
#include <limits>
#include "fastlib/fastlib.h"
#include "u/nvasil/mmanager/memory_manager.h"
#include "u/nvasil/test/test.h"
#include "u/nvasil/dataset/binary_dataset.h"
#include "tree_parameters_macro.h"
#include "euclidean_metric.h"
#include "null_statistics.h"
#include "hyper_rectangle.h"
#include "point_identity_discriminator.h"
#include "kd_pivoter1.h"
#include "binary_tree.h"

using namespace std;

template<typename TYPELIST, bool diagnostic>
class BinaryTreeTest {
 public:
  typedef typename TYPELIST::Precision_t   Precision_t;
	typedef typename TYPELIST::Allocator_t   Allocator_t;
  typedef typename TYPELIST::Metric_t      Metric_t;
	typedef typename TYPELIST::BoundingBox_t BoundingBox_t;
	typedef typename TYPELIST::NodeCachedStatistics_t NodeCachedStatistics_t;
	typedef typename TYPELIST::PointIdDiscriminator_t PointIdDiscriminator_t;
	typedef typename TYPELIST::Pivot_t Pivot_t;
	typedef Point<Precision_t, Allocator_t> Point_t; 
	typedef BinaryTree<TYPELIST, diagnostic> BinaryTree_t;
	typedef typename BinaryTree_t::Node_t Node_t;
  BinaryTreeTest() {
	}	
	void Init() {
    Allocator_t::allocator_ = new Allocator_t();
		Allocator_t::allocator_->Initialize();
		dimension_=2;
    num_of_points_=1000;
    data_file_="data";
		knns_=40;
    range_=0.2;
		result_file_="allnn";
    data_.Init(data_file_, num_of_points_, dimension_);
    for(index_t i=0; i<num_of_points_; i++) {
		  for(index_t j=0; j<dimension_; j++) {
			  data_.At(i,j)=Precision_t(rand())/RAND_MAX - 0.48;
			}
			data_.set_id(i,i);
		}		
    tree_.Init(&data_);
	}
	void Destruct() {
	  tree_.Destruct();
	  data_.Destruct();
	  unlink(data_file_.c_str());
	  unlink(data_file_.append(".ind").c_str());	
		unlink(result_file_.c_str());
	}
	void BuildDepthFirst(){
    printf("Testing BuildDepthFirst...\n");
		tree_.BuildDepthFirst();
	}
	void BuildBreadthFirst() {
		printf("Testing BuildBreadthFirst...\n");
	  tree_.BuildBreadthFirst();
	}
	void kNearestNeighbor() {
		printf("Testing kNearestNeighbor...\n");
		tree_.BuildDepthFirst();
    vector<pair<Precision_t, Point_t> > nearest_tree;
		pair<Precision_t, index_t> nearest_naive[num_of_points_];
	  for(index_t i=0; i<num_of_points_; i++) {
		  tree_.NearestNeighbor(data_.get_point(i),
                            &nearest_tree,
                            knns_);
			Naive(i, nearest_naive);
			for(index_t j=0; j<knns_; j++) {
			  TEST_DOUBLE_APPROX(nearest_naive[j+1].first, 
			 	  	               nearest_tree[j].first,
					                 numeric_limits<Precision_t>::epsilon());
			  TEST_ASSERT(nearest_tree[j].second.get_id()==
				            nearest_naive[j+1].second)	;
			}
		}
	}
	void RangeNearestNeighbor() {
		printf("Testing RangeNearestNeighbor...\n");
		tree_.BuildBreadthFirst();
	  vector<pair<Precision_t, Point_t> > nearest_tree;
		pair<Precision_t, index_t> nearest_naive[num_of_points_];
	  for(index_t i=0; i<num_of_points_; i++) {
		  tree_.NearestNeighbor(data_.get_point(i),
                            &nearest_tree,
                            range_);
			Naive(i, nearest_naive);
			for(index_t j=0; j<(index_t)nearest_tree.size(); j++) {
			  TEST_DOUBLE_APPROX(nearest_naive[j+1].first, 
			 	  	                 nearest_tree[j].first,
					                   numeric_limits<Precision_t>::epsilon());
			  TEST_ASSERT(nearest_tree[j].second.get_id()==
				            nearest_naive[j+1].second)	;
			}
		}
	}
	void AllKNearestNeighbors() {
		printf("Testing AllKNearestNeighbors...\n");
		tree_.BuildBreadthFirst();
	  tree_.InitAllKNearestNeighborOutput(result_file_, 
				                                 knns_);
		tree_.AllNearestNeighbors(tree_.parent_, knns_);
    tree_.CloseAllKNearestNeighborOutput(knns_);
    struct stat info;
		if (stat(data_file_.c_str(), &info)!=0) {
      FATAL("Error %s file %s\n",
				    strerror(errno), data_file_.c_str());
		}
		uint64 map_size = info.st_size-sizeof(int32);

    int fp=open(result_file_.c_str(), O_RDWR);
    typename Node_t::NNResult *res;
		res=(typename Node_t::NNResult *) mmap(NULL, 
				                                map_size, 
											         				  PROT_READ | PROT_WRITE, 
															          MAP_SHARED, fp,
			                                  0);
		close(fp);
		std::sort(res, res+num_of_points_*knns_);
		pair<Precision_t, index_t> nearest_naive[num_of_points_];
    for(index_t i=0; i<num_of_points_; i++) {
		  Naive(res[i].point_id_, nearest_naive);
		 	for(index_t j=0; j<knns_; j++) {
			 	TEST_DOUBLE_APPROX(nearest_naive[j+1].first, 
			 	  	                 res[i*knns_+j].distance_,
					                   numeric_limits<Precision_t>::epsilon());
			  TEST_ASSERT(res[j].nearest_.get_id()==
				            nearest_naive[j+1].second);
			}			
		}		
		munmap(res, map_size);
	}
	
	void AllRangeNearestNeighbors() {
	  printf("Testing AllRangeNearestNeighbors...\n");
	 	tree_.BuildBreadthFirst();
	  tree_.InitAllRangeNearestNeighborOutput(result_file_);
		tree_.AllNearestNeighbors(tree_.parent_, range_);
    tree_.CloseAllRangeNearestNeighborOutput();
    struct stat info;
		if (stat(data_file_.c_str(), &info)!=0) {
      FATAL( "Error %s file %s\n",
				    strerror(errno), data_file_.c_str());
		}
		uint64 map_size = info.st_size-sizeof(int32);

    int fp=open(result_file_.c_str(), O_RDWR);
    typename Node_t::NNResult *res;
		res=(typename Node_t::NNResult *)mmap(NULL, 
				                                map_size, 
											          				PROT_READ | PROT_WRITE, 
															          MAP_SHARED, fp,
			                                  0);
		close(fp);
		std::sort(res, res+num_of_points_*knns_);
		pair<Precision_t, index_t> nearest_naive[num_of_points_];
		index_t i=0;
    while (i<num_of_points_) {
		  Naive(res[i].point_id_, nearest_naive);
			index_t j=0;
		 	while (nearest_naive[j+1].first<range_) {
			 	TEST_DOUBLE_APPROX(nearest_naive[j+1].first, 
			 	  	                 res[i].distance_,
					                   numeric_limits<Precision_t>::epsilon());
			  TEST_ASSERT(res[i].nearest_.get_id()==
				            nearest_naive[j+1].second);
				i++;
				j++;
			}			
		}		
		munmap(res, map_size);
	}

  void TestAll() {
	  Init();
		BuildDepthFirst();
		Destruct();
		Init();
		BuildBreadthFirst();
		Destruct();
		Init();
		kNearestNeighbor();
		Destruct();
		Init();
		RangeNearestNeighbor();
		Destruct();
		Init();
		AllKNearestNeighbors();
		Destruct();
		Init();
		AllRangeNearestNeighbors();
    Destruct();
	}	
 private:
  BinaryTree_t tree_; 
  BinaryDataset<Precision_t> data_;
  string data_file_;
	string result_file_;
	int32 dimension_;
	index_t num_of_points_;
	index_t knns_;
	Precision_t range_;
	
	void Naive(index_t query, 
			       pair<Precision_t, index_t> *result) {
    
		for(index_t i=0;  i<num_of_points_; i++) {
		  if (unlikely(data_.get_id(i)==data_.get_id(query))) {
			  continue;
			}
			Precision_t dist=Metric_t::Distance(data_.At(i), 
					                                data_.At(query), 
					                                dimension_);
		  result[i].first=dist;
		  result[i].second=i;
		}
		sort(result, result+num_of_points_);
	}
	
};
/*
TREE_PARAMETERS(float32,
		            MemoryManager<false>,
		            EuclideanMetric<float32>,
	              HyperRectangle,
	              NullStatistics,
                SimpleDiscriminator,
		            KdPivoter1) 
								*/
struct BasicTypes {
  typedef float32 Precision_t;
	typedef MemoryManager<false> Allocator_t;
	typedef EuclideanMetric<float32> Metric_t;
};
struct Parameters {
  typedef float32 Precision_t;
	typedef MemoryManager<false> Allocator_t;
	typedef EuclideanMetric<float32> Metric_t;
	typedef HyperRectangle<BasicTypes, false> BoundingBox_t;
	typedef NullStatistics NodeCachedStatistics_t;
  typedef SimpleDiscriminator PointIdDiscriminator_t;
  typedef KdPivoter1<BasicTypes, false> Pivot_t; 
};
typedef BinaryTreeTest<Parameters, false> BinaryTreeTest_t;
int main(int argc, char *argv[]) {
  BinaryTreeTest_t test;
	test.TestAll();
}
