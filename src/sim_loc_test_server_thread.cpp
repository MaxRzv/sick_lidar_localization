/*
 * @brief sim_loc_test_server_thread implements a simple tcp server thread,
 * simulating a localization controller for unittests. It runs a thread to listen
 * and accept tcp connections from clients and generates telegrams to test
 * the ros driver for sim localization.
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

#include "sick_lidar_localization/sim_loc_test_server_thread.h"
#include "sick_lidar_localization/sim_loc_testcase_generator.h"
#include "sick_lidar_localization/sim_loc_random.h"
#include "sick_lidar_localization/sim_loc_utils.h"

/*
 * Constructor. The server thread does not start automatically, call start() and stop() to start and stop the server.
 * @param[in] nh ros node handle
 * @param[in] tcp_port tcp server port, default: The localization controller uses IP port number 2201 to send localization results
 */
sick_lidar_localization::TestServerThread::TestServerThread(ros::NodeHandle* nh, int tcp_port) :
  m_tcp_port(tcp_port), m_tcp_connection_thread(0), m_tcp_connection_thread_running(false), m_worker_thread_running(false),
  m_ioservice(), m_tcp_acceptor(m_ioservice, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), m_tcp_port)),
  m_result_telegram_rate(10), m_demo_move_in_circles(false), m_error_simulation_enabled(false), m_error_simulation_flag(NO_ERROR),
  m_error_simulation_thread(0), m_error_simulation_thread_running(false)
{
  m_tcp_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
  if(nh)
  {
    std::string result_testcases_topic = "/sick_lidar_localization/test_server/result_testcases"; // default topic to publish testcases with result port telegrams (type SickLocResultPortTestcaseMsg)
    ros::param::param<double>("/sick_lidar_localization/test_server/result_telegrams_rate", m_result_telegram_rate, m_result_telegram_rate);
    ros::param::param<std::string>("/sick_lidar_localization/test_server/result_testcases_topic", result_testcases_topic, result_testcases_topic);
    ros::param::param<std::string>("/sick_lidar_localization/test_server/result_testcases_frame_id", m_result_testcases_frame_id, "result_testcases");
    ros::param::param<bool>("/sim_loc_test_server/demo_circles", m_demo_move_in_circles, m_demo_move_in_circles);
    ros::param::param<bool>("/sim_loc_test_server/error_simulation", m_error_simulation_enabled, m_error_simulation_enabled);
    // ros publisher for testcases with result port telegrams (type SickLocResultPortTestcaseMsg)
    m_result_testcases_publisher = nh->advertise<sick_lidar_localization::SickLocResultPortTestcaseMsg>(result_testcases_topic, 1);
  }
}

/*
 * Destructor. Stops the server thread and closes all tcp connections.
 * @param[in] tcp_port tcp server port, default: The localization controller uses IP port number 2201 to send localization results
 */
sick_lidar_localization::TestServerThread::~TestServerThread()
{
  stop();
}

/*
 * Starts the server thread, starts to listen and accept tcp connections from clients.
 * @return true on success, false on failure.
 */
bool sick_lidar_localization::TestServerThread::start(void)
{
  if(m_error_simulation_enabled)
  {
    ROS_WARN_STREAM("TestServerThread: running in error simulation mode.");
    ROS_WARN_STREAM("Test server will intentionally react incorrect and will send false data and produce communication errors.");
    ROS_WARN_STREAM("Running test server in error simulation mode is for test purposes only. Do not expect typical results.");
    m_error_simulation_thread_running = true;
    m_error_simulation_thread =  new boost::thread(&sick_lidar_localization::TestServerThread::runErrorSimulationThreadCb, this);
  }
  else
  {
    ROS_INFO_STREAM("TestServerThread: running in normal mode, no error simulation.");
    
  }
  m_tcp_connection_thread_running = true;
  m_tcp_connection_thread = new boost::thread(&sick_lidar_localization::TestServerThread::runConnectionThreadCb, this);
  return true;
}

/*
 * Stops the server thread and closes all tcp connections.
 * @return true on success, false on failure.
 */
