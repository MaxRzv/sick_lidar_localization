/*
 * @brief sim_loc_driver implements a ros driver for sick localization.
 * sim_loc_driver establishes a tcp connection to the localization controller
 * (f.e. SIM1000FXA), receives and converts result port telegrams and
 * publishes all sim location data by ros messages of type SickLocResultPortTelegramMsg.
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
#include <ros/ros.h>
#include <string>
#include <vector>

#include "sick_lidar_localization/sim_loc_driver_monitor.h"

int main(int argc, char** argv)
{
  // Ros configuration and initialization
  ros::init(argc, argv, "sim_loc_driver");
  ros::NodeHandle nh;
  ROS_INFO_STREAM("sim_loc_driver started.");
  
  // Create a worker thread, which
  // - connects to the localization controller (f.e. SIM1000FXA),
  // - receives binary result port telegrams,
  // - converts them to SickLocResultPortTelegramMsg
  // - publishes the sim location data
  std::string server_adress("192.168.0.1"), server_default_adress("192.168.0.1");  // IP adress of the localization controller, default: "192.168.0.1"
  int tcp_port = 2201;                     // Default: The localization controller uses IP port number 2201 to send localization results
  ros::param::param<std::string>("/sim_loc_driver/localization_controller_ip_adress" , server_adress, server_adress);
  ros::param::param<std::string>("/sick_lidar_localization/driver/localization_controller_default_ip_adress", server_default_adress, server_default_adress);
  ros::param::param<int>("/sick_lidar_localization/driver/result_telegrams_tcp_port", tcp_port, tcp_port);
  server_adress = (server_adress.empty()) ? server_default_adress : server_adress;
  
  // Initialize driver threads to connect to localization controller and to monitor driver messages
  sick_lidar_localization::DriverMonitor driver_monitor(&nh, server_adress, tcp_port);
  
  // Subscribe to sim_loc_driver messages
  std::string result_telegrams_topic = "/sick_lidar_localization/driver/result_telegrams";      // default topic to publish result port telegram messages (type SickLocResultPortTelegramMsg)
  ros::param::param<std::string>("/sick_lidar_localization/driver/result_telegrams_topic", result_telegrams_topic, result_telegrams_topic);
  ros::Subscriber result_telegram_subscriber = nh.subscribe(result_telegrams_topic, 1, &sick_lidar_localization::DriverMonitor::messageCbResultPortTelegrams, &driver_monitor);
  
  // Start driver threads to connect to localization controller and to monitor driver messages
  if(!driver_monitor.start())
  {
    ROS_ERROR_STREAM("## ERROR sim_loc_driver: could not start driver monitor thread, exiting");
    return EXIT_FAILURE;
  }
  
  // Run ros event loop
  ros::spin();
  
  // Cleanup and exit
  std::cout << "sim_loc_driver finished." << std::endl;
  ROS_INFO_STREAM("sim_loc_driver finished.");
  driver_monitor.stop();
  std::cout << "sim_loc_driver exits." << std::endl;
  ROS_INFO_STREAM("sim_loc_driver exits.");
  return EXIT_SUCCESS;
}
