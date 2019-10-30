/*
 * @brief sim_loc_pointcloud_converts sim_loc_driver messages (type sick_lidar_localization::SickLocResultPortTelegramMsg),
 * to PointCloud2 messages and publishes PointCloud2 messages on topic "/cloud".
 *
 * It also serves as an usage example for sick_lidar_localization and shows how to use sick_lidar_localization
 * in a custumized application.
 *
 * Copyright (C) 2019 Ing.-Buero Dr. Michael Lehning, Hildesheim
 * Copyright (C) 2019 SICK AG, Waldkirch
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
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
 *     * Neither the name of SICK AG nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission
 *     * Neither the name of Ing.-Buero Dr. Michael Lehning nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission
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
 *
 *      Authors:
 *         Michael Lehning <michael.lehning@lehning.de>
 *
 *  Copyright 2019 SICK AG
 *  Copyright 2019 Ing.-Buero Dr. Michael Lehning
 *
 */
#include <math.h>

#include "sick_lidar_localization/sim_loc_utils.h"
#include "sick_lidar_localization/sim_loc_pointcloud_converter.h"

/*!
 * Constructor
 * @param[in] nh ros node handle
 */
sick_lidar_localization::PointCloudConverter::PointCloudConverter(ros::NodeHandle* nh)
: m_point_cloud_frame_id("sick_lidar_localization"), m_converter_thread_running(false), m_converter_thread(0)
{
  if(nh)
  {
    std::string point_cloud_topic = "/cloud"; // default topic to publish result port telegram messages (type SickLocResultPortTelegramMsg)
    ros::param::param<std::string>("/sick_lidar_localization/driver/point_cloud_topic", point_cloud_topic, point_cloud_topic);
    ros::param::param<std::string>("/sick_lidar_localization/driver/point_cloud_frame_id", m_point_cloud_frame_id, m_point_cloud_frame_id);
    m_point_cloud_publisher = nh->advertise<sensor_msgs::PointCloud2>(point_cloud_topic, 1);
  }
}

/*!
 * Destructor
 */
sick_lidar_localization::PointCloudConverter::~PointCloudConverter()
{
  stop();
}

/*!
 * Starts the converter thread, converts telegrams and publishes PointCloud2 messages.
 * @return true on success, false on failure.
 */
bool sick_lidar_localization::PointCloudConverter::start(void)
{
  m_converter_thread_running = true;
  m_converter_thread = new boost::thread(&sick_lidar_localization::PointCloudConverter::runPointCloudConverterThreadCb, this);
  return true;
}

/*!
 * Stops the converter thread.
 * @return true on success, false on failure.
 */
bool sick_lidar_localization::PointCloudConverter::stop(void)
{
  m_converter_thread_running = false;
  if(m_converter_thread)
  {
    m_converter_thread->join();
    delete(m_converter_thread);
    m_converter_thread = 0;
  }
  return true;
}

/*!
 * Callback for result telegram messages (SickLocResultPortTelegramMsg) from sim_loc_driver.
 * The received message is buffered by fifo m_result_port_telegram_fifo.
 * Message handling and evaluation is done in the converter thread.
 * @param[in] msg result telegram message (SickLocResultPortTelegramMsg)
 */
void sick_lidar_localization::PointCloudConverter::messageCbResultPortTelegrams(const sick_lidar_localization::SickLocResultPortTelegramMsg & msg)
{
  m_result_port_telegram_fifo.push(msg);
}

/*!
 * Creates and returns a list of 4 points from position and orientaion of a sensor:
 * the center point (PoseX,PoseY) and 3 corner points of a triangle showing the orientation (PoseYaw)
 * Note: This is just an example to show the position and orientation of a sensor by a list of points.
 * @param[in] posx x-position of sensor in meter
 * @param[in] posy y-position of sensor in meter
 * @param[in] yaw orientation (yaw) of sensor in radians
 * @param[in] triangle_height heigt of the virtual triangle in meter
 * @return list of 3D points
 */
std::vector<geometry_msgs::Point> sick_lidar_localization::PointCloudConverter::poseToDemoPoints(double posx, double posy, double yaw, double triangle_height)
{
  // Center point at (posx, posy)
  std::vector<geometry_msgs::Point> points;
  geometry_msgs::Point centerpoint;
  centerpoint.x = posx;
  centerpoint.y = posy;
  centerpoint.z = 0;
  points.push_back(centerpoint);
  geometry_msgs::Point cornerpoint = centerpoint;
  // Add triangle corner points at (posx + dx, posy + dy)
  cornerpoint.x = centerpoint.x + triangle_height * std::cos(yaw);
  cornerpoint.y = centerpoint.y + triangle_height * std::sin(yaw);
  points.push_back(cornerpoint);
  cornerpoint.x = centerpoint.x + 0.5 * triangle_height * std::cos(yaw + M_PI_2);
  cornerpoint.y = centerpoint.y + 0.5 * triangle_height * std::sin(yaw + M_PI_2);
  points.push_back(cornerpoint);
  cornerpoint.x = centerpoint.x + 0.5 * triangle_height * std::cos(yaw - M_PI_2);
  cornerpoint.y = centerpoint.y + 0.5 * triangle_height * std::sin(yaw - M_PI_2);
  points.push_back(cornerpoint);
  return points;
}