bool sick_lidar_localization::TestServerThread::stop(void)
{
  m_tcp_connection_thread_running = false;
  m_error_simulation_thread_running = false;
  m_worker_thread_running = false;
  if(m_error_simulation_thread)
  {
    m_error_simulation_thread->join();
    delete(m_error_simulation_thread);
    m_error_simulation_thread = 0;
  }
  m_ioservice.stop();
  if(m_tcp_acceptor.is_open())
  {
    m_tcp_acceptor.cancel(); // cancel the blocking call m_tcp_acceptor.listen() in the connection thread
    m_tcp_acceptor.close();
  }
  if(m_tcp_connection_thread)
  {
    m_tcp_connection_thread->join();
    delete(m_tcp_connection_thread);
    m_tcp_connection_thread = 0;
  }
  closeTcpConnections();
  closeWorkerThreads();
  return true;
}

/*
 * Closes all tcp connections
 */
void sick_lidar_localization::TestServerThread::closeTcpConnections(void)
{
  for(std::list<boost::asio::ip::tcp::socket*>::iterator socket_iter = m_tcp_sockets.begin(); socket_iter != m_tcp_sockets.end(); socket_iter++)
  {
    boost::asio::ip::tcp::socket* p_socket = *socket_iter;
    if(p_socket && p_socket->is_open())
    {
      p_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
      p_socket->close();
    }
    *socket_iter = 0;
  }
  m_tcp_sockets.clear();
}

/*
 * Stops all worker threads
 */
void sick_lidar_localization::TestServerThread::closeWorkerThreads(void)
{
  boost::lock_guard<boost::mutex> worker_thread_lockguard(m_tcp_worker_threads_mutex);
  m_worker_thread_running = false;
  for(std::list<boost::thread*>::iterator thread_iter = m_tcp_worker_threads.begin(); thread_iter != m_tcp_worker_threads.end(); thread_iter++)
  {
    boost::thread *p_thread = *thread_iter;
    p_thread->join();
    delete(p_thread);
  }
  m_tcp_worker_threads.clear();
}

/*
 * Callback for result telegram messages (SickLocResultPortTelegramMsg) from sim_loc_driver.
 * Buffers the last telegram to monitor sim_loc_driver messages in error simulation mode.
 * @param[in] msg result telegram message (SickLocResultPortTelegramMsg)
 */
void sick_lidar_localization::TestServerThread::messageCbResultPortTelegrams(const sick_lidar_localization::SickLocResultPortTelegramMsg & msg)
{
  m_last_telegram_received.set(msg);
}

/*
 * Thread callback, listens and accept tcp connections from clients.
 * Starts a new worker thread to generate result port telegrams for each tcp client.
 */
void sick_lidar_localization::TestServerThread::runConnectionThreadCb(void)
{
  ROS_INFO_STREAM("TestServerThread: connection thread started");
  while(ros::ok() && m_tcp_connection_thread_running && m_tcp_acceptor.is_open())
  {
    if(m_error_simulation_flag.get() == DONT_LISTEN) // error simulation: testserver does not open listening port
    {
      ros::Duration(0.1).sleep();
      continue;
    }
    // normal mode: listen to tcp port, accept and connect to new tcp clients
    boost::asio::ip::tcp::socket* tcp_client_socket = new boost::asio::ip::tcp::socket(m_ioservice);
    boost::system::error_code errorcode;
    if(m_error_simulation_flag.get() != DONT_ACCECPT)
      ROS_INFO_STREAM("TestServerThread: listening to tcp connections on port " << m_tcp_port);
    m_tcp_acceptor.listen();
    if(m_error_simulation_flag.get() == DONT_ACCECPT) // error simulation: testserver does not does not accecpt tcp clients
    {
      ros::Duration(0.1).sleep();
      continue;
    }
    m_tcp_acceptor.accept(*tcp_client_socket, errorcode); // normal mode: accept new tcp client
    if (!errorcode && tcp_client_socket->is_open())
    {
      m_tcp_sockets.push_back(tcp_client_socket);
      // tcp client connected, start worker thread
      ROS_INFO_STREAM("TestServerThread: established new tcp client connection");
      boost::lock_guard<boost::mutex> worker_thread_lockguard(m_tcp_worker_threads_mutex);
      m_worker_thread_running = true;
      boost::thread* tcp_worker_thread = new boost::thread(&sick_lidar_localization::TestServerThread::runWorkerThreadCb, this, tcp_client_socket);
      m_tcp_worker_threads.push_back(tcp_worker_thread);
    }
  }
  closeTcpConnections();
  m_tcp_connection_thread_running = false;
  ROS_INFO_STREAM("TestServerThread: connection thread finished");
}

