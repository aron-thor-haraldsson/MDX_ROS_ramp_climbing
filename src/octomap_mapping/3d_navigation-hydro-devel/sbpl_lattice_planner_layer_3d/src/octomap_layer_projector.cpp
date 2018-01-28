/**
* octomap_server: A Tool to serve 3D OctoMaps in ROS (binary and as visualization)
* (inspired by the ROS map_saver)
* @author A. Hornung, University of Freiburg, Copyright (C) 2010-2011.
* @see http://octomap.sourceforge.net/
* License: BSD
*/

/*
 * Copyright (c) 2010-2011, A. Hornung, University of Freiburg
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of Freiburg nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <octomap_server/OctomapServerCombined.h>

namespace sbpl_lattice_planner_layer_3d
{

OctomapLayerProjector::OctomapLayerProjector( octomap::OcTree* octree )
  : m_nh()
  , m_maxRange(-1.0)
  , m_worldFrameId("/map")
  , m_baseFrameId("base_footprint")
  , m_useHeightMap(true)
  , m_colorFactor(0.8)
  , m_pointcloudMinZ(-std::numeric_limits<double>::max())
  , m_pointcloudMaxZ(std::numeric_limits<double>::max())
  , m_occupancyMinZ(-std::numeric_limits<double>::max())
  , m_occupancyMaxZ(std::numeric_limits<double>::max())
  , m_minSizeX(0.0)
  , m_minSizeY(0.0)
  , m_filterSpeckles(false)
  , m_filterGroundPlane(true)
  , m_groundFilterDistance(0.04)
  , m_groundFilterAngle(0.15)
  , m_groundFilterPlaneDistance(0.07)
  , octree_( octree )
{
  haveAttachedObject = false;
  ros::NodeHandle private_nh("~");
  private_nh.param("frame_id", m_worldFrameId, m_worldFrameId);
  private_nh.param("base_frame_id", m_baseFrameId, m_baseFrameId);
  private_nh.param("height_map", m_useHeightMap, m_useHeightMap);
  private_nh.param("color_factor", m_colorFactor, m_colorFactor);

  private_nh.param("pointcloud_min_z", m_pointcloudMinZ,m_pointcloudMinZ);
  private_nh.param("pointcloud_max_z", m_pointcloudMaxZ,m_pointcloudMaxZ);
  private_nh.param("occupancy_min_z", m_occupancyMinZ,m_occupancyMinZ);
  private_nh.param("occupancy_max_z", m_occupancyMaxZ,m_occupancyMaxZ);
  private_nh.param("min_x_size", m_minSizeX,m_minSizeX);
  private_nh.param("min_y_size", m_minSizeY,m_minSizeY);

  private_nh.param("filter_speckles", m_filterSpeckles, m_filterSpeckles);
  private_nh.param("filter_ground", m_filterGroundPlane, m_filterGroundPlane);
  // distance of points from plane for RANSAC
  private_nh.param("ground_filter/distance", m_groundFilterDistance, m_groundFilterDistance);
  // angular derivation of found plane:
  private_nh.param("ground_filter/angle", m_groundFilterAngle, m_groundFilterAngle);
  // distance of found plane from z=0 to be detected as ground (e.g. to exclude tables)
  private_nh.param("ground_filter/plane_distance", m_groundFilterPlaneDistance, m_groundFilterPlaneDistance);

// I don't think the resolution is up to me at this point.  Should be set by owner of octree.
//  double res = 0.05;
//  private_nh.param("resolution", res, res);
//  octree_->setResolution(res);

  private_nh.param("sensor_model/max_range", m_maxRange, m_maxRange);

// Should these live here, or in planner?
  double probHit = 0.7;
  double probMiss = 0.4;
  double thresMin = 0.12;
  double thresMax = 0.97;
  private_nh.param("sensor_model/hit", probHit, probHit);
  private_nh.param("sensor_model/miss", probMiss, probMiss);
  private_nh.param("sensor_model/min", thresMin, thresMin);
  private_nh.param("sensor_model/max", thresMax, thresMax);
  octree_->setProbHit(probHit);
  octree_->setProbMiss(probMiss);
  octree_->setClampingThresMin(thresMin);
  octree_->setClampingThresMax(thresMax);

  double r, g, b, a;
  private_nh.param("color/r", r, 0.0);
  private_nh.param("color/g", g, 0.0);
  private_nh.param("color/b", b, 1.0);
  private_nh.param("color/a", a, 1.0);
  m_color.r = r;
  m_color.g = g;
  m_color.b = b;
  m_color.a = a;

  bool staticMap = false;
  //if (filename != "")
  //staticMap = true;

  m_markerPub = m_nh.advertise<visualization_msgs::MarkerArray>("occupied_cells_vis_array", 1, staticMap);
  m_binaryMapPub = m_nh.advertise<octomap_ros::OctomapBinary>("octomap_binary", 1, staticMap);
  m_pointCloudPub = m_nh.advertise<sensor_msgs::PointCloud2>("octomap_point_cloud_centers", 1, staticMap);
  m_collisionObjectPub = m_nh.advertise<mapping_msgs::CollisionObject>("octomap_collision_object", 1, staticMap);
  m_mapPub = m_nh.advertise<nav_msgs::OccupancyGrid>("map", 5, staticMap);
  base_mapPub = m_nh.advertise<nav_msgs::OccupancyGrid>("base_map", 5, staticMap);
  spine_mapPub = m_nh.advertise<nav_msgs::OccupancyGrid>("spine_map", 5, staticMap);
  arm_mapPub = m_nh.advertise<nav_msgs::OccupancyGrid>("arm_map", 5, staticMap);
  boundPub = m_nh.advertise<visualization_msgs::Marker>("arm_bound_box",1);

  m_octomapService = m_nh.advertiseService("octomap_binary", &OctomapLayerProjector::serviceCallback, this);
  m_clearBBXService = private_nh.advertiseService("clear_bbx", &OctomapLayerProjector::clearBBXSrv, this);

  // a filename to load is set => distribute a static map latched
  if (filename != ""){
    //if (staticMap){
    if (octree_->readBinary(filename)){
      ROS_INFO("Octomap file %s loaded (%zu nodes).", filename.c_str(),octree_->size());

      publishAll();
    } else{
      ROS_ERROR("Could not open requested file %s, exiting.", filename.c_str());
      exit(-1);
    }
  } //else { // otherwise: do scan integration
}

OctomapLayerProjector::~OctomapLayerProjector()
{
}

///// Hopefully insertCloudCallback() is not needed here because its
///// functionality should have been moved into MoveIt somewhere.  I
///// still need to compare this function to whatever is in MoveIt to
///// make sure there are no 3d-nav-specific changes here.
#if 0
// This is called whenever a new point cloud is received.
void OctomapLayerProjector::insertCloudCallback( const sensor_msgs::PointCloud2::ConstPtr& cloud )
{
  ros::WallTime startTime = ros::WallTime::now();

  //
  // ground filtering in base frame (why not in world frame?? --hersh)
  //
  pcl::PointCloud<pcl::PointXYZ> pc; // input cloud for filtering and ground-detection
  pcl::fromROSMsg(*cloud, pc);

  tf::StampedTransform sensorToWorldTf, sensorToBaseTf, baseToWorldTf;
  try {
    // insertCloudCallback is called from a tf::MessageFilter, so this
    // "wait" call should not be necessary. -hersh
    m_tfListener.waitForTransform(m_worldFrameId, cloud->header.frame_id, cloud->header.stamp, ros::Duration(0.2));

    m_tfListener.lookupTransform(m_worldFrameId, cloud->header.frame_id, cloud->header.stamp, sensorToWorldTf);
    m_tfListener.lookupTransform(m_baseFrameId, cloud->header.frame_id, cloud->header.stamp, sensorToBaseTf);
    m_tfListener.lookupTransform(m_worldFrameId, m_baseFrameId, cloud->header.stamp, baseToWorldTf);
  } catch(tf::TransformException& ex){
    ROS_ERROR_STREAM( "Transform error: " << ex.what() << ", quitting callback");
    return;
  }
  point3d sensorOrigin = pointTfToOctomap(sensorToWorldTf.getOrigin());
  Eigen::Matrix4f sensorToBase, baseToWorld;
  pcl_ros::transformAsMatrix(sensorToBaseTf, sensorToBase);
  pcl_ros::transformAsMatrix(baseToWorldTf, baseToWorld);

  // transform pointcloud from sensor frame to fixed robot frame
  pcl::transformPointCloud(pc, pc, sensorToBase);

  // filter height and range, also removes NANs:
  pcl::PassThrough<pcl::PointXYZ> pass;
  pass.setFilterFieldName("z");
  pass.setFilterLimits(m_pointcloudMinZ, m_pointcloudMaxZ);
  pass.setInputCloud(pc.makeShared());
  pass.filter(pc);

  pcl::PointCloud<pcl::PointXYZ> pc_ground; // segmented ground plane
  pcl::PointCloud<pcl::PointXYZ> pc_nonground; // everything else
  pc_ground.header = pc.header;
  pc_nonground.header = pc.header;

  if (m_filterGroundPlane){
    if (pc.size() < 50){
      ROS_WARN("Pointcloud in OctomapLayerProjector too small, skipping ground plane extraction");
      pc_nonground = pc;
    } else {
      // plane detection for ground plane removal:
      pcl::ModelCoefficients::Ptr coefficients (new pcl::ModelCoefficients);
      pcl::PointIndices::Ptr inliers (new pcl::PointIndices);

      // Create the segmentation object and set up:
      pcl::SACSegmentation<pcl::PointXYZ> seg;
      seg.setOptimizeCoefficients (true);
      // TODO: maybe a filtering based on the surface normals might be more robust / accurate?
      seg.setModelType(pcl::SACMODEL_PERPENDICULAR_PLANE);
      seg.setMethodType(pcl::SAC_RANSAC);
      seg.setMaxIterations(200);
      seg.setDistanceThreshold (m_groundFilterDistance);
      seg.setAxis(Eigen::Vector3f(0,0,1));
      seg.setEpsAngle(m_groundFilterAngle);


      pcl::PointCloud<pcl::PointXYZ> cloud_filtered(pc);
      // Create the filtering object
      pcl::ExtractIndices<pcl::PointXYZ> extract;
      bool groundPlaneFound = false;

      while(cloud_filtered.size() > 10 && !groundPlaneFound){
        seg.setInputCloud(cloud_filtered.makeShared());
        seg.segment (*inliers, *coefficients);
        if (inliers->indices.size () == 0){
          ROS_WARN("No plane found in cloud.");

          break;
        }

        extract.setInputCloud(cloud_filtered.makeShared());
        extract.setIndices(inliers);

        if (std::abs(coefficients->values.at(3)) < m_groundFilterPlaneDistance){
          ROS_DEBUG("Ground plane found: %zu/%zu inliers. Coeff: %f %f %f %f", inliers->indices.size(), cloud_filtered.size(),
                    coefficients->values.at(0), coefficients->values.at(1), coefficients->values.at(2), coefficients->values.at(3));
          extract.setNegative (false);
          extract.filter (pc_ground);

          // remove ground points from full pointcloud:
          // workaround for PCL bug:
          if(inliers->indices.size() != cloud_filtered.size()){
            extract.setNegative(true);
            pcl::PointCloud<pcl::PointXYZ> cloud_out;
            extract.filter(cloud_out);
            pc_nonground += cloud_out;
            cloud_filtered = cloud_out;
          }

          groundPlaneFound = true;
        } else{
          ROS_DEBUG("Horizontal plane (not ground) found: %zu/%zu inliers. Coeff: %f %f %f %f", inliers->indices.size(), cloud_filtered.size(),
                    coefficients->values.at(0), coefficients->values.at(1), coefficients->values.at(2), coefficients->values.at(3));
          pcl::PointCloud<pcl::PointXYZ> cloud_out;
          extract.setNegative (false);
          extract.filter(cloud_out);
          pc_nonground +=cloud_out;
          // debug
//						pcl::PCDWriter writer;
//						writer.write<pcl::PointXYZ>("nonground_plane.pcd",cloud_out, false);

          // remove current plane from scan for next iteration:
          // workaround for PCL bug:
          if(inliers->indices.size() != cloud_filtered.size()){
            extract.setNegative(true);
            cloud_out.points.clear();
            extract.filter(cloud_out);
            cloud_filtered = cloud_out;
          } else{
            cloud_filtered.points.clear();
          }
        }

      }
      if (!groundPlaneFound){ // no plane found or remaining points too small
        ROS_WARN("No ground plane found in scan");
        //pc_nonground += cloud_filtered;

        pcl::PassThrough<pcl::PointXYZ> second_pass;
        second_pass.setFilterFieldName("z");
        second_pass.setFilterLimits(-0.10, 0.10);
        second_pass.setInputCloud(pc.makeShared());
        second_pass.filter(pc_ground);

        second_pass.setFilterLimitsNegative (true);
        second_pass.filter(pc_nonground);
      }

      // debug:
//				pcl::PCDWriter writer;
//				if (pc_ground.size() > 0)
//					writer.write<pcl::PointXYZ>("ground.pcd",pc_ground, false);
//				if (pc_nonground.size() > 0)
//					writer.write<pcl::PointXYZ>("nonground.pcd",pc_nonground, false);

    }
  } else {
    pc_nonground = pc;
  }

  // transform clouds to world frame for insertion
  pcl::transformPointCloud(pc_ground, pc_ground, baseToWorld);
  pcl::transformPointCloud(pc_nonground, pc_nonground, baseToWorld);


//		// insert without pruning and 'dirty'
//		geometry_msgs::Point sensorOrigin;
//		tf::pointTFToMsg(trans.getOrigin(), sensorOrigin);
//		m_octoMap.insertScan(transformed_cloud, sensorOrigin, m_maxRange, false, true);
//		// TODO: eval which faster: "dirty" with updateInner?
//		octree_->updateInnerOccupancy();
//		octree_->prune();
//

  // instead of direct scan insertion, compute update to filter ground:
  KeySet free_cells, occupied_cells;
  // insert ground points only as free:
  for (pcl::PointCloud<pcl::PointXYZ>::const_iterator it = pc_ground.begin(); it != pc_ground.end(); ++it){
    point3d point(it->x, it->y, it->z);
    // maxrange check
    if ((m_maxRange > 0.0) && ((point - sensorOrigin).norm() > m_maxRange) ) {
      point = sensorOrigin + (point - sensorOrigin).normalized() * m_maxRange;
    }

    // only clear space (ground points)
    if (octree_->computeRayKeys(sensorOrigin, point, m_keyRay)){
      free_cells.insert(m_keyRay.begin(), m_keyRay.end());
    }
  }

  // all other points: free on ray, occupied on endpoint:
  for (pcl::PointCloud<pcl::PointXYZ>::const_iterator it = pc_nonground.begin(); it != pc_nonground.end(); ++it){
    point3d point(it->x, it->y, it->z);
    // maxrange check
    if ((m_maxRange < 0.0) || ((point - sensorOrigin).norm() <= m_maxRange) ) {

      // free cells
      if (octree_->computeRayKeys(sensorOrigin, point, m_keyRay)){
        free_cells.insert(m_keyRay.begin(), m_keyRay.end());
      }
      // occupied endpoint
      OcTreeKey key;
      if (octree_->genKey(point, key)){
        occupied_cells.insert(key);
      }
    } else {// ray longer than maxrange:;
      point3d new_end = sensorOrigin + (point - sensorOrigin).normalized() * m_maxRange;
      if (octree_->computeRayKeys(sensorOrigin, new_end, m_keyRay)){
        free_cells.insert(m_keyRay.begin(), m_keyRay.end());
      }
    }
  }

  // mark free cells only if not seen occupied in this cloud
  for(KeySet::iterator it = free_cells.begin(), end=free_cells.end(); it!= end; ++it){
    if (occupied_cells.find(*it) == occupied_cells.end()){
      octree_->updateNode(*it, false, true);
    }
  }

  // now mark all occupied cells:
  for (KeySet::iterator it = occupied_cells.begin(), end=free_cells.end(); it!= end; it++) {
    octree_->updateNode(*it, true, true);
  }
  octree_->updateInnerOccupancy();
  octree_->prune();

  double total_elapsed = (ros::WallTime::now() - startTime).toSec();
  ROS_DEBUG("Pointcloud insertion in OctomapServer done (%d+%d pts (ground/nonground), %f sec)", int(pc_ground.size()), int(pc_nonground.size()), total_elapsed);

  publishAll(cloud->header.stamp);
}
#endif // 0

void OctomapLayerProjector::setGridSize( int grid_size_x, int grid_size_y )
{
  if( grid_size_x * grid_size_y != grid_size_x_ * grid_size_y_ )
  {
    int data_size = grid_size_x * grid_size_y;
    deleteGrids();
    
  }
}

void OctomapLayerProjector::publishAll( const ros::Time& rostime )
{
  ros::WallTime startTime = ros::WallTime::now();

  if (octree_->size() <= 1){
    ROS_WARN("Nothing to publish, octree is empty");
    return;
  }
  // configuration, right now only for testing:
  bool publishCollisionObject = true;
  bool publishMarkerArray = true;
  bool publishPointCloud = true;
  bool publish2DMap = true;

  // init collision object:
  mapping_msgs::CollisionObject collisionObject;
  collisionObject.header.frame_id = m_worldFrameId;
  collisionObject.header.stamp = rostime;
  collisionObject.id = "map";

  geometric_shapes_msgs::Shape shape;
  shape.type = geometric_shapes_msgs::Shape::BOX;
  shape.dimensions.resize(3);

  geometry_msgs::Pose pose;
  pose.orientation = tf::createQuaternionMsgFromYaw(0.0);

  // init markers:
  double minX, minY, minZ, maxX, maxY, maxZ;
  octree_->getMetricMin(minX, minY, minZ);
  octree_->getMetricMax(maxX, maxY, maxZ);

  visualization_msgs::MarkerArray occupiedNodesVis;
  // each array stores all cubes of a different size, one for each depth level:
  occupiedNodesVis.markers.resize(octree_->getTreeDepth());
  double lowestRes = octree_->getResolution();

  // init pointcloud:
  pcl::PointCloud<pcl::PointXYZ> pclCloud;

  // init projected 2D map:
  nav_msgs::OccupancyGrid map;
  nav_msgs::OccupancyGrid base_map;
  nav_msgs::OccupancyGrid spine_map;
  nav_msgs::OccupancyGrid arm_map;
  nav_msgs::OccupancyGrid temp_arm_map;
  map.info.resolution = octree_->getResolution();
  map.header.frame_id = m_worldFrameId;
  map.header.stamp = rostime;
  base_map.info.resolution = octree_->getResolution();
  base_map.header.frame_id = m_worldFrameId;
  base_map.header.stamp = rostime;
  spine_map.info.resolution = octree_->getResolution();
  spine_map.header.frame_id = m_worldFrameId;
  spine_map.header.stamp = rostime;
  arm_map.info.resolution = octree_->getResolution();
  arm_map.header.frame_id = m_worldFrameId;
  arm_map.header.stamp = rostime;
  temp_arm_map.info.resolution = octree_->getResolution();
  temp_arm_map.header.frame_id = m_worldFrameId;
  temp_arm_map.header.stamp = rostime;

  octomap::point3d minPt(minX, minY, minZ);
  octomap::point3d maxPt(maxX, maxY, maxZ);
  octomap::OcTreeKey minKey, maxKey, curKey;
  if (!octree_->genKey(minPt, minKey)){
    ROS_ERROR("Could not create min OcTree key at %f %f %f", minPt.x(), minPt.y(), minPt.z());
    return;
  }
  if (!octree_->genKey(maxPt, maxKey)){
    ROS_ERROR("Could not create max OcTree key at %f %f %f", maxPt.x(), maxPt.y(), maxPt.z());
    return;
  }

  ROS_DEBUG("MinKey: %d %d %d / MaxKey: %d %d %d", minKey[0], minKey[1], minKey[2], maxKey[0], maxKey[1], maxKey[2]);

  // add padding if requested (= new min/maxPts in x&y):
  double halfPaddedX = 0.5*m_minSizeX;
  double halfPaddedY = 0.5*m_minSizeY;
  minX = std::min(minX, -halfPaddedX);
  maxX = std::max(maxX, halfPaddedX);
  minY = std::min(minY, -halfPaddedY);
  maxY = std::max(maxY, halfPaddedY);
  minPt = octomap::point3d(minX, minY, minZ);
  maxPt = octomap::point3d(maxX, maxY, maxZ);

  octomap::OcTreeKey paddedMinKey, paddedMaxKey;
  if (!octree_->genKey(minPt, paddedMinKey)){
    ROS_ERROR("Could not create padded min OcTree key at %f %f %f", minPt.x(), minPt.y(), minPt.z());
    return;
  }
  if (!octree_->genKey(maxPt, paddedMaxKey)){
    ROS_ERROR("Could not create padded max OcTree key at %f %f %f", maxPt.x(), maxPt.y(), maxPt.z());
    return;
  }

  ROS_DEBUG("Padded MinKey: %d %d %d / padded MaxKey: %d %d %d", paddedMinKey[0], paddedMinKey[1], paddedMinKey[2], paddedMaxKey[0], paddedMaxKey[1], paddedMaxKey[2]);
  assert(paddedMaxKey[0] >= maxKey[0] && paddedMaxKey[1] >= maxKey[1]);

  map.info.width = paddedMaxKey[0] - paddedMinKey[0] +1;
  map.info.height = paddedMaxKey[1] - paddedMinKey[1] +1;
  base_map.info.width = paddedMaxKey[0] - paddedMinKey[0] +1;
  base_map.info.height = paddedMaxKey[1] - paddedMinKey[1] +1;
  spine_map.info.width = paddedMaxKey[0] - paddedMinKey[0] +1;
  spine_map.info.height = paddedMaxKey[1] - paddedMinKey[1] +1;
  arm_map.info.width = paddedMaxKey[0] - paddedMinKey[0] +1;
  arm_map.info.height = paddedMaxKey[1] - paddedMinKey[1] +1;
  temp_arm_map.info.width = paddedMaxKey[0] - paddedMinKey[0] +1;
  temp_arm_map.info.height = paddedMaxKey[1] - paddedMinKey[1] +1;
  int mapOriginX = minKey[0] - paddedMinKey[0];
  int mapOriginY = minKey[1] - paddedMinKey[1];
  assert(mapOriginX >= 0 && mapOriginY >= 0);

  // might not exactly be min / max of octree:
  octomap::point3d origin;
  octree_->genCoords(paddedMinKey, octree_->getTreeDepth(), origin);
  map.info.origin.position.x = origin.x() - octree_->getResolution()*0.5;
  map.info.origin.position.y = origin.y() - octree_->getResolution()*0.5;
  base_map.info.origin.position.x = origin.x() - octree_->getResolution()*0.5;
  base_map.info.origin.position.y = origin.y() - octree_->getResolution()*0.5;
  base_map.info.origin.position.z = 0.0;
  spine_map.info.origin.position.x = origin.x() - octree_->getResolution()*0.5;
  spine_map.info.origin.position.y = origin.y() - octree_->getResolution()*0.5;
  spine_map.info.origin.position.z = 0.45;
  arm_map.info.origin.position.x = origin.x() - octree_->getResolution()*0.5;
  arm_map.info.origin.position.y = origin.y() - octree_->getResolution()*0.5;
  temp_arm_map.info.origin.position.x = origin.x() - octree_->getResolution()*0.5;
  temp_arm_map.info.origin.position.y = origin.y() - octree_->getResolution()*0.5;

  // Allocate space to hold the data (init to unknown)
  map.data.resize(base_map.info.width * base_map.info.height, -1);
  base_map.data.resize(base_map.info.width * base_map.info.height, -1);
  spine_map.data.resize(spine_map.info.width * spine_map.info.height, -1);
  arm_map.data.resize(arm_map.info.width * arm_map.info.height, 0);
  temp_arm_map.data.resize(arm_map.info.width * arm_map.info.height, 0);

  geometry_msgs::PointStamped vin;
  vin.point.x = 0;
  vin.point.y = 0;
  vin.point.z = 0;
  vin.header.stamp = ros::Time(0);
  double link_padding = 0.03;

  double minArmHeight = 2.0;
  double maxArmHeight = 0.0;
  if(haveAttachedObject){
    printf("adjust for attached object\n");
    vin.header.frame_id = attachedFrame;
    geometry_msgs::PointStamped vout;
    m_tfListener.transformPoint("base_footprint",vin,vout);
    //ROS_ERROR("link %s with height %f\n",arm_links[i],vout.point.z);
    maxArmHeight = vout.point.z + (attachedMaxOffset + link_padding);
    minArmHeight = vout.point.z - (attachedMinOffset + link_padding);
  }

  char* arm_links[10] = {"l_elbow_flex_link","l_gripper_l_finger_tip_link",
                         "l_gripper_r_finger_tip_link","l_upper_arm_roll_link","l_wrist_flex_link",
                         "r_elbow_flex_link","r_gripper_l_finger_tip_link","r_gripper_r_finger_tip_link",
                         "r_upper_arm_roll_link","r_wrist_flex_link"};
  double offsets[10] = {0.10, 0.03, 0.03, 0.16, 0.05, 0.10, 0.03, 0.03, 0.16, 0.05};

  for(int i=0; i<10; i++){
    vin.header.frame_id = arm_links[i];
    geometry_msgs::PointStamped vout;
    m_tfListener.transformPoint("base_footprint",vin,vout);
    //ROS_ERROR("link %s with height %f\n",arm_links[i],vout.point.z);
    double upper = vout.point.z + (offsets[i] + link_padding);
    double lower = vout.point.z - (offsets[i] + link_padding);
    if(upper > maxArmHeight)
      maxArmHeight = upper;
    if(lower <  minArmHeight)
      minArmHeight = lower;
  }
  arm_map.info.origin.position.z = (maxArmHeight+minArmHeight)/2.0;

  visualization_msgs::Marker m;
  m.header.frame_id = "base_footprint";
  m.header.stamp = ros::Time();
  m.ns = "bound_the_box";
  m.id = 0;
  m.type = visualization_msgs::Marker::CUBE;
  m.action = visualization_msgs::Marker::ADD;
  m.pose.position.x = 0.5;
  m.pose.position.y = 0.0;
  m.pose.position.z = (maxArmHeight+minArmHeight)/2.0;
  m.pose.orientation.x = 0;
  m.pose.orientation.y = 0;
  m.pose.orientation.z = 0;
  m.pose.orientation.w = 1;
  m.scale.x = 1.0;
  m.scale.y = 0.4;
  m.scale.z = maxArmHeight-minArmHeight;
  m.color.a = 1;
  m.color.r = 0;
  m.color.g = 0;
  m.color.b = 1;
  boundPub.publish(m);

    
  for( OcTreeROS::OcTreeType::iterator it = octree_->begin(), end = octree_->end(); it != end; ++it )
  {
    double z = it.getZ();
    if (octree_->isNodeOccupied(*it)){
      if (z > m_occupancyMinZ && z < m_occupancyMaxZ)
      {
        octomap::OcTreeKey nKey = it.getKey();
        octomap::OcTreeKey key;
        // Ignore speckles in the map:
        // TODO: only at lowest res!
        if (m_filterSpeckles){
          bool neighborFound = false;
          for (key[2] = nKey[2] - 1; !neighborFound && key[2] <= nKey[2] + 1; ++key[2]){
            for (key[1] = nKey[1] - 1; !neighborFound && key[1] <= nKey[1] + 1; ++key[1]){
              for (key[0] = nKey[0] - 1; !neighborFound && key[0] <= nKey[0] + 1; ++key[0]){
                if (key != nKey){
                  OcTreeNode* node = octree_->search(key);
                  if (node && octree_->isNodeOccupied(node)){
                    // we have a neighbor => break!
                    neighborFound = true;
                  }
                }
              }
            }
          }
          // done with search, see if found and skip otherwise:
          if (!neighborFound){
            ROS_DEBUG("Ignoring single speckle at (%f,%f,%f)", it.getX(), it.getY(), it.getZ());
            continue;
          }
        } // else: current octree node is no speckle, send it out

        double size = it.getSize();
        double x = it.getX();
        double y = it.getY();

        // create collision object:
        if (publishCollisionObject){
          shape.dimensions[0] = shape.dimensions[1] = shape.dimensions[2] = size;
          collisionObject.shapes.push_back(shape);
          pose.position.x = x;
          pose.position.y = y;
          pose.position.z = z;
          collisionObject.poses.push_back(pose);
        }

        //create marker:
        if (publishMarkerArray){
          int idx = int(log2(it.getSize() / lowestRes) +0.5);
          assert (idx >= 0 && unsigned(idx) < occupiedNodesVis.markers.size());
          geometry_msgs::Point cubeCenter;
          cubeCenter.x = x;
          cubeCenter.y = y;
          cubeCenter.z = z;

          occupiedNodesVis.markers[idx].points.push_back(cubeCenter);
          if (m_useHeightMap){
            double h = (1.0 - std::min(std::max((cubeCenter.z-minZ)/ (maxZ - minZ), 0.0), 1.0)) *m_colorFactor;
            occupiedNodesVis.markers[idx].colors.push_back(heightMapColor(h));
          }
        }

        // insert into pointcloud:
        if (publishPointCloud)
          pclCloud.push_back(pcl::PointXYZ(x, y, z));

        // update 2D map (occupied always overrides):
        if (publish2DMap)
        {
          if (it.getDepth() == octree_->getTreeDepth())
          {
            int i = nKey[0] - paddedMinKey[0];
            int j = nKey[1] - paddedMinKey[1];
            map.data[map.info.width*j + i] = 100;
            if(base_map.data[base_map.info.width*j + i] == -1)
              base_map.data[base_map.info.width*j + i] = 0;
            if(spine_map.data[spine_map.info.width*j + i] == -1)
              spine_map.data[spine_map.info.width*j + i] = 0;
            //if(arm_map.data[arm_map.info.width*j + i] == -1)
            //arm_map.data[arm_map.info.width*j + i] = 0;
            if(z>=0 && z<0.3)
              base_map.data[base_map.info.width*j + i] = 101;
            if(z>=0.25 && z<1.4)
              spine_map.data[spine_map.info.width*j + i] = 101;
            if(z>=minArmHeight && z<=maxArmHeight)
            {
              //arm_map.data[arm_map.info.width*j + i] = 100;
              arm_map.data[arm_map.info.width*j + i]++;
              temp_arm_map.data[arm_map.info.width*j + i]++;
            }
          }
          else
          {
            int intSize = 1 << (octree_->getTreeDepth() - it.getDepth());
            octomap::OcTreeKey minKey=it.getIndexKey();
            for(int dx=0; dx < intSize; dx++){
              int i = minKey[0]+dx - paddedMinKey[0];
              for(int dy=0; dy < intSize; dy++){
                int j = minKey[1]+dy - paddedMinKey[1];
                map.data[map.info.width*j + i] = 100;
                if(base_map.data[base_map.info.width*j + i] == -1)
                  base_map.data[base_map.info.width*j + i] = 0;
                if(spine_map.data[spine_map.info.width*j + i] == -1)
                  spine_map.data[spine_map.info.width*j + i] = 0;
                //if(arm_map.data[arm_map.info.width*j + i] == -1)
                //arm_map.data[arm_map.info.width*j + i] = 0;
                if(z>=0 && z<0.3)
                  base_map.data[base_map.info.width*j + i] = 101;
                if(z>=0.25 && z<1.4)
                  spine_map.data[spine_map.info.width*j + i] = 101;
                if(z>=minArmHeight && z<=maxArmHeight){
                  //arm_map.data[arm_map.info.width*j + i] = 100;
                  arm_map.data[arm_map.info.width*j + i]++;
                  temp_arm_map.data[arm_map.info.width*j + i]++;
                }
              }
            }
          }
        }
      }
    } else{ // node not occupied => mark as free in 2D map if unknown so far
      if (publish2DMap){
        if (it.getDepth() == octree_->getTreeDepth()){
          octomap::OcTreeKey nKey = it.getKey();
          int i = nKey[0] - paddedMinKey[0];
          int j = nKey[1] - paddedMinKey[1];
          if(map.data[map.info.width*j + i] == -1)
            map.data[map.info.width*j + i] = 0;
          if(base_map.data[base_map.info.width*j + i] == -1)
            base_map.data[base_map.info.width*j + i] = 0;
          if(spine_map.data[spine_map.info.width*j + i] == -1)
            spine_map.data[spine_map.info.width*j + i] = 0;
          //if(arm_map.data[arm_map.info.width*j + i] == -1)
          //arm_map.data[arm_map.info.width*j + i] = 0;
          if(z>=minArmHeight && z<=maxArmHeight)
            temp_arm_map.data[arm_map.info.width*j + i]++;
        } else{
          int intSize = 1 << (octree_->getTreeDepth() - it.getDepth());
          octomap::OcTreeKey minKey=it.getIndexKey();
          for(int dx=0; dx < intSize; dx++){
            int i = minKey[0]+dx - paddedMinKey[0];
            for(int dy=0; dy < intSize; dy++){
              int j = minKey[1]+dy - paddedMinKey[1];
              if(map.data[map.info.width*j + i] == -1)
                map.data[map.info.width*j + i] = 0;
              if(base_map.data[base_map.info.width*j + i] == -1)
                base_map.data[base_map.info.width*j + i] = 0;
              if(spine_map.data[spine_map.info.width*j + i] == -1)
                spine_map.data[spine_map.info.width*j + i] = 0;
              //if(arm_map.data[arm_map.info.width*j + i] == -1)
              //arm_map.data[arm_map.info.width*j + i] = 0;
              if(z>=minArmHeight && z<=maxArmHeight)
                temp_arm_map.data[arm_map.info.width*j + i]++;
            }
          }
        }
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////
  // This finalizes the tall or regular obstacle cell data in the
  // arm_map.  A "tall" obstacle is one were more than 80% of the
  // cells in the column are obstacle.
  //
  // It also makes a list of regular obstacle cells to be used in the
  // next step.
  std::vector<int> shortObsCells;
  for(unsigned int i=0; i<arm_map.data.size(); i++){
    if(temp_arm_map.data[i] == 0){
      if(map.data[i] == -1)
        arm_map.data[i] = -1;
    }
    else if(arm_map.data[i] == 0)
      arm_map.data[i] = 0;
    else if(double(arm_map.data[i])/temp_arm_map.data[i] > 0.8)
      arm_map.data[i] = 101;
    else{
      arm_map.data[i] = 100;
      shortObsCells.push_back(i);
    }
  }

  ////////////////////////////////////////////////////////////////////////
  // This section TALL-ifies all regular obstacle cells adjacent to
  // tall obstacle cells.
  std::vector<int> tallObsCells;
  tallObsCells.reserve(shortObsCells.size());
  int dxy[8] = { -arm_map.info.width-1, -arm_map.info.width, -arm_map.info.width+1,
                 -1,                    /* center */         1,
                 arm_map.info.width-1,  arm_map.info.width,  arm_map.info.width+1 };
  for(unsigned int i=0; i<shortObsCells.size(); i++){
    for(int j=0; j<8; j++){
      int temp = shortObsCells[i]+dxy[j];
      if(temp<0 || temp>=arm_map.data.size())
        continue;
      if(arm_map.data[temp]==101){
        tallObsCells.push_back(shortObsCells[i]);
        break;
      }
    }
  }
  for(unsigned int i=0; i<tallObsCells.size(); i++)
  {
    arm_map.data[tallObsCells[i]] = 101;
  }
  ////////////////////////////////////////////////////////////////////////

  //ROS_ERROR("short %d tall %d\n",shortObsCells.size(),tallObsCells.size());

  // finish MarkerArray:
  if (publishMarkerArray){
    for (unsigned i= 0; i < occupiedNodesVis.markers.size(); ++i){
      double size = lowestRes * pow(2,i);

      occupiedNodesVis.markers[i].header.frame_id = m_worldFrameId;
      occupiedNodesVis.markers[i].header.stamp = rostime;
      occupiedNodesVis.markers[i].ns = "map";
      occupiedNodesVis.markers[i].id = i;
      occupiedNodesVis.markers[i].type = visualization_msgs::Marker::CUBE_LIST;
      occupiedNodesVis.markers[i].scale.x = size;
      occupiedNodesVis.markers[i].scale.y = size;
      occupiedNodesVis.markers[i].scale.z = size;
      occupiedNodesVis.markers[i].color = m_color;


      if (occupiedNodesVis.markers[i].points.size() > 0)
        occupiedNodesVis.markers[i].action = visualization_msgs::Marker::ADD;
      else
        occupiedNodesVis.markers[i].action = visualization_msgs::Marker::DELETE;
    }


    m_markerPub.publish(occupiedNodesVis);
  }

  // finish pointcloud:
  if (publishPointCloud){
    sensor_msgs::PointCloud2 cloud;
    pcl::toROSMsg (pclCloud, cloud);
    cloud.header.frame_id = m_worldFrameId;
    cloud.header.stamp = rostime;
    m_pointCloudPub.publish(cloud);
  }

  if (publishCollisionObject)
    m_collisionObjectPub.publish(collisionObject);

  if (publish2DMap){
    m_mapPub.publish(map);
    base_mapPub.publish(base_map);
    spine_mapPub.publish(spine_map);
    arm_mapPub.publish(arm_map);
  }

  double total_elapsed = (ros::WallTime::now() - startTime).toSec();
  ROS_DEBUG("Map publishing in OctomapServer took %f sec", total_elapsed);

}


