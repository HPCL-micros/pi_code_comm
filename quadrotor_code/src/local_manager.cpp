#define TF_EULER_DEFAULT_ZYX

#include "ros/ros.h"
#include "tf/tf.h"
#include <tf/transform_broadcaster.h>
#include "nav_msgs/Odometry.h"
#include "decide_softbus_msgs/NavigationPoint.h"
#include "geometry_msgs/PointStamped.h"
#include <decide_softbus_msgs/SetControlling.h>
#include "bebop_msgs/Ardrone3PilotingStateAttitudeChanged.h"
#include "bebop_msgs/Ardrone3PilotingStateSpeedChanged.h"
#include "quadrotor_code/Status.h"
#include "quadrotor_code/Neighbor.h"
#include "std_msgs/Time.h"
#include "std_msgs/String.h"
#include <geodesy/utm.h>
#include "sensor_msgs/NavSatFix.h"
#include <tf/LinearMath/Quaternion.h>
#include "geometry_msgs/PoseArray.h"
#include "std_msgs/Int32.h"
#include <string>
#include <list>
#include <vector>
#include <iostream>
#include <utility>
#include <cmath>
#include <ctime>
#include <fstream>
#include <math.h>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/string.hpp> 
#include <boost/serialization/vector.hpp>
using namespace std;
#define R 7
#define sendT 1
#define perror 0  //value
#define verror 0 //rate
double PI=acos(-1);
double stopdist = 1.5;

int line_uav_num=4;  //每一行摆放的飞机的数量
double delta_dis=3; //无人机之间的间距
int stand_vx=0, stand_vy=0;//作为标准的无人机的虚拟坐标
double stand_x, stand_y;//作为标准的无人机的实际坐标

/*ofstream vel_out("/home/czx/vel.txt");
bool odom_first=true;
std_msgs::Time odom_start_time;*/
struct NeighborInfo{
    int _r_id;
    float _px;
    float _py;
    float _vx;
    float _vy;
    
    template<class Archive>
    void serialize(Archive &ar,const unsigned int version)
    {
        ar & _r_id;
        ar & _px;
        ar & _py;
        ar & _vx;
        ar & _vy;
    }
    
    NeighborInfo(int r_id,float px,float py,float vx,float vy )
    {
        _r_id=r_id;
        _px=px;
        _py=py;
        _vx=vx;
        _vy=vy;
    }
    
    NeighborInfo()
    {
        _r_id=-1;
        _px=0;
        _py=0;
        _vx=0;
        _vy=0;
    }
};
class OdomHandle
{
    public:
    ros::Subscriber sub_yaw,sub_vel,sub_fix, sub_odom;
    ros::Publisher pub;
    ros::Publisher neighbor_pub;
    ros::Publisher stop_pub;
    ros::Publisher fix_odom_pub;
    double odomx,odomy,odomz,odomtheta;
    double _px,_py,_vx,_vy;
    double _pz,_vz,_theta;
    int _r_id; 
    bool yawrcv,velrcv,fixrcv;
    
    double start_x,start_y;
    int vx,vy;
    double delta_x,delta_y;
    int count;
    
    double xx,yy,zz,ww;
   
    OdomHandle(int r_id)
    {
        ros::NodeHandle n;
        //double xerror,yerror;
        yawrcv = false;
        velrcv = false;
        fixrcv = false;
        stringstream ss;
        ss<<"/uav"<<r_id<<"/states/ardrone3/PilotingState/AttitudeChanged";
        sub_yaw = n.subscribe(ss.str(), 1000, &OdomHandle::yawcb,this);
 
        stringstream ss1;
        //ss1<<"/uav"<<r_id<<"/states/ardrone3/PilotingState/SpeedChanged";
        ss1<<"/uav"<<r_id<<"/velctrl/input";
        sub_vel = n.subscribe(ss1.str(), 1000, &OdomHandle::velcb2,this);

        stringstream ss2;
        ss2<<"/uav"<<r_id<<"/fix";
        //sub_fix = n.subscribe(ss2.str(), 1000, &OdomHandle::fixcb,this);
        
        stringstream ss3;
        ss3<<"/uav"<<r_id<<"/position";
        pub = n.advertise<quadrotor_code::Status>(ss3.str(),1000);
        
        stringstream ss4;
        ss4<<"/uav"<<r_id<<"/neighbor";
        neighbor_pub = n.advertise<quadrotor_code::Neighbor>(ss4.str(),1000);
        
        stringstream ss5;
        ss5<<"/uav"<<r_id<<"/stop";
        //stop_pub = n.advertise<std_msgs::Int32>(ss5.str(),1000);
        stop_pub = n.advertise<std_msgs::Int32>("/stop",1000);
        
        stringstream ss6;
        ss6<<"/uav"<<r_id<<"/fix_odom";
        fix_odom_pub = n.advertise<nav_msgs::Odometry>(ss6.str(),1000);
        
        stringstream ss7;
        ss7<<"/uav"<<r_id<<"/odom";
        sub_odom = n.subscribe(ss7.str(), 1000, &OdomHandle::odomcb,this);
        
        _px=0;
        _py=0;
        _vx=0;
        _vy=0;
        _pz=0;
        _vz=0;
        _r_id = r_id;
        vx=r_id/line_uav_num;  //计算摆放位置的虚拟坐标
        vy=r_id%line_uav_num;
        
        odomx = 0;//vx * delta_dis;
        odomy = 0;//-vy * delta_dis;
        odomz = 0;
        
        odomtheta=0;
        count = 0;
        
        ww=1;
        xx=0;yy=0;zz=0;ww=1;
      
    }
    //NEU -> NWU
    void yawcb(const bebop_msgs::Ardrone3PilotingStateAttitudeChanged::ConstPtr & msg)
    {
        double msgtheta = msg->yaw;
        _theta = -msgtheta;
        if(count == 0)
        {
            odomtheta = _theta;
        }
        count++;
        /*
        if (msgtheta >= 0)
        {
             _theta = PI/2 - msgtheta;
        }
        else
        {
            if(msgtheta > -PI/2)
            {
                _theta = PI/2 - msgtheta;
            }
            else
            {
                _theta = - 3*PI/2 -msgtheta;
            }
        }*/
        yawrcv = true;
    }

