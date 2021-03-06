#include "FcClient.h"

#include <vector>
#include <iostream>
#include <iomanip>


int main()
{
	using namespace std;
	using namespace FusionCrowdWeb;

	FusionCrowdClient::GlobalStartup();
	cout << "---Fusion Crowd Client---" << endl << endl;

	FusionCrowdClient client;

	cout << "Connecting to main server... ";
	client.ConnectToMainServer(WebAddress("127.0.0.1", 49000));
	cout << "success" << endl;

	FusionCrowd::FCArray<AgentInitData> agentsData(2);
	agentsData[0] = { -20.f, 0, 20.f, 0 };
	agentsData[1] = { 20.f, 0, -20.f, 0 };

	auto navMeshFileName = FcFileWrapper::GetFullNameForResource("grid.nav");
	InitComputingData initData(navMeshFileName, agentsData);
	delete navMeshFileName;

	client.InitComputation(initData);
	cout << "Computation initialization requested" << endl;

	cout << "Computing steps requesting... ";
	const int stepsNum = 50;
	vector<OutputComputingData> results;
	for (int i = 0; i < stepsNum; i++)
	{
		auto result = client.RequestComputation(InputComputingData(0.1f, FusionCrowd::FCArray<ChangeGoalData>(0)));
		results.push_back(result);
	}
	cout << "success" << endl;

	client.DisconnectFromMainServer();
	cout << "Disconnection from main server" << endl << endl;

	FusionCrowdClient::GlobalCleanup();

	cout << "Results:" << endl << endl;
	cout << setprecision(3);
	for (int i = 0; i < results.size(); i += 5)
	{
		for (auto info : results[i].AgentInfos)
		{
			cout << setw(10) << info.Id << setw(10) << info.PosX << setw(10) << info.PosY;
		}
		cout << endl;
	}
	cout << endl;

	system("pause");

	return 0;
}
