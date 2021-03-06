//====================================================================================== 
#include "RockBridge.hpp"

#include <std/typekit/Plugin.hpp>
#include <std/transports/corba/TransportPlugin.hpp>
#include <std/transports/typelib/TransportPlugin.hpp>
#include <std/transports/mqueue/TransportPlugin.hpp>

#include <base/typekit/Plugin.hpp>
#include <base/transports/corba/TransportPlugin.hpp>
#include <base/transports/typelib/TransportPlugin.hpp>
#include <base/transports/mqueue/TransportPlugin.hpp>

#include <rock_gazebo/ModelTask.hpp>
#include <rock_gazebo/WorldTask.hpp>
#include <rock_gazebo/ThrusterTask.hpp>
#include <rock_gazebo/LaserScanTask.hpp>
#include <rock_gazebo/CameraTask.hpp>
#include <rock_gazebo/ImuTask.hpp>
#include <rock_gazebo/GPSTask.hpp>
#include <rock_gazebo/typekit/Plugin.hpp>
#include <rock_gazebo/transports/corba/TransportPlugin.hpp>
#include <rock_gazebo/transports/typelib/TransportPlugin.hpp>
#include <rock_gazebo/transports/mqueue/TransportPlugin.hpp>

#include <logger/Logger.hpp>
#include <logger/typekit/Plugin.hpp>
#include <logger/transports/corba/TransportPlugin.hpp>
#include <logger/transports/typelib/TransportPlugin.hpp>
#include <logger/transports/mqueue/TransportPlugin.hpp>

#include <gps_base/typekit/Plugin.hpp>
#include <gps_base/transports/corba/TransportPlugin.hpp>
#include <gps_base/transports/typelib/TransportPlugin.hpp>
#include <gps_base/transports/mqueue/TransportPlugin.hpp>

#include <rtt/Activity.hpp>
#include <rtt/TaskContext.hpp>
#include <rtt/base/ActivityInterface.hpp>
#include <rtt/extras/SequentialActivity.hpp>
#include <rtt/transports/corba/ApplicationServer.hpp>
#include <rtt/transports/corba/TaskContextServer.hpp>
#include <rtt/transports/corba/CorbaDispatcher.hpp>


using namespace std;
using namespace gazebo;
using namespace rock_gazebo;

RockBridge::RockBridge()
{
}

RockBridge::~RockBridge()
{
    // Deregister the CORBA stuff
    RTT::corba::TaskContextServer::CleanupServers();
    RTT::corba::CorbaDispatcher::ReleaseAll();

    // Delete pointers to activity
    for(Activities::iterator activity_it = activities.begin();
            activity_it != activities.end(); ++activity_it)
    {
        delete *activity_it;
    }

    // Delete pointers to tasks
    for(Tasks::iterator task_it = tasks.begin();
            task_it != tasks.end(); ++task_it)
    {
        delete *task_it;
    }

    RTT::corba::TaskContextServer::ShutdownOrb();
    RTT::corba::TaskContextServer::DestroyOrb();
}

void RockBridge::Load(int _argc , char** _argv)
{
    RTT::corba::ApplicationServer::InitOrb(_argc, _argv);
    RTT::corba::TaskContextServer::ThreadOrb(ORO_SCHED_OTHER, RTT::os::LowestPriority, 0);

    // Import typekits to allow RTT convert the types used by the components
    RTT::types::TypekitRepository::Import(new orogen_typekits::stdTypekitPlugin);
    RTT::types::TypekitRepository::Import(new orogen_typekits::stdCorbaTransportPlugin);
    RTT::types::TypekitRepository::Import(new orogen_typekits::stdMQueueTransportPlugin);
    RTT::types::TypekitRepository::Import(new orogen_typekits::stdTypelibTransportPlugin);

    RTT::types::TypekitRepository::Import(new orogen_typekits::baseTypekitPlugin);
    RTT::types::TypekitRepository::Import(new orogen_typekits::baseCorbaTransportPlugin);
    RTT::types::TypekitRepository::Import(new orogen_typekits::baseMQueueTransportPlugin);
    RTT::types::TypekitRepository::Import(new orogen_typekits::baseTypelibTransportPlugin);

    RTT::types::TypekitRepository::Import(new orogen_typekits::gps_baseTypekitPlugin);
    RTT::types::TypekitRepository::Import(new orogen_typekits::gps_baseCorbaTransportPlugin);
    RTT::types::TypekitRepository::Import(new orogen_typekits::gps_baseMQueueTransportPlugin);
    RTT::types::TypekitRepository::Import(new orogen_typekits::gps_baseTypelibTransportPlugin);

    RTT::types::TypekitRepository::Import(new orogen_typekits::rock_gazeboTypekitPlugin);
    RTT::types::TypekitRepository::Import(new orogen_typekits::rock_gazeboCorbaTransportPlugin);
    RTT::types::TypekitRepository::Import(new orogen_typekits::rock_gazeboMQueueTransportPlugin);
    RTT::types::TypekitRepository::Import(new orogen_typekits::rock_gazeboTypelibTransportPlugin);

    RTT::types::TypekitRepository::Import(new orogen_typekits::loggerTypekitPlugin);
    RTT::types::TypekitRepository::Import(new orogen_typekits::loggerCorbaTransportPlugin);
    RTT::types::TypekitRepository::Import(new orogen_typekits::loggerMQueueTransportPlugin);
    RTT::types::TypekitRepository::Import(new orogen_typekits::loggerTypelibTransportPlugin);

    // Each simulation step the Update method is called to update the simulated sensors and actuators
    eventHandler.push_back(
            event::Events::ConnectWorldUpdateBegin(
                boost::bind(&RockBridge::updateBegin,this, _1)));
    eventHandler.push_back(
            event::Events::ConnectWorldCreated(
                boost::bind(&RockBridge::worldCreated,this, _1)));
    eventHandler.push_back(
            event::Events::ConnectAddEntity(
                boost::bind(&RockBridge::entityCreated,this, _1)));
}

