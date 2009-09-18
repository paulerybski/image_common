/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2009, Willow Garage, Inc.
*  All rights reserved.
* 
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
* 
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

#include "image_transport/image_publisher.h"
#include "image_transport/publisher_plugin.h"
#include <pluginlib/class_loader.h>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/erase.hpp>

namespace image_transport {

struct Publisher::Impl
{
  Impl()
    : loader("image_transport", "image_transport::PublisherPlugin")
  {
  }

  ~Impl()
  {
    shutdown();
  }
  
  void shutdown()
  {
    BOOST_FOREACH(PublisherPlugin& pub, publishers)
      pub.shutdown();
  }
  
  std::string topic;
  pluginlib::ClassLoader<PublisherPlugin> loader;
  boost::ptr_vector<PublisherPlugin> publishers;
  TransportTopicMap topic_map;
};

Publisher::Publisher()
  : impl_(new Impl)
{
  // Default behavior: load all plugins and use default topic names.
  BOOST_FOREACH(std::string lookup_name, impl_->loader.getDeclaredClasses()) {
    boost::erase_last(lookup_name, "_pub");
    impl_->topic_map[lookup_name] = "";
  }
}

Publisher::Publisher(const Publisher& rhs)
  : impl_(rhs.impl_)
{
}

Publisher::~Publisher()
{
}

void Publisher::advertise(ros::NodeHandle& nh, const std::string& topic, uint32_t queue_size,
                          const ros::SubscriberStatusCallback& connect_cb,
                          const ros::SubscriberStatusCallback& disconnect_cb,
                          const ros::VoidPtr& tracked_object, bool latch)
{
  impl_->topic = nh.resolveName(topic);
  
  BOOST_FOREACH(const TransportTopicMap::value_type& value, impl_->topic_map) {
    std::string lookup_name = value.first + "_pub";
    //ROS_INFO("Loading %s", lookup_name.c_str());
    try {
      PublisherPlugin* pub = impl_->loader.createClassInstance(lookup_name);
      impl_->publishers.push_back(pub);
      std::string sub_topic = value.second;
      if (sub_topic.empty())
        sub_topic = pub->getDefaultTopic(impl_->topic);
      nh.setParam(sub_topic + "/transport_type", pub->getTransportName());
      pub->advertise(nh, sub_topic, queue_size, connect_cb, disconnect_cb, tracked_object, latch);
    }
    catch (const std::runtime_error& e) {
      ROS_WARN("Failed to load plugin %s, error string: %s",
               lookup_name.c_str(), e.what());
    }
  }
}

void Publisher::advertise(ros::NodeHandle& nh, const std::string& topic,
                          uint32_t queue_size, bool latch)
{
  advertise(nh, topic, queue_size, ros::SubscriberStatusCallback(), ros::SubscriberStatusCallback(),
            ros::VoidPtr(), latch);
}

uint32_t Publisher::getNumSubscribers() const
{
  uint32_t count = 0;
  BOOST_FOREACH(const PublisherPlugin& pub, impl_->publishers)
    count += pub.getNumSubscribers();
  return count;
}

std::string Publisher::getTopic() const
{
  return impl_->topic;
}

Publisher::TransportTopicMap& Publisher::getTopicMap()
{
  return impl_->topic_map;
}

const Publisher::TransportTopicMap& Publisher::getTopicMap() const
{
  return impl_->topic_map;
}

void Publisher::publish(const sensor_msgs::Image& message) const
{
  BOOST_FOREACH(const PublisherPlugin& pub, impl_->publishers) {
    if (pub.getNumSubscribers() > 0)
      pub.publish(message);
  }
}

void Publisher::publish(const sensor_msgs::ImageConstPtr& message) const
{
  publish(*message);
}

void Publisher::shutdown()
{
  impl_->shutdown();
}

} //namespace image_transport
