#include "WebDataSerializer.h"
#include "SimpleDataSerializer.h"


namespace FusionCrowdWeb
{
	using namespace FusionCrowd;

	size_t WebDataSerializer<InitComputingData>::Serialize(const InitComputingData& inData, char*& outRawData)
	{
		size_t dataSize = inData.NavMeshFile.GetSize() 
			+ SimpleDataSerializer<FCArray<AgentInitData>>::SizeOf(inData.AgentsData);
		outRawData = new char[dataSize];
		auto iter = outRawData;

		inData.NavMeshFile.WriteToMemory(iter);
		SimpleDataSerializer<FCArray<AgentInitData>>::Serialize(inData.AgentsData, iter);

		return dataSize;
	}


	InitComputingData WebDataSerializer<InitComputingData>::Deserialize(const char* inRawData)
	{
		FcFileWrapper navMeshFile;
		navMeshFile.ReadFromMemory(inRawData);
		FCArray<AgentInitData> agentsData = SimpleDataSerializer<FCArray<AgentInitData>>::Deserialize(inRawData);

		return InitComputingData{ std::move(navMeshFile), agentsData };
	}


	size_t WebDataSerializer<InputComputingData>::Serialize(const InputComputingData& inData, char*& outRawData)
	{
		size_t dataSize = sizeof(float);
		outRawData = new char[dataSize];
		auto iter = outRawData;

		SimpleDataSerializer<float>::Serialize(inData.TimeStep, iter);

		return dataSize;
	}


	InputComputingData WebDataSerializer<InputComputingData>::Deserialize(const char* inRawData)
	{
		float timeStep = SimpleDataSerializer<float>::Deserialize(inRawData);
		return InputComputingData{ timeStep };
	}


	size_t WebDataSerializer<OutputComputingData>::Serialize(const OutputComputingData& inData, char*& outRawData)
	{
		size_t dataSize = SimpleDataSerializer<FCArray<AgentInfo>>::SizeOf(inData.AgentInfos);
		outRawData = new char[dataSize];
		auto iter = outRawData;

		SimpleDataSerializer<FCArray<AgentInfo>>::Serialize(inData.AgentInfos, iter);

		return dataSize;
	}


	OutputComputingData WebDataSerializer<OutputComputingData>::Deserialize(const char* inRawData)
	{
		FCArray<AgentInfo> agents = SimpleDataSerializer<FCArray<AgentInfo>>::Deserialize(inRawData);

		return OutputComputingData{ agents };
	}
}