// worldCreated() is called every time a world is aCreatedvoid RockBridge::entityCreated(string const& entityName)
void RockBridge::entityCreated(string const& entityName)
{
    gzmsg << "RockBridge: added entity: "<< entityName << endl;

    /*if (entityName == "ned" || entityName == "ocean_box") {
        return;
	}*/

    physics::WorldPtr world = physics::get_world(this->worldName);
    if (!world)
    {
        gzerr << "RockBridge: cannot find world " << worldName << endl;
        return;
    }

	/*
    typedef physics::Model_V Model_V;
    Model_V model_list = world->GetModels();
    for(Model_V::iterator model_it = model_list.begin(); model_it != model_list.end(); ++model_it)
    {
        gzmsg << "RockBridge: comparing model: "<< (*model_it)->GetName() << endl;
		if ((*model_it)->GetName() != entityName) {
		    continue;
		}
        gzmsg << "RockBridge: initializing model: "<< (*model_it)->GetName() << endl;
        sdf::ElementPtr modelElement = (*model_it)->GetSDF();

        // Create and initialize a component for each gazebo model
        ModelTask* model_task = new ModelTask();
        model_task->setGazeboModel(world, (*model_it) );
        setupTaskActivity(model_task);

        // Create and initialize a component for each model plugin
        instantiatePluginComponents( modelElement, (*model_it) );

        // Create and initialize a component for each sensor
        instantiateSensorComponents( modelElement, (*model_it) );
    }
    model_list.clear();
	*/

    gzmsg << "RockBridge: before get model: "<< entityName << endl;
	//physics::ModelPtr model; // = world->getByName(entityName);
	//model = model->GetByName(entityName);
	physics::EntityPtr entity = world->GetEntity(entityName);
    gzmsg << "RockBridge: while get model: "<< entity->GetScopedName() << endl;
	entity->Print("  ");
	
	//physics::ModelPtr model = world->GetModel(entityName);
	//physics::ModelPtr model(new physics::Model(entity));
	physics::ModelPtr model = entity->GetParentModel();
	model->Print("  ");
    //gzmsg << "RockBridge: initializing model: "<< model->GetName() << endl;
    
	sdf::ElementPtr modelElement = model->GetSDF();

    // Create and initialize a component for each gazebo model
    ModelTask* model_task = new ModelTask();
    model_task->setGazeboModel(world, model);
    setupTaskActivity(model_task);

    // Create and initialize a component for each model plugin
    instantiatePluginComponents( modelElement, model);

    // Create and initialize a component for each sensor
    instantiateSensorComponents( modelElement, model);

    gzmsg << "RockBridge: added model: "<< entityName << endl;
}

