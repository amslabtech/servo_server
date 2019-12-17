//
//	CCV Remote Controller
//
#include <iostream>
#include <time.h>
#include <sys/time.h>
#include <mosquitto.hpp>		// c++ wrapper of mosquitto
#include "ccv_servo_structure.hpp"	// my data structure
#include "imu_structure.hpp"	// my data structure
#include <math.h>
#include <mutex>

using namespace std;
using namespace servo;

CcvServoStructure servo_data;
std::mutex  mtx_pose;
volatile bool pose_update = false;


//
// MQTT section
//

class ServoSubscriber : public Mosquitto {
  protected:
//	ServoStructure data;
	const char* topic;
	void onConnected();
	void onError(const char* _msg) { std::cout << _msg; }
	void onMessage(std::string _topic, void* _data, int _len);
	struct timeval ts;

  public:
	ServoSubscriber(const char* _topic):topic(_topic){}
};

void ServoSubscriber::onConnected()
{
	std::cout << "ServoSubscriber is Connected.\n";
	subscribe(topic);
}

#include <strings.h>
void ServoSubscriber::onMessage(std::string _topic, void* _data, int _len)
{
//	gettimeofday(&ts,NULL);
	bcopy(_data, (char*)&servo_data, sizeof(servo_data));		
//	int32_t diff = (ts.tv_sec-data.ts.tv_sec)*1000000 + ts.tv_usec-data.ts.tv_usec;
//	std::cout << std::setw(5) << diff << " usec,";
	servo_data.print_read();
}


namespace robot {
	float pose[4];
};


//
// MQTT IMU Subscriber
//

class ImuSubscriber : public Mosquitto {
  protected:
	ImuStructure data;
	const char* topic;
	void onConnected();
	void onError(const char* _msg) { std::cout << _msg; }
	void onMessage(std::string _topic, void* _data, int _len);
	struct timeval ts;

  public:
	ImuSubscriber(const char* _topic):topic(_topic){}
};

void ImuSubscriber::onConnected()
{
	std::cout << "ServoSubscriber is Connected.\n";
	subscribe(topic);
}

#include <strings.h>
void ImuSubscriber::onMessage(std::string _topic, void* _data, int _len)
{
//	gettimeofday(&ts,NULL);
	mtx_pose.lock();
	bcopy(_data, (char*)&data, sizeof(data));		
	robot::pose[0] = data.fusion[0];
	robot::pose[1] = data.fusion[1];
	robot::pose[2] = data.fusion[2];
	pose_update = true;
	mtx_pose.unlock();
}



int main(int argc, char* argv[])
{
	char* ip_addr = (char*)"192.168.0.172";
	if(argc>1) {
		ip_addr = argv[1];
	}
	std::cout << "connecting " << ip_addr << " ...\n";

	//
	// MQTT Subscriber section
	//
	ImuSubscriber imu_data_catcher(imu::topic);
	imu_data_catcher.set_username_password("imu_subscriber",servo::password);
	imu_data_catcher.connect(ip_addr);
	imu_data_catcher.loop_start();


	//
	// MQTT Subscriber section
	//
	const char* name_catcher = "servo_data_catcher";
	ServoSubscriber servo_data_catcher(topic_read);
	servo_data_catcher.set_username_password(name_catcher,servo::password);
	servo_data_catcher.connect(ip_addr);


	//
	// MQTT Publisher section
	//
	const char* name_commander = "servo_servo_commander";
    Mosquitto servo_commander;
    servo_commander.set_username_password(name_commander,servo::password);
    servo_commander.connect(ip_addr);
    servo_commander.subscribe(servo::topic_write);


	//
	// Start event loops
	//
	servo_data_catcher.loop_start();

	for(int i=0; ; i++) {
		if(pose_update==true) {
			mtx_pose.lock();
			servo_data.command_position[servo::ROLL ] = -robot::pose[0]/4;
			servo_data.command_position[servo::FORE ] = -robot::pose[1]/4;
			servo_data.command_position[servo::REAR ] =  robot::pose[1]/4;
			servo_data.command_position[servo::STEER] = 
									12.0F*M_PI/180*sin(i/M_PI/200);
			pose_update = false;
			mtx_pose.unlock();

			servo_commander.publish(servo::topic_write,
				&servo_data.command_position,sizeof(servo_data.command_position));
		}
		usleep(5*1000);
	}

	//
	// Termination
	//
	servo_data_catcher.cleanup_library();
//	servo_commander.cleanup_library();
}