/*
 * Worker thread callback, generates and sends telegrams to a tcp client.
 * There's one worker thread for each tcp client.
 * @param[in] p_socket socket to send telegrams to the tcp client
 */
void sick_lidar_localization::TestServerThread::runWorkerThreadCb(boost::asio::ip::tcp::socket* p_socket)
{
  ROS_INFO_STREAM("TestServerThread: worker thread started");
  ros::Duration send_telegrams_delay(1.0 / m_result_telegram_rate);
  sick_lidar_localization::SickLocResultPortTestcaseMsg testcase = sick_lidar_localization::TestcaseGenerator::createDefaultResultPortTestcase(); // initial testcase is the default testcase
  sick_lidar_localization::UniformRandomInteger random_generator(0,255);
  sick_lidar_localization::UniformRandomInteger random_length(1, 512);
  sick_lidar_localization::UniformRandomInteger random_integer(0, INT_MAX);
  double circle_yaw = 0;
  while(ros::ok() && m_worker_thread_running && p_socket && p_socket->is_open())
  {
    boost::system::error_code error_code;
    send_telegrams_delay.sleep();
    if (m_error_simulation_flag.get() == DONT_SEND) // error simulation: testserver does not send any telegrams
      continue;
    if (m_error_simulation_flag.get() == SEND_RANDOM_TCP) // error simulation: testserver sends invalid random tcp packets
    {
      std::vector<uint8_t> random_data = random_generator.generate(random_length.generate()); // binary random data of random size
      boost::asio::write(*p_socket, boost::asio::buffer(random_data.data(), random_data.size()), boost::asio::transfer_exactly(random_data.size()), error_code);
      ROS_DEBUG_STREAM("TestServerThread: send random data " << sick_lidar_localization::Utils::toHexString(random_data));
    }
    else
    {
      if(m_demo_move_in_circles) // simulate a sensor moving in circles
      {
        testcase = sick_lidar_localization::TestcaseGenerator::createResultPortCircles(2.0, circle_yaw);
        circle_yaw = sick_lidar_localization::Utils::normalizeAngle(circle_yaw + 1.0 * M_PI / 180);
      }
      if (m_error_simulation_flag.get() == SEND_INVALID_TELEGRAMS) // error simulation: testserver sends invalid telegrams (invalid data, false checksums, etc.)
      {
        int number_random_bytes = ((random_integer.generate()) % (testcase.binary_data.size()));
        for(int cnt_random_bytes = 0; cnt_random_bytes < number_random_bytes; cnt_random_bytes++)
        {
          int byte_cnt = ((random_integer.generate()) % (testcase.binary_data.size()));
          testcase.binary_data[byte_cnt] = (uint8_t)(random_generator.generate() & 0xFF);
        }
        ROS_DEBUG_STREAM("TestServerThread: send random binary telegram " << sick_lidar_localization::Utils::toHexString(testcase.binary_data));
      }
      // send binary result port telegram to tcp client
      size_t bytes_written = boost::asio::write(*p_socket, boost::asio::buffer(testcase.binary_data.data(), testcase.binary_data.size()), boost::asio::transfer_exactly(testcase.binary_data.size()), error_code);
      if (error_code || bytes_written != testcase.binary_data.size())
      {
        std::stringstream error_info;
        error_info << "## ERROR TestServerThread: failed to send binary result port telegram, " << bytes_written << " of " << testcase.binary_data.size() << " bytes send, error code: " << error_code.message();
        ROS_LOG_STREAM((m_error_simulation_flag.get() == NO_ERROR) ? (::ros::console::levels::Warn) : (::ros::console::levels::Debug), ROSCONSOLE_DEFAULT_NAME, error_info.str());
      }
      else
      {
        ROS_DEBUG_STREAM("TestServerThread: send binary result port telegram " << sick_lidar_localization::Utils::toHexString(testcase.binary_data));
      }
      // publish testcases (SickLocResultPortTestcaseMsg, i.e. binary telegram and SickLocResultPortTelegramMsg messages) for test and verification of sim_loc_driver
      testcase.header.stamp = ros::Time::now();
      testcase.header.frame_id = m_result_testcases_frame_id;
      m_result_testcases_publisher.publish(testcase);
    }
    // next testcase is a result port telegram with random data
    testcase = sick_lidar_localization::TestcaseGenerator::createRandomResultPortTestcase();
  }
  ROS_INFO_STREAM("TestServerThread: worker thread finished");
}

