#include "ZanlungoComponent.h"

#include "Math/geomQuery.h"
#include "Math/Util.h"
#include "Navigation/Obstacle.h"
#include "Navigation/AgentSpatialInfo.h"
#include "Navigation/NavSystem.h"

using namespace DirectX::SimpleMath;

namespace FusionCrowd
{
	namespace Zanlungo
	{
		class ZanlungoComponent::ZanlungoComponentImpl
		{
		public:
			ZanlungoComponentImpl(Simulator & simulator)
				: _simulator(simulator), _navSystem(simulator.GetNavSystem()), _agentScale(2000), _obstScale(2000), _reactionTime(0.5f), _forceDistance(0.08f)
			{
			}

			ZanlungoComponentImpl(Simulator & simulator, float agentScale, float obstScale, float reactionTime, float forceDistance)
				: _simulator(simulator), _navSystem(simulator.GetNavSystem()), _agentScale(agentScale), _obstScale(obstScale), _reactionTime(reactionTime), _forceDistance(forceDistance)
			{
			}

			~ZanlungoComponentImpl() = default;

			std::string GetName() { return "zanlungo"; };

			void AddAgent(size_t id)
			{
				AddAgent(id, 80.0f);
			}

			void AddAgent(size_t id, float mass)
			{
				_agents[id] = ZAgentParamentrs(mass);
			}

			bool DeleteAgent(size_t id)
			{
				_agents.erase(id);

				return true;
			}

			void Update(float timeStep)
			{
				for (auto p : _agents)
				{
					auto id = p.first;
					AgentSpatialInfo & agent = _simulator.GetNavSystem().GetSpatialInfo(id);
					ComputeNewVelocity(agent, timeStep);
				}
			}

		private:
			void ComputeNewVelocity(AgentSpatialInfo & agent, float timeStep)
			{
				float T_i;
				bool interacts = ComputeTTI(&agent, T_i, timeStep);

				Vector2 force(DrivingForce(&agent));

				if (interacts) {
					// if T_i never got set, there are no interactions to do
					const float SPEED = agent.vel.Length();
					const float B = _forceDistance;

					std::vector<AgentSpatialInfo> nearAgents = _navSystem.GetNeighbours(agent.id);
					// const float MAG = Simulator::AGENT_SCALE * SPEED / T_i;
					for (size_t j = 0; j < nearAgents.size(); ++j) {
						// 2. Use T_i to compute the direction
						AgentSpatialInfo other = nearAgents[j];

						force += AgentForce(&agent, &other, T_i);
					}
					// obstacles
					Vector2 futurePos = agent.pos + agent.vel * T_i;
					const float OBST_MAG = _obstScale * SPEED / T_i;
					for (auto obst : _navSystem.GetClosestObstacles(agent.id)) {
						Vector2 nearPt;  // set by call to distanceSqToPoint
						float d2;        // set by call to distanceSqToPoint
						if (obst.distanceSqToPoint(futurePos, nearPt, d2) == Obstacle::LAST)
							continue;
						Vector2 D_ij = futurePos - nearPt;
						float dist = D_ij.Length();
						D_ij /= dist;
						dist -= agent.radius;
						force += D_ij * (OBST_MAG * expf(-dist / B));
					}
				}

				Vector2 acc = force / _agents[agent.id]._mass;
				agent.velNew = agent.vel + acc * timeStep;
			}

