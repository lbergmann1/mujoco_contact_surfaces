/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2022, Bielefeld University
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
 *   * Neither the name of Bielefeld University nor the names of its
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

/* Authors: Florian Patzelt*/

#include <mujoco_contact_surface_sensors/tactile_sensor_base.h>

namespace mujoco_contact_surface_sensors {

bool TactileSensorBase::load(mjModelPtr m, mjDataPtr d)
{
	if (rosparam_config_.getType() == XmlRpc::XmlRpcValue::TypeStruct && rosparam_config_.hasMember("geomName") &&
	    rosparam_config_.hasMember("topicName") && rosparam_config_.hasMember("updateRate") &&
	    rosparam_config_.hasMember("sensorName")) {
		geomName = static_cast<std::string>(rosparam_config_["geomName"]);

		int id = mj_name2id(m.get(), mjOBJ_GEOM, geomName.c_str());
		if (id >= 0) {
			geomID       = id;
			lastUpdate   = ros::Time();
			topicName    = static_cast<std::string>(rosparam_config_["topicName"]);
			publisher    = node_handle_->advertise<tactile_msgs::TactileState>(topicName, 1, true);
			sensorName   = static_cast<std::string>(rosparam_config_["sensorName"]);
			updateRate   = static_cast<double>(rosparam_config_["updateRate"]);
			updatePeriod = ros::Duration(1.0 / updateRate);
			if (rosparam_config_.hasMember("visualize")) {
				visualize = static_cast<bool>(rosparam_config_["visualize"]);
			}
			return true;
		}
	}
	return false;
}

void TactileSensorBase::update(const mjModel *m, mjData *d, const std::vector<GeomCollisionPtr> &geomCollisions)
{
	auto now = ros::Time::now();
	if (now < lastUpdate) { // reset lastUpdate after jump back in time
		lastUpdate = ros::Time();
	}
	// if enough time has passed do a sensor update
	if (now - lastUpdate >= updatePeriod) {
		lastUpdate = now;
		n_vGeom    = 0;
		int id     = geomID;
		internal_update(m, d, geomCollisions);

		// publish sensor data
		if (publisher.getNumSubscribers() > 0) {
			tactile_state_msg_.header.stamp = now;
			publisher.publish(tactile_state_msg_);
		}
	}
}

void TactileSensorBase::renderCallback(mjModelPtr model, mjDataPtr data, mjvScene *scene)
{
	if (visualize) {
		ROS_WARN_STREAM_COND_NAMED(
		    scene->maxgeom < n_vGeom, "mujoco_contact_surface_sensors",
		    "Not all vgeoms could be visualized: n_vGeom = " << n_vGeom << " scene->maxgeom = " << scene->maxgeom);
		for (int i = 0; i < n_vGeom && scene->ngeom < scene->maxgeom; ++i) {
			scene->geoms[scene->ngeom++] = vGeoms[i];
		}
	}
}

void TactileSensorBase::reset() {}

bool TactileSensorBase::initVGeom(int type, const mjtNum size[3], const mjtNum pos[3], const mjtNum mat[9],
                                  const float rgba[4])
{
	if (n_vGeom < mujoco_contact_surfaces::MAX_VGEOM) {
		mjvGeom *g = vGeoms + n_vGeom++;
		mjv_initGeom(g, type, size, pos, mat, rgba);
		return true;
	}
	return false;
}
} // namespace mujoco_contact_surface_sensors