/*
 * Waits for a given time in seconds, as long as ros::ok() and m_error_simulation_thread_running == true.
 * @param[in] seconds delay in seconds
 */
void sick_lidar_localization::TestServerThread::errorSimulationWait(double seconds)
{
  ros::Time starttime = ros::Time::now();
  while(ros::ok() && m_error_simulation_thread_running && (ros::Time::now() - starttime).toSec() < seconds)
  {
    ros::Duration(0.001).sleep();
  }
}

/*
 * Waits for and returns the next telegram message from sick_lidar_localization driver.
 * @param[in] timeout_seconds wait timeout in seconds
 * @param[out] telegram_msg last telegram message received
 * @return true, if a new telegram message received, false otherwise (timeout or shutdown)
 */
bool sick_lidar_localization::TestServerThread::errorSimulationWaitForTelegramReceived(double timeout_seconds, sick_lidar_localization::SickLocResultPortTelegramMsg & telegram_msg)
{
  ros::Time starttime = ros::Time::now();
  while(ros::ok() && m_error_simulation_thread_running)
  {
    telegram_msg = m_last_telegram_received.get();
    if(telegram_msg.header.stamp > starttime)
      return true; // new telegram received
    if((ros::Time::now() - starttime).toSec() >= timeout_seconds)
      return false; // timeout
    ros::Duration(0.001).sleep();
  }
  return false;
}

/*
 * Thread callback, runs an error simulation and switches m_error_simulation_flag through the error test cases.
 */
