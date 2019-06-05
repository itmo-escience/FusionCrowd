#pragma once
#include "Config.h"
#include "Navigation/Obstacle.h"
#include "Math/Util.h"

#include <vector>

namespace FusionCrowd
{
	//Forward Declaration
	class Agent;

	class FUSION_CROWD_API ProximityQuery
	{
	public:
		ProximityQuery() {}
		virtual ~ProximityQuery() {}

		virtual void StartQuery() = 0;
		virtual DirectX::SimpleMath::Vector2 GetQueryPoint() = 0;
		virtual float GetMaxAgentRange() = 0;
		virtual float GetMaxObstacleRange() = 0;
		virtual void FilterAgent(const Agent * agent, float distSq) = 0;
		virtual void FilterObstacle(const Obstacle * obstacle, float distSq) = 0;
	};
}