/*!
 * Converts a telegram from type SickLocResultPortTelegramMsg to PointCloud2
 * @param[in] msg result telegram message (SickLocResultPortTelegramMsg)
 * @return PointCloud2 message
 */
sensor_msgs::PointCloud2 sick_lidar_localization::PointCloudConverter::convert(const sick_lidar_localization::SickLocResultPortTelegramMsg & msg)
{
  sensor_msgs::PointCloud2 pointcloud_msg;
  // Create center point (PoseX,PoseY) and 3 corner points of a triangle showing the orientation (PoseYaw)
  // Note: This is just an example to demonstrate use of sick_lidar_localization and PointCloud2 messages!
  std::vector<geometry_msgs::Point> points = poseToDemoPoints(
    1.0e-3 * msg.telegram_payload.PoseX, // x-position in meter
    1.0e-3 * msg.telegram_payload.PoseY, // y-position in meter
    1.0e-3 * msg.telegram_payload.PoseYaw * M_PI / 180.0, // yaw angle in radians
    0.6); // triangle_height in meter (demo only)
  // set pointcloud header
  pointcloud_msg.header.stamp = msg.header.stamp; // timestamp of telegram
  pointcloud_msg.header.frame_id = m_point_cloud_frame_id;
  pointcloud_msg.header.seq = 0;
  // clear cloud data
  pointcloud_msg.height = 0;
  pointcloud_msg.width = 0;
  pointcloud_msg.data.clear();
  // set pointcloud field properties
  int numChannels = 3; // "x", "y", "z"
  std::string channelId[] = { "x", "y", "z" };
  pointcloud_msg.height = 1;
  pointcloud_msg.width = points.size(); // normally we have 4 points (center point and 3 corner points)
  pointcloud_msg.is_bigendian = false;
  pointcloud_msg.is_dense = true;
  pointcloud_msg.point_step = numChannels * sizeof(float);
  pointcloud_msg.row_step = pointcloud_msg.point_step * pointcloud_msg.width;
  pointcloud_msg.fields.resize(numChannels);
  for (int i = 0; i < numChannels; i++)
  {
    pointcloud_msg.fields[i].name = channelId[i];
    pointcloud_msg.fields[i].offset = i * sizeof(float);
    pointcloud_msg.fields[i].count = 1;
    pointcloud_msg.fields[i].datatype = sensor_msgs::PointField::FLOAT32;
  }
  // set pointcloud data values
  pointcloud_msg.data.resize(pointcloud_msg.row_step * pointcloud_msg.height);
  float* pfdata = reinterpret_cast<float*>(&pointcloud_msg.data[0]);
  for(size_t data_cnt = 0, point_cnt = 0; point_cnt < points.size(); point_cnt++)
  {
    pfdata[data_cnt++] = static_cast<float>(points[point_cnt].x);
    pfdata[data_cnt++] = static_cast<float>(points[point_cnt].y);
    pfdata[data_cnt++] = static_cast<float>(points[point_cnt].z);
  }
  return pointcloud_msg;
}

/*!
 * Thread callback, pops received telegrams from the fifo buffer m_result_port_telegram_fifo,
 * converts the telegrams from type SickLocResultPortTelegramMsg to PointCloud2 and publishes
 * PointCloud2 messages.
 */
void sick_lidar_localization::PointCloudConverter::runPointCloudConverterThreadCb(void)
{
  ROS_INFO_STREAM("PointCloudConverter: converter thread for sim_loc_driver messages started");
  while(ros::ok() && m_converter_thread_running)
  {
    // Wait for next telegram
    while(ros::ok() && m_converter_thread_running && m_result_port_telegram_fifo.empty())
    {
      ros::Duration(0.0001).sleep();
      m_result_port_telegram_fifo.waitForElement();
    }
    sick_lidar_localization::SickLocResultPortTelegramMsg telegram = m_result_port_telegram_fifo.pop();
    // Convert telegram to PointCloud2
    sensor_msgs::PointCloud2 pointcloud_msg = convert(telegram);
    // Publish PointCloud2 data
    m_point_cloud_publisher.publish(pointcloud_msg);
  }
  ROS_INFO_STREAM("PointCloudConverter: converter thread for sim_loc_driver messages finished");
}