void sick_lidar_localization::TestServerThread::runErrorSimulationThreadCb(void)
{
  sick_lidar_localization::SickLocResultPortTelegramMsg telegram_msg;
  size_t number_testcases = 0, number_testcases_failed = 0;
  ROS_INFO_STREAM("TestServerThread: error simulation thread started");
  
  // Error simulation: start normal execution
  number_testcases++;
  m_error_simulation_flag.set(NO_ERROR);
  ROS_INFO_STREAM("TestServerThread: 1. error simulation testcase: normal execution, expecting telegram messages from driver");
  errorSimulationWait(10);
  if(!errorSimulationWaitForTelegramReceived(10, telegram_msg))
  {
    number_testcases_failed++;
    ROS_WARN_STREAM("## ERROR TestServerThread: 1. error simulation testcase: no ros telegram message received, expected SickLocResultPortTelegramMsg from driver");
  }
  else
    ROS_INFO_STREAM("TestServerThread: 1. error simulation testcase: received telegram message \"" << sick_lidar_localization::Utils::flattenToString(telegram_msg) << "\", okay");
  
  // Error simulation testcase: testserver does not open listening port for 10 seconds, normal execution afterwards.
  number_testcases++;
  m_error_simulation_flag.set(DONT_LISTEN);
  ROS_INFO_STREAM("TestServerThread: 2. error simulation testcase: server not responding, not listening, no tcp connections accepted.");
  closeWorkerThreads();
  closeTcpConnections();
  errorSimulationWait(10);
  m_error_simulation_flag.set(NO_ERROR);
  ROS_INFO_STREAM("TestServerThread: 2. error simulation testcase: switched to normal execution, expecting telegram messages from driver");
  errorSimulationWait(10);
  if(!errorSimulationWaitForTelegramReceived(10, telegram_msg))
  {
    number_testcases_failed++;
    ROS_WARN_STREAM("## ERROR TestServerThread: 2. error simulation testcase: no ros telegram message received, expected SickLocResultPortTelegramMsg from driver");
  }
  else
    ROS_INFO_STREAM("TestServerThread: 2. error simulation testcase: received telegram message \"" << sick_lidar_localization::Utils::flattenToString(telegram_msg) << "\", okay");
  
  // Error simulation testcase: testserver does not accecpt tcp clients for 10 seconds, normal execution afterwards.
  number_testcases++;
  m_error_simulation_flag.set(DONT_ACCECPT);
  ROS_INFO_STREAM("TestServerThread: 3. error simulation testcase: server not responding, listening on port " << m_tcp_port << ", but accepting no tcp clients");
  closeWorkerThreads();
  closeTcpConnections();
  errorSimulationWait(10);
  m_error_simulation_flag.set(NO_ERROR);
  errorSimulationWait(10);
  if(!errorSimulationWaitForTelegramReceived(10, telegram_msg))
  {
    number_testcases_failed++;
    ROS_WARN_STREAM("## ERROR TestServerThread: 3. error simulation testcase: no ros telegram message received, expected SickLocResultPortTelegramMsg from driver");
  }
  else
    ROS_INFO_STREAM("TestServerThread: 3. error simulation testcase: received telegram message \"" << sick_lidar_localization::Utils::flattenToString(telegram_msg) << "\", okay");
  
  // Error simulation testcase: testserver does not send telegrams for 10 seconds, normal execution afterwards.
  number_testcases++;
  m_error_simulation_flag.set(DONT_SEND);
  ROS_INFO_STREAM("TestServerThread: 4. error simulation testcase: server not sending telegrams");
  errorSimulationWait(10);
  m_error_simulation_flag.set(NO_ERROR);
  closeWorkerThreads();
  errorSimulationWait(10);
  if(!errorSimulationWaitForTelegramReceived(10, telegram_msg))
  {
    number_testcases_failed++;
    ROS_WARN_STREAM("## ERROR TestServerThread: 4. error simulation testcase: no ros telegram message received, expected SickLocResultPortTelegramMsg from driver");
  }
  else
    ROS_INFO_STREAM("TestServerThread: 4. error simulation testcase: received telegram message \"" << sick_lidar_localization::Utils::flattenToString(telegram_msg) << "\", okay");
  
  // Error simulation testcase: testserver sends random tcp data for 10 seconds, normal execution afterwards.
  number_testcases++;
  m_error_simulation_flag.set(SEND_RANDOM_TCP);
  ROS_INFO_STREAM("TestServerThread: 5. error simulation testcase: server sending random tcp data");
  errorSimulationWait(10);
  m_error_simulation_flag.set(NO_ERROR);
  closeWorkerThreads();
  errorSimulationWait(10);
  if(!errorSimulationWaitForTelegramReceived(10, telegram_msg))
  {
    number_testcases_failed++;
    ROS_WARN_STREAM("## ERROR TestServerThread: 5. error simulation testcase: no ros telegram message received, expected SickLocResultPortTelegramMsg from driver");
  }
  else
    ROS_INFO_STREAM("TestServerThread: 5. error simulation testcase: received telegram message \"" << sick_lidar_localization::Utils::flattenToString(telegram_msg) << "\", okay");
  
  // Error simulation testcase: testserver sends invalid telegrams (invalid data, false checksums, etc.) for 10 seconds, normal execution afterwards.
  number_testcases++;
  m_error_simulation_flag.set(SEND_INVALID_TELEGRAMS);
  ROS_INFO_STREAM("TestServerThread: 6. error simulation testcase: server sending invalid telegrams");
  errorSimulationWait(10);
  m_error_simulation_flag.set(NO_ERROR);
  closeWorkerThreads();
  errorSimulationWait(10);
  if(!errorSimulationWaitForTelegramReceived(10, telegram_msg))
  {
    number_testcases_failed++;
    ROS_WARN_STREAM("## ERROR TestServerThread: 6. error simulation testcase: no ros telegram message received, expected SickLocResultPortTelegramMsg from driver");
  }
  else
    ROS_INFO_STREAM("TestServerThread: 6. error simulation testcase: received telegram message \"" << sick_lidar_localization::Utils::flattenToString(telegram_msg) << "\", okay");
  
  // End of error simulation, print summary and exit
  m_error_simulation_thread_running = false;
  ROS_INFO_STREAM("TestServerThread: error simulation thread finished");
  ROS_INFO_STREAM("TestServerThread: error simulation summary: " << (number_testcases - number_testcases_failed) << " of " << number_testcases<< " testcases passed, " << number_testcases_failed << " failures.");
  ROS_INFO_STREAM("TestServerThread: exit with ros::shutdown()");
  ros::shutdown();
}