bool OctomapLayerProjector::serviceCallback(octomap_ros::GetOctomap::Request  &req,
                                            octomap_ros::GetOctomap::Response &res)
{
  ROS_INFO("Sending map data on service request");
  res.map.header.frame_id = m_worldFrameId;
  res.map.header.stamp = ros::Time::now();
  octomap::octomapMapToMsgData(m_octoMap.octree, res.map.data);

  return true;
}

bool OctomapLayerProjector::clearBBXSrv(octomap_ros::ClearBBXRegionRequest& req, octomap_ros::ClearBBXRegionRequest& resp){
  OcTreeROS::OcTreeType::leaf_bbx_iterator it, end;
  point3d min = pointMsgToOctomap(req.min);
  point3d max = pointMsgToOctomap(req.max);

  for(OcTreeROS::OcTreeType::leaf_bbx_iterator it = octree_->begin_leafs_bbx(min,max),
        end=octree_->end_leafs_bbx(); it!= end; ++it){
    it->setLogOdds(-2);
//			octree_->updateNode(it.getKey(), -6.0f);
  }
  octree_->updateInnerOccupancy();

  publishAll(ros::Time::now());

  return true;
}

void OctomapLayerProjector::publishMap(const ros::Time& rostime){

  octomap_ros::OctomapBinary map;
  map.header.frame_id = m_worldFrameId;
  map.header.stamp = rostime;

  octomap::octomapMapToMsgData(m_octoMap.octree, map.data);

  m_binaryMapPub.publish(map);
}

std_msgs::ColorRGBA OctomapLayerProjector::heightMapColor(double h) const {

  std_msgs::ColorRGBA color;
  color.a = 1.0;
  // blend over HSV-values (more colors)

  double s = 1.0;
  double v = 1.0;

  h -= floor(h);
  h *= 6;
  int i;
  double m, n, f;

  i = floor(h);
  f = h - i;
  if (!(i & 1))
    f = 1 - f; // if i is even
  m = v * (1 - s);
  n = v * (1 - s * f);

  switch (i) {
  case 6:
  case 0:
    color.r = v; color.g = n; color.b = m;
    break;
  case 1:
    color.r = n; color.g = v; color.b = m;
    break;
  case 2:
    color.r = m; color.g = v; color.b = n;
    break;
  case 3:
    color.r = m; color.g = n; color.b = v;
    break;
  case 4:
    color.r = n; color.g = m; color.b = v;
    break;
  case 5:
    color.r = v; color.g = m; color.b = n;
    break;
  default:
    color.r = 1; color.g = 0.5; color.b = 0.5;
    break;
  }

  return color;
}

} // end namespace octomap