// worldCreated() is called every time a world is added
void RockBridge::worldCreated(string const& worldName)
{
    // Create the logger component and start the activity
    logger::Logger* logger_task = new logger::Logger();
    logger_task->provides()->setName("gazebo:" + worldName +"_Logger");
    // RTT::Activity runs the task in separate thread
    RTT::Activity* logger_activity = new RTT::Activity( logger_task->engine() );
    RTT::corba::TaskContextServer::Create( logger_task );
    RTT::corba::CorbaDispatcher::Instance( logger_task->ports(), ORO_SCHED_OTHER, RTT::os::LowestPriority );
    logger_activity->start();
    activities.push_back( logger_activity );
    tasks.push_back( logger_task );

    physics::WorldPtr world = physics::get_world(worldName);
    //world = physics::get_world(worldName);
    if (!world)
    {
        gzerr << "RockBridge: cannot find world " << worldName << endl;
        return;
    }

    gzmsg << "RockBridge: initializing world: " << worldName << endl;
    WorldTask* world_task = new WorldTask();
    world_task->setGazeboWorld(world);
    setupTaskActivity(world_task);
	this->worldName = worldName;

	/*
    typedef physics::Model_V Model_V;
    Model_V model_list = world->GetModels();
    for(Model_V::iterator model_it = model_list.begin(); model_it != model_list.end(); ++model_it)
    {
        gzmsg << "RockBridge: initializing model: "<< (*model_it)->GetName() << endl;
        sdf::ElementPtr modelElement = (*model_it)->GetSDF();

        // Create and initialize a component for each gazebo model
        ModelTask* model_task = new ModelTask();
        model_task->setGazeboModel(world, (*model_it) );
        setupTaskActivity(model_task);

        // Create and initialize a component for each model plugin
        instantiatePluginComponents( modelElement, (*model_it) );

        // Create and initialize a component for each sensor
        instantiateSensorComponents( modelElement, (*model_it) );
    }
    model_list.clear();
	*/
}

void RockBridge::setupTaskActivity(RTT::TaskContext* task)
{
    // Export the component interface on CORBA to Ruby access the component
    RTT::corba::TaskContextServer::Create( task );
    RTT::corba::CorbaDispatcher::Instance( task->ports(), ORO_SCHED_OTHER, RTT::os::LowestPriority );

    // Create and start sequential task activities
    RTT::extras::SequentialActivity* activity =
        new RTT::extras::SequentialActivity(task->engine());
    activity->start();
    activities.push_back(activity);
    tasks.push_back(task);
}

// Callback method triggered every update begin
// It triggers all rock components (world, model and plugins)
void RockBridge::updateBegin(common::UpdateInfo const& info)
{
    for(Activities::iterator it = activities.begin(); it != activities.end(); ++it)
    {
        (*it)->trigger();
    }
}


void RockBridge::instantiatePluginComponents(sdf::ElementPtr modelElement, ModelPtr model)
{
    sdf::ElementPtr pluginElement = modelElement->GetElement("plugin");
    while(pluginElement) {
        string pluginFilename = pluginElement->Get<string>("filename");
        gzmsg << "RockBridge: found plugin "<< pluginFilename << endl;
        // Add more model plugins testing them here
        if(pluginFilename == "libgazebo_thruster.so")
        {
            ThrusterTask* thruster_task = new ThrusterTask();
            thruster_task->setGazeboModel( model );
            setupTaskActivity(thruster_task);
        }

        pluginElement = pluginElement->GetNextElement("plugin");
    }
}

template<typename RockTask>
void RockBridge::setupSensorTask(ModelPtr model, sdf::ElementPtr sensorElement)
{
    string sensorName = sensorElement->Get<string>("name");
    string sensorType = sensorElement->Get<string>("type");
    gzmsg << "RockBridge: creating " << sensorType << " component: " + sensorName << endl;
    rock_gazebo::SensorTask* task = new RockTask();
    task->setGazeboModel(model, sensorElement);
    setupTaskActivity(task);
}

void RockBridge::instantiateSensorComponents(sdf::ElementPtr modelElement, ModelPtr model)
{
    sdf::ElementPtr linkElement = modelElement->GetElement("link");
    while( linkElement ){
        sdf::ElementPtr sensorElement = linkElement->GetElement("sensor");
        while( sensorElement ){
            string sensorName = sensorElement->Get<string>("name");
            string sensorType = sensorElement->Get<string>("type");

            gzmsg << "RockGazebo: adding sensor " << sensorName << " of type " << sensorType << endl;
            if (sensorType == "ray")
                setupSensorTask<LaserScanTask>(model, sensorElement);
            else if(sensorType == "camera")
                setupSensorTask<CameraTask>(model, sensorElement);
            else if(sensorType == "imu")
                setupSensorTask<ImuTask>(model, sensorElement);
            else if (sensorType == "gps")
                setupSensorTask<GPSTask>(model, sensorElement);
            else
                gzmsg << "RockGazebo: cannot handle sensor " << sensorName << " of type " << sensorType << endl;

            sensorElement = sensorElement->GetNextElement("sensor");
        }
        linkElement = linkElement->GetNextElement("link");
    }
}

