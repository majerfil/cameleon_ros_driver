#include <ros/ros.h>
#include <tf/tf.h>
#include <geometry_msgs/Twist.h>
#include <nav_msgs/Odometry.h>
#include "CMessageClient.h"
#include <string.h>
#include <boost/thread/thread.hpp>

ros::Subscriber commandSub;
ros::Publisher odometryPub;

//robot Parameters
int robotPort;
std::string robotIP;
double maxForwardSpeed,maxBackwardSpeed,maxTurnSpeed;
double forwardSpeedGain,turnSpeedGain;
CMessage message;
CMessageClient client;
CStatusMessage status;
nav_msgs::Odometry odometry;

bool lastOdoValid = false;
double px = 0;
double py = 0;
double heading = 0;
double lastOdoLeft = 0;
double lastOdoRight = 0;

void updateOdometry(CStatusMessage status)
{
	double odoLeft = 0.001 * (double) status.odoLeft;
	double odoRight = 0.001 * (double) status.odoRight;
	if (lastOdoValid) {
		double wheelBase = 0.40;
		double dLeft = odoLeft - lastOdoLeft;
		double dRight = odoRight - lastOdoRight;
		double dForward = 0.5 * ( dLeft + dRight );
		double dHeading = (dRight - dLeft) / wheelBase;

		heading += dHeading;
		px += dForward * cos(heading);
		py += dForward * sin(heading);

		odometry.pose.pose.position.x = px;
		odometry.pose.pose.position.y = py;
		tf::Quaternion orientation;
	        orientation.setRPY((float)status.roll*M_PI/180,(float)status.pitch*M_PI/180,heading);
		odometry.pose.pose.orientation.x = orientation[0];
		odometry.pose.pose.orientation.y = orientation[1];
		odometry.pose.pose.orientation.z = orientation[2];
		odometry.pose.pose.orientation.w = orientation[3];
		odometryPub.publish(odometry); 
	}
	lastOdoLeft = odoLeft;
	lastOdoRight = odoRight;
	lastOdoValid = true;
}

void readData()
{
	CStatusMessage status;
	while (ros::ok())
	{
		if (client.checkForHeader() == 0) {
			if (client.checkForStatus(status) == 0){
				updateOdometry(status);
			}
		}
	}
}

void commandCallback(const geometry_msgs::Twist::ConstPtr& msg)
{
	message.type = MSG_SPEED;
	message.forward = (int)(msg->linear.x*1000);
	message.turn = (int)(msg->angular.z*1000);
	message.flipper = (int)(msg->angular.y*1000);

	if (message.forward > +maxForwardSpeed*1000) message.forward = maxForwardSpeed*1000;
	if (message.forward < -maxBackwardSpeed*1000) message.forward = -maxBackwardSpeed*1000;
	if (message.turn    > +maxTurnSpeed*1000) message.turn = maxTurnSpeed*1000;
	if (message.turn    < -maxTurnSpeed*1000) message.turn = -maxTurnSpeed*1000;
	client.sendMessage(message);
}
     
int main(int argc, char** argv)
{
	ros::init(argc, argv, "cameleon_ros_driver");
	ros::NodeHandle n("~");

	//initialize robot model parameters
	n.param("cameleon_port",robotPort,50004);
	n.param("cameleon_ip",robotIP,std::string("172.43.50.193"));
	n.param("cameleon_maxForwardSpeed",maxForwardSpeed,0.5);
	n.param("cameleon_maxBackwardSpeed",maxBackwardSpeed,0.5);
	n.param("cameleon_maxTurnSpeed",maxTurnSpeed,0.5);
	printf("%f",maxTurnSpeed);
	bool requier[2] = {true, true};
	client.init(robotIP.c_str(), robotPort, requier);

	boost::thread thread_b(readData);

	//create the robot
	odometryPub = n.advertise<nav_msgs::Odometry>("/odom", 1);
	commandSub = n.subscribe("/cmd_vel", 1, commandCallback);

	ros::spin();
}
