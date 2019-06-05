#include "Goal.h"
#include "GoalSet.h"

namespace FusionCrowd
{
	Goal::~Goal()
	{
		if (_geometry) delete _geometry;
	}

	void Goal::setGeometry(FusionCrowd::Math::Geometry2D* geometry)
	{
		if (_geometry != 0x0) delete _geometry;
		_geometry = geometry;
	}

	bool Goal::hasCapacity() const
	{
		bool result = _population < _capacity;
		return result;
	}

	void Goal::assign(const size_t agentId)
	{
		++_population;
		if (_population >= _capacity && _goalSet) _goalSet->setGoalFull(this);
	}

	void Goal::free()
	{
		if (_population >= _capacity && _goalSet) _goalSet->setGoalAvailable(this);
		--_population;
	}
}