			bool ComputeTTI(AgentSpatialInfo* agent, float& T_i, float timeStep) const
			{
				const float COS_FOV = -0.8f;  // cos( HALFPI );// cos( PI / 4.f ); //
				bool interacts = false;
				T_i = MathUtil::INFTY;
#define COLLIDE_PRIORITY
#ifdef COLLIDE_PRIORITY
				float t_collision = T_i;
#endif
				std::vector<AgentSpatialInfo> nearAgents = _navSystem.GetNeighbours(agent->id);
				for (size_t j = 0; j < nearAgents.size(); ++j) {
					AgentSpatialInfo other = nearAgents[j];

					// Right of way-dependent calculations
					Vector2 myVel = agent->vel;
					Vector2 hisVel = other.vel;
					//m RightOfWayVel(agent, hisVel, other->_velPref.getPreferredVel(), other->_priority, myVel);

					const Vector2 relVel = myVel - hisVel;
					Vector2 relPos = agent->pos - other.pos;
					//	This define determines if additional prediction code is executed
					//		The original zanlungo model does not include performing exact collisions
					//		between disks.  It simply estimates the time to interaction based on
					//		projection of center on preferred velocity.
					//		This define includes a precise test of ray-circle intersection to test
					//		to see if the relative velocity intersects the minkowski sum of this
					//		agent with its neighbor.  It makes the respones far more robust.
#define PRECISE
#ifdef PRECISE
					float circRadius = agent->radius + other.radius;
					// first test to see if there's an actual collision imminent
					float contactT = Math::rayCircleTTC(relVel, -relPos, circRadius);
					// std::cout << "\tColliding with " << other->_id << " at " << contactT << "\n";
#ifdef COLLIDE_PRIORITY
					if (contactT < t_collision) {
						// the ray intersects the circle -- actual collision is possible
						t_collision = contactT;
						interacts = true;
					}
					else if (t_collision == MathUtil::INFTY) {
#else
					if (contactT < T_i) {
						// the ray intersects the circle -- actual collision is possible
						T_i = contactT;
						interacts = true;
					}
					else {
#endif  // COLLIDE_PRIORITY
#endif  // PRECISE
						// no collision possible, see if getting close is possible
#ifdef ZANLUNGO_FOV
						float dist = abs(relPos);
						float relSpeed = abs(relVel);
						// This is the angle between relative position and ORIENTATION
						float cosTheta = -(relPos * _orient) / dist;
						if (cosTheta >= COS_FOV) {
							// This is the angle between relative position and relative velocity
							cosTheta = relPos * relVel / (-dist * relSpeed);
							float t_ij = cosTheta * dist / relSpeed;
							// std::cout << "\tApproaching " << other->_id << " at " << t_ij << "\n";
							if (t_ij < T_i) {
								T_i = t_ij;
								interacts = true;
							}
						}
#else
	// note: relPos points from other agent to this agent, so they need to point in
	// OPPOSITE directions for convergence
						float dp = -relPos.Dot(relVel);
						if (dp > 0.f) {
							float t_ij = dp / relVel.LengthSquared();
							if (t_ij < T_i) {
								T_i = t_ij;
								interacts = true;
							}
						}
#endif
#ifdef PRECISE
					}
#endif
				}
				// Compute time to interaction for obstacles
				for (auto obst : _navSystem.GetClosestObstacles(agent->id)) {
					// TODO: Interaction with obstacles is, currently, defined strictly
					//	by COLLISIONS.  Only if I'm going to collide with an obstacle is
					//	a force applied.  This may be too naive.
					//	I'll have to investigate this.
					float t = obst.circleIntersection(agent->vel, agent->pos, agent->radius);
					if (t < T_i) {
						T_i = t;
						interacts = true;
					}
				}
#ifdef COLLIDE_PRIORITY
				if (t_collision < MathUtil::INFTY) T_i = t_collision;
#endif
				if (T_i < timeStep) {
					T_i = timeStep;
				}

				return interacts;
			}

			//float RightOfWayVel(FusionCrowd::Agent* agent, Vector2& otherVel, const Vector2& otherPrefVel,
			//	float otherPriority, Vector2& vel) const
			//{
			//	//float rightOfWay = agent->_priority - otherPriority;
			//	rightOfWay = (rightOfWay < -1.f) ? -1.f : (rightOfWay > 1.f) ? 1.f : rightOfWay;
			//	if (rightOfWay < 0.f) {
			//		float R2 = sqrtf(-rightOfWay);  // rightOfWay * rightOfWay; // -rightOfWay; //
			//		vel = agent->_vel;
			//		otherVel += R2 * (otherPrefVel - otherVel);
			//		return -R2;
			//	}
			//	else if (rightOfWay > 0.f) {
			//		float R2 = sqrtf(rightOfWay);  // rightOfWay * rightOfWay; // rightOfWay; //
			//		vel = agent->_vel + R2 * (agent->prefVelocity.getSpeed() - agent->_vel);
			//		return R2;
			//	}
			//	else {
			//		vel = agent->vel;
			//		return 0.f;
			//	}
			//}

			Vector2 AgentForce(AgentSpatialInfo* agent, AgentSpatialInfo * other, float T_i) const
			{
				float D = _forceDistance;
				// Right of way-dependent calculations
				Vector2 myVel = agent->vel;
				Vector2 hisVel = other->vel;
				float weight =
					1.f; //- RightOfWayVel(agent, hisVel, other->prefVelocity.getPreferred(), other->priority, myVel);

				const Vector2 relVel = myVel - hisVel;

				Vector2 futPos = agent->pos + myVel * T_i;
				Vector2 otherFuturePos = other->pos + hisVel * T_i;
				Vector2 D_ij = futPos - otherFuturePos;

				// If the relative velocity is divergent do nothing
				if (D_ij.Dot(agent->vel - other->vel) > 0.f) return Vector2(0.f, 0.f);
				float dist = D_ij.Length();
				D_ij /= dist;
				//if (weight > 1.f) {
				//	// Other agent has right of way
				//	float prefSpeed = other->prefVelocity.getSpeed();
				//	Vector2 perpDir;
				//	bool interpolate = true;
				//	if (prefSpeed < 0.0001f) {
				//		// he wants to be stationary, accelerate perpinduclarly to displacement
				//		Vector2 currRelPos = agent->pos - other->pos;
				//		perpDir = Vector2(-currRelPos.y, currRelPos.x);
				//		if (perpDir.Dot(agent->vel) < 0.f)
				//			perpDir *= -1;
				//	}
				//	else {
				//		// He's moving somewhere, accelerate perpindicularly to his preferred direction
				//		// of travel.
				//		const Vector2 prefDir(other->prefVelocity.getPreferred());
				//		if (prefDir.Dot(D_ij) > 0.f) {
				//			// perpendicular to preferred velocity
				//			perpDir = Vector2(-prefDir.y, prefDir.x);
				//			if (perpDir.Dot(D_ij) < 0.f)
				//				perpDir *= -1;
				//		}
				//		else {
				//			interpolate = false;
				//		}
				//	}
				//	// spherical linear interpolation
				//	if (interpolate) {
				//		float sinTheta = MathUtil::det(perpDir, D_ij);
				//		if (sinTheta < 0.f) {
				//			sinTheta = -sinTheta;
				//		}
				//		if (sinTheta > 1.f) {
				//			sinTheta = 1.f;  // clean up numerical error arising from determinant
				//		}
				//		D_ij = Math::slerp(weight - 1.f, D_ij, perpDir, sinTheta);
				//	}
				//}
				dist -= (agent->radius + other->radius);
				float magnitude = weight * _agentScale * (agent->vel - other->vel).Length() / T_i;
				const float MAX_FORCE = 1e15f;
				if (magnitude >= MAX_FORCE) {
					magnitude = MAX_FORCE;
				}
				// float magnitude = weight * Simulator::AGENT_SCALE * abs( myVel - hisVel ) / T_i;
				// 3. Compute the force
				return D_ij * (magnitude * expf(-dist / D));
			}

			Vector2 DrivingForce(AgentSpatialInfo* agent)
			{
				auto & agentInfo = _simulator.GetNavSystem().GetSpatialInfo(agent->id);
				return (agentInfo.prefVelocity.getPreferredVel() - agent->vel) * (_agents[agent->id]._mass / _reactionTime);
			}

			Simulator & _simulator;
			NavSystem & _navSystem;
			std::map<int, ZAgentParamentrs> _agents;
			float _agentScale;
			float _obstScale;
			float _reactionTime;
			float _forceDistance;
		};

		ZanlungoComponent::ZanlungoComponent(Simulator & simulator)
			: pimpl(std::make_unique<ZanlungoComponentImpl>(simulator))
		{
		}

		ZanlungoComponent::ZanlungoComponent(Simulator & simulator, float agentScale, float obstScale, float reactionTime, float forceDistance)
			: pimpl(std::make_unique<ZanlungoComponentImpl>(simulator, agentScale, obstScale, reactionTime, forceDistance))
		{
		}

		ZanlungoComponent::~ZanlungoComponent() = default;

		std::string ZanlungoComponent::GetName()
		{
			return pimpl->GetName();
		}

		void ZanlungoComponent::AddAgent(size_t id)
		{
			pimpl->AddAgent(id);
		}

		void ZanlungoComponent::AddAgent(size_t id, float mass)
		{
			pimpl->AddAgent(id, mass);
		}

		bool ZanlungoComponent::DeleteAgent(size_t id)
		{
			return pimpl->DeleteAgent(id);
		}

		void ZanlungoComponent::Update(float timeStep)
		{
			pimpl->Update(timeStep);
		}
	}
}
