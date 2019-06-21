/* @file   stereoVIOROS.cpp
 * @brief  ROS Wrapper for Spark-VIO
 * @author Yun Chang 
 */

#include <future>

// Still need gflags for parameters in VIO
#include <gflags/gflags.h>
#include <glog/logging.h>

// Dependencies from ROS
#include <ros/ros.h>

// Dependencies from VIO
#include <utils/Timer.h>
#include <LoggerMatlab.h>

// Dependencies from this repository
#include "RosBaseDataSource.h"
#include "RosbagDataSource.h"
#include "RosDataSource.h"

DEFINE_bool(parallel_run, true, "Run VIO parallel or sequential");
DEFINE_bool(online_run, true, "Run Spark-VIO-ROS online or offline");
DEFINE_string(rosbag_path, "rosbag", "Path to rosbag");

////////////////////////////////////////////////////////////////////////////////
// stereoVIOexample using ROS wrapper example
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[]) {
  // Initialize Google's flags library.
  google::ParseCommandLineFlags(&argc, &argv, true);
  // Initialize Google's logging library.
  google::InitGoogleLogging(argv[0]);

  // Initialize ROS node
  ros::init(argc, argv, "spark_vio");

  std::string left_camera_topic = "cam0/image_raw";
  std::string right_camera_topic = "cam1/image_raw";
  std::string imu_topic = "imu0";
  std::string reinit_flag_topic = "sparkvio/reinit_flag";
  std::string reinit_pose_topic = "sparkvio/reinit_pose";

  VIO::RosDataProvider ros_wrapper(left_camera_topic,
                                   right_camera_topic,
                                   imu_topic,
                                   reinit_flag_topic,
                                   reinit_pose_topic);

  bool is_pipeline_successful = false;

  auto tic = VIO::utils::Timer::tic();

  if (!FLAGS_parallel_run) {
    VIO::Pipeline vio_pipeline (ros_wrapper.getParams(), false); // run sequential

    // Register callback to vio_pipeline.
    ros_wrapper.registerVioCallback(
        std::bind(&VIO::Pipeline::spin, &vio_pipeline, std::placeholders::_1)); 

    // Spin dataset and handle threads
    is_pipeline_successful = ros_wrapper.spin();

  } else {
    VIO::Pipeline vio_pipeline (ros_wrapper.getParams(), true);

    // Register callback to vio_pipeline.
    ros_wrapper.registerVioCallback(
          std::bind(&VIO::Pipeline::spin, &vio_pipeline, std::placeholders::_1));

    // Spin dataset.
    auto handle = std::async(std::launch::async,
                             &VIO::RosDataProvider::spin, &ros_wrapper);

    vio_pipeline.spinViz();
    is_pipeline_successful = handle.get();
  }

  auto spin_duration = VIO::utils::Timer::toc(tic);

  LOG(WARNING) << "Spin took: " << spin_duration.count() << " ms.";

  if (is_pipeline_successful) {
    // Log overall time of pipeline run.
    VIO::LoggerMatlab logger;
    logger.openLogFiles(11);
    logger.logPipelineOverallTiming(spin_duration);
    logger.closeLogFiles();
  }

  return is_pipeline_successful? EXIT_SUCCESS : EXIT_FAILURE;
}
