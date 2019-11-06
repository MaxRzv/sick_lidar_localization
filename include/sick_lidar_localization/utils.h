/*
 * @brief sim_loc_utils contains a collection of utility functions for SIM Localization.
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
#ifndef __SIM_LOC_UTILS_H_INCLUDED
#define __SIM_LOC_UTILS_H_INCLUDED

#include <math.h>
#include <string>
#include <sstream>
#include <vector>
#include <boost/thread.hpp>

#include "sick_lidar_localization/SickLocColaTelegramSrv.h"
#include "sick_lidar_localization/SickLocResultPortHeaderMsg.h"
#include "sick_lidar_localization/SickLocResultPortPayloadMsg.h"
#include "sick_lidar_localization/SickLocResultPortCrcMsg.h"
#include "sick_lidar_localization/SickLocResultPortTelegramMsg.h"
#include "sick_lidar_localization/SickLocResultPortTestcaseMsg.h"
#include "sick_lidar_localization/SickLocRequestTimestampSrv.h"

namespace sick_lidar_localization
{
  /*!
   * class SetGet implements setter and getter template functions for thread-protected set and get of a value.
   */
  template<typename ElementType, typename MutexType = boost::mutex> class SetGet
  {
  public:
  
    /*! Constructor with optional initialization of the value */
    SetGet(const ElementType & value = ElementType()) : m_value(value) {}
  
    /*! Sets the value threadsafe */
    void set(const ElementType & value)
    {
      boost::lock_guard<MutexType> value_lockguard(m_value_mutex);
      m_value = value;
    }
  
    /*! Returns the value threadsafe */
    ElementType get(void)
    {
      boost::lock_guard<MutexType> value_lockguard(m_value_mutex);
      return m_value;
    }

  protected:
  
    /** member variables */
    ElementType m_value; ///< protected value
    MutexType m_value_mutex; ///< mutex to protect value
  };
  
  /*!
   * class Utils implements utility functions for SIM Localization.
   */
  class Utils
  {
  public:

    /*!
     * Converts and returns binary data to hex string
     * @param[in] binary_data binary input data
     * @return hex string
     */
    static std::string toHexString(const std::vector<uint8_t> & binary_data);
  
    /*!
     * Shortcut to replace linefeeds by colon-separators
     */
    static void flattenString(std::string & s);
    
    /*!
     * Shortcut to print a type in flatten format, replacing linefeeds by colon-separators
     */
    template <typename T> static std::string flattenToString(const T & x)
    {
      std::stringstream s;
      s << x;
      std::string out(s.str());
      flattenString(out);
      return out;
    }
  
    /*!
     * Compares two objects by streaming them to strings.
     * Returns true, if string representation of x and y is identical,
     * otherwise false.
     */
    template <typename T> static bool identicalByStream(const T & x, const T & y)
    {
      std::stringstream sx, sy;
      sx << x;
      sy << y;
      return sx.str() == sy.str();
    }
  
    /*!
     * Compares two SickLocResultPortTelegramMsgs by streaming them to strings.
     * Returns true, if string representation of x and y is identical,
     * otherwise false. The message headers with timestamp and frame_id are ignored.
     */
    static bool identicalByStream(const SickLocResultPortTelegramMsg & x, const SickLocResultPortTelegramMsg & y)
    {
      return identicalByStream(x.telegram_header, y.telegram_header)
      && identicalByStream(x.telegram_payload, y.telegram_payload)
      && identicalByStream(x.telegram_trailer, y.telegram_trailer);
    }
  
    /*!
     * Returns the normalized angle, i.e. the angle in the range -PI to +PI.
     * @param[in] angle angle in radians
     * @return normalized angle in radians
     */
    static double normalizeAngle(double angle)
    {
      while(angle > M_PI)
        angle -= (2.0 * M_PI);
      while(angle < -M_PI)
        angle += (2.0 * M_PI);
      return angle;
    }
  
  }; // class Utils
  
} // namespace sick_lidar_localization
#endif // __SIM_LOC_UTILS_H_INCLUDED