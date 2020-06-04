#include "FcComputationalServer.h"

#include "WsException.h"
#include "WebMessage.h"
#include "FcWebData.h"
#include "WebDataSerializer.h"
#include "FcFileWrapper.h"

#include <iostream>


namespace FusionCrowdWeb
{
	void FcComputationalServer::StartServer(WebAddress inAddress)
	{
		WebNode::StartServer(inAddress);
		std::cout << "Successfully started on " << inAddress.IpAddress << ':' << inAddress.Port << std::endl << std::endl;
	}


	void FcComputationalServer::AcceptMainServerConnection()
	{
		_mainServerId = AcceptInputConnection();
		std::cout << "MainServer connected" << std::endl << std::endl;
	}


	void FcComputationalServer::InitComputation()
	{
		//reading data
		auto request = Receive(_mainServerId);
		if (request.first.AsRequestCode != RequestCode::InitSimulation)
		{
			throw FcWebException("RequestError");
		}
		auto data = WebDataSerializer<InitComputingData>::Deserialize(request.second);
		std::cout << "Init data received" << std::endl;

		//initing simulation
		_builder = std::shared_ptr<ISimulatorBuilder>(BuildSimulator(), [](ISimulatorBuilder* inBuilder)
		{
			BuilderDeleter(inBuilder);
		});

		for (auto component : ComponentIds::allOperationComponentTypes)
		{
			_builder->WithOp(component);
		}

		data.NavMeshFile.SetFileNameAsResource("duplicate.nav");
		data.NavMeshFile.Unwrap();

		_builder->WithNavMesh(data.NavMeshFile.GetFileName());

		_simulator = std::shared_ptr<ISimulatorFacade>(_builder->Build(), [](ISimulatorFacade* inSimulatorFacade)
		{
			SimulatorFacadeDeleter(inSimulatorFacade);
		});
		_simulator->SetIsRecording(false);

		//initing agents
		for (auto& agentData : data.AgentsData)
		{
			auto agentId = _simulator->AddAgent(agentData.X, agentData.Y, 1, 10,
				ComponentIds::ORCA_ID, ComponentIds::NAVMESH_ID, ComponentIds::FSM_ID);
			_simulator->SetAgentGoal(agentId, Point(agentData.GoalX, agentData.GoalY));
		}
	}


	void FcComputationalServer::ProcessComputationRequest()
	{
		auto request = Receive(_mainServerId);
		if (request.first.AsRequestCode != RequestCode::DoStep)
		{
			throw FcWebException("RequestError");
		}
		auto inData = WebDataSerializer<InputComputingData>::Deserialize(request.second);
		std::cout << "Computing data received" << std::endl;

		_simulator->DoStep(inData.TimeStep);

		FCArray<AgentInfo> agents(_simulator->GetAgentCount());
		_simulator->GetAgents(agents);
		OutputComputingData outData = OutputComputingData{ agents };

		Send(_mainServerId, ResponseCode::Success, outData);
		std::cout << "Computing result sent" << std::endl;
	}
}
