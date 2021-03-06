#include <iostream>
#include <time.h>
#include <thread>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/registration/ndt.h>
#include <pcl/registration/gicp.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/visualization/pcl_visualizer.h>

#include <pclomp/ndt_omp.h>
#include <pclomp/gicp_omp.h>

// align point clouds and measure processing time
pcl::PointCloud<pcl::PointXYZ>::Ptr align(pcl::Registration<pcl::PointXYZ, pcl::PointXYZ>::Ptr registration, const pcl::PointCloud<pcl::PointXYZ>::Ptr& target_cloud, const pcl::PointCloud<pcl::PointXYZ>::Ptr& source_cloud ) {
  registration->setInputTarget(target_cloud);
  registration->setInputSource(source_cloud);
  pcl::PointCloud<pcl::PointXYZ>::Ptr aligned(new pcl::PointCloud<pcl::PointXYZ>());

  clock_t t1,t2,t3;
  t1 = clock();
  registration->align(*aligned);
  t2 = clock();
  std::cout << "single : " << ((double)(t2-t1)) / CLOCKS_PER_SEC << "[sec]" << std::endl;
    
  /*
  for(int i=0; i<10; i++) {
    registration->align(*aligned);
  }
  t3 = clock();
  std::cout << "10times: " << ((double)(t3-t2))/CLOCKS_PER_SEC << "[sec]" << std::endl;
  */
  std::cout << "fitness: " << registration->getFitnessScore() << std::endl << std::endl;
    
  std::cout << "transformation matrix is: \n" << registration->getFinalTransformation() << std::endl;
  return aligned;
}


int main(int argc, char** argv) {
  std::string target_pcd = "room_scan1.pcd";
  std::string source_pcd = "room_scan2.pcd";

  pcl::PointCloud<pcl::PointXYZ>::Ptr target_cloud(new pcl::PointCloud<pcl::PointXYZ>());
  pcl::PointCloud<pcl::PointXYZ>::Ptr source_cloud(new pcl::PointCloud<pcl::PointXYZ>());

  if(pcl::io::loadPCDFile(target_pcd, *target_cloud)) {
    std::cerr << "failed to load " << target_pcd << std::endl;
    return 0;
  }
  if(pcl::io::loadPCDFile(source_pcd, *source_cloud)) {
    std::cerr << "failed to load " << source_pcd << std::endl;
    return 0;
  }

  // downsampling
  pcl::PointCloud<pcl::PointXYZ>::Ptr downsampled(new pcl::PointCloud<pcl::PointXYZ>());

  pcl::VoxelGrid<pcl::PointXYZ> voxelgrid;
  voxelgrid.setLeafSize(0.1f, 0.1f, 0.1f);

  voxelgrid.setInputCloud(target_cloud);
  voxelgrid.filter(*downsampled);
  *target_cloud = *downsampled;

  voxelgrid.setInputCloud(source_cloud);
  voxelgrid.filter(*downsampled);
  source_cloud = downsampled;

  //ros::Time::init();

  // benchmark
  /*
  std::cout << "--- pcl::GICP ---" << std::endl;
  pcl::GeneralizedIterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ>::Ptr gicp(new pcl::GeneralizedIterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ>());
  pcl::PointCloud<pcl::PointXYZ>::Ptr aligned = align(gicp, target_cloud, source_cloud);

  std::cout << "--- pclomp::GICP ---" << std::endl;
  pclomp::GeneralizedIterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ>::Ptr gicp_omp(new pclomp::GeneralizedIterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ>());
  aligned = align(gicp_omp, target_cloud, source_cloud);
  */

  std::cout << "--- pcl::NDT ---" << std::endl;
  pcl::NormalDistributionsTransform<pcl::PointXYZ, pcl::PointXYZ>::Ptr ndt(new pcl::NormalDistributionsTransform<pcl::PointXYZ, pcl::PointXYZ>());
  ndt->setResolution(1.0);
  pcl::PointCloud<pcl::PointXYZ>::Ptr aligned = align(ndt, target_cloud, source_cloud);

  std::vector<int> num_threads = {1, omp_get_max_threads()};
  std::vector<std::pair<std::string, pclomp::NeighborSearchMethod>> search_methods = {
    {"KDTREE", pclomp::KDTREE},
    {"DIRECT7", pclomp::DIRECT7},
    {"DIRECT1", pclomp::DIRECT1}
  };

  pclomp::NormalDistributionsTransform<pcl::PointXYZ, pcl::PointXYZ>::Ptr ndt_omp(new pclomp::NormalDistributionsTransform<pcl::PointXYZ, pcl::PointXYZ>());
  ndt_omp->setResolution(1.0);

  for(int n : num_threads) {
    for(const auto& search_method : search_methods) {
      std::cout << "--- pclomp::NDT (" << search_method.first << ", " << n << " threads) ---" << std::endl;
      ndt_omp->setNumThreads(n);
      ndt_omp->setNeighborhoodSearchMethod(search_method.second);
      aligned = align(ndt_omp, target_cloud, source_cloud);
    }
  }

  // visualization
  pcl::visualization::PCLVisualizer vis("vis");
  //pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> target_handler(target_cloud, 255.0, 0.0, 0.0);
  pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> source_handler(source_cloud, 0.0, 255.0, 0.0);
  pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> aligned_handler(aligned, 0.0, 0.0, 255.0);
  //vis.addPointCloud(target_cloud, target_handler, "target");
  vis.addPointCloud(source_cloud, source_handler, "source");
  vis.addPointCloud(aligned, aligned_handler, "aligned");
  vis.spin();

  return 0;
}