    void velcb(const bebop_msgs::Ardrone3PilotingStateSpeedChanged::ConstPtr & msg)
    {
        _vx = msg->speedX;
        _vy = -msg->speedY;//NEU -> NWU
        velrcv = true;
    }
    
    void velcb2(const geometry_msgs::Twist::ConstPtr & msg)
    {
        _vx = msg->linear.x;
        _vy = -msg->linear.y;//NEU -> NWU
        velrcv = true;
    }
    /*
    void fixcb(const sensor_msgs::NavSatFix::ConstPtr & msg)
    {
        geographic_msgs::GeoPoint geo_pt;
        geo_pt.latitude = msg->latitude;
        geo_pt.longitude = msg->longitude;
        geo_pt.altitude = msg->altitude;
        geodesy::UTMPoint utm_pt(geo_pt);
        _py = utm_pt.easting;
        _px = utm_pt.northing;
        
        if(count<10)
        {
            start_x=utm_pt.northing;
            start_y=utm_pt.easting;

            if(vx==stand_vx && vy==stand_vy && count==5)
            {
                stand_x=start_x;
                stand_y=start_y;
            }

            if(count==9)
            {
                delta_x=((vx-stand_vx)*delta_dis+stand_x)-start_x;
                delta_y=((vy-stand_vy)*delta_dis+stand_y)-start_y;
                cout<<"uav"<<_r_id<<" aligned"<<endl;
            }

            count++;
        }
        else
        {
           _py = _py+delta_y - stand_y;
           _px = _px+delta_x - stand_x;
        }
        
        fixrcv = true;
        _py = - _py;//NEU -> NWU
    }
    */
    void odomcb(const nav_msgs::Odometry::ConstPtr & msg)
    {
        /*if(odom_first)
        {
            odom_start_time.data=msg->header.stamp;
            odom_first=false;
        }*/
        
        _px= msg->pose.pose.position.x+ odomx;
        _py= msg->pose.pose.position.y+ odomy;
        //cout<<vx<<' '<<vy<<' '<<_px<<' '<<_py<<endl;
        
        
        xx=msg->pose.pose.orientation.x;
        yy=msg->pose.pose.orientation.y;
        zz=msg->pose.pose.orientation.z;
        ww=msg->pose.pose.orientation.w;
        double tmp_vx=msg->twist.twist.linear.x;
        double tmp_vy=msg->twist.twist.linear.y;
        //serialize a class containning (_r_id,_px,_py,_vx,_vy) here,int*1,double*4
       
    }
};

static vector<OdomHandle*> odom_list;
int robotnum=50;
vector<vector<int> > adj_list;

double dist(int i,int j)
{
    double re=pow(odom_list[i]->_px-odom_list[j]->_px,2)+pow(odom_list[i]->_py-odom_list[j]->_py,2);
    //if(i==4)
    //cout<<re<<endl;
    return sqrt(re);
    
}

int main(int argc, char** argv)
{
   ros::init(argc,argv,"sim_manager");
   ros::NodeHandle n;
   srand(time(0));
   bool param_ok = ros::param::get ("~robotnum", robotnum);
   
   //ofstream fout("/home/liminglong/czx/traject.txt");
   //ofstream fout2("/home/liminglong/czx/velocity.txt");
   //ros::Publisher posepub = n.advertise<geometry_msgs::PoseArray>("/swarm_pose",1000);
   //rviz_goal_pub_ = n.advertise<decide_softbus_msgs::NavigationPoint>("/uav0/move_base_simple/goal", 0 );
   //ros::Subscriber rviz_sub = n.subscribe<geometry_msgs::PoseStamped>("/rviz_simple/goal", 1000, rvizGoalCb);
   ros::Publisher stringpub = n.advertise<std_msgs::String>("/broadcast",1000);
   //rviz_start_action_client_= n.serviceClient<decide_softbus_msgs::SetControlling>("/uav0/move_base/set_controlling");
   //ros::Subscriber rviz_start_action_sub = n.subscribe<geometry_msgs::PointStamped>("/rviz_simple/start_action", 1000, rvizStartActionCb);
   
   tf::TransformBroadcaster br;
   //for(int i=0;i<robotnum;i++)
   //{
      OdomHandle *p=new OdomHandle(robotnum);
      odom_list.push_back(p);//only one element
     // adj_list.push_back(vector<int>());
   //}
   
   //for(int i=0;i<robotnum;i++)
   //{
       stringstream ss;
       ss<<"~uav"<<robotnum<<"_x";
       param_ok = ros::param::get (ss.str(), odom_list[0]->odomx);
       if(!param_ok)
       {
           cout<<"[ERROR]getting x param failed"<<endl;
           exit(1);
       }
       stringstream ssy;
       ssy<<"~uav"<<robotnum<<"_y";
       param_ok = ros::param::get (ssy.str(), odom_list[0]->odomy);
       if(!param_ok)
       {
           cout<<"[ERROR]getting y param failed"<<endl;
           exit(1);
       }
   //}
   //neighbor_list.push_back(NeighborHandle(1));
   ros::Rate loop_rate(20);
   int count = 1;
   while(ros::ok())
   {  
      ros::spinOnce();
      for(int i=0;i<1;i++)
      {
           //publish status msg 
           quadrotor_code::Status sendmsg;
           sendmsg.px = odom_list[i]-> _px;
           sendmsg.py = odom_list[i]-> _py;
           sendmsg.vx = odom_list[i]-> _vx;
           sendmsg.vy = odom_list[i]-> _vy;
           sendmsg.theta =  odom_list[i]-> _theta;
           odom_list[i]->pub.publish(sendmsg);
           NeighborInfo ni(robotnum,odom_list[i]-> _px,odom_list[i]-> _py,odom_list[i]-> _vx,odom_list[i]-> _vy);
           ostringstream archiveStream;
           boost::archive::text_oarchive archive(archiveStream);
           archive<<ni;
           string ni_string=archiveStream.str();
           //sendstring here
           std_msgs::String sendstring;
           sendstring.data = ni_string;
           stringpub.publish(sendstring);
           //
           istringstream iarchiveStream(ni_string);
           boost::archive::text_iarchive iarchive(iarchiveStream);
           NeighborInfo ni_new;
           //cout<<ni_new._r_id<<endl;
           iarchive>>ni_new;
           //cout<<ni_new._r_id<<endl;
           
      }
      //publish odom 
      for(int i=0;i<1;i++)
      {
          nav_msgs::Odometry odommsg;
          stringstream ss;
          ss<<"uav"<<robotnum<<"/odom";
          odommsg.header.frame_id="map";
          odommsg.child_frame_id=ss.str().c_str();
          odommsg.pose.pose.position.x=odom_list[i]-> _px;
          odommsg.pose.pose.position.y=odom_list[i]-> _py;
          odommsg.pose.pose.position.z=odom_list[i]-> _pz;
         
          odommsg.pose.pose.orientation.x=odom_list[i]->xx;
          odommsg.pose.pose.orientation.y=odom_list[i]->yy;
          odommsg.pose.pose.orientation.z=odom_list[i]->zz;
          odommsg.pose.pose.orientation.w=odom_list[i]->ww;
          
          odom_list[i]->fix_odom_pub.publish(odommsg);
      }
      //publish tf map->odom
      for(int i=0;i<1;i++)
      {
          tf::Transform transform;
          transform.setOrigin( tf::Vector3(odom_list[i]->odomx,odom_list[i]->odomy , odom_list[i]->odomz) );
          tf::Quaternion q;
          //q.setRPY(0, 0, odom_list[i]->odomtheta);
          q.setRPY(0, 0, 0);
          transform.setRotation(q);
          
          stringstream ss;
          ss<<"uav"<<robotnum<<"/odom";
          
          br.sendTransform(tf::StampedTransform(transform, ros::Time::now(), "map", ss.str().c_str()));
          //cout<<"sent"<<endl;
      }
      count++;
      loop_rate.sleep();
   }
   return 0;
}
