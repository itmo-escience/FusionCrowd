#include "FcWebData.h"

#include "Export/INavMeshPublic.h"


namespace FusionCrowdWeb
{
	NavMeshRegion::NavMeshRegion()
	{
	}


	NavMeshRegion::NavMeshRegion(const std::string& inNavMeshPath)
	{
		using namespace FusionCrowd;

		auto vertices = NavMeshHelper::LoadNavMeshVertices(inNavMeshPath.c_str());

		if (vertices.size() == 0)
		{
			return;
		}

		float maxX, minX, maxY, minY;
		maxX = minX = vertices[0].X;
		maxY = minY = vertices[0].Y;

		for (auto vertex : vertices)
		{
			maxX = vertex.X > maxX ? vertex.X : maxX;
			minX = vertex.X < minX ? vertex.X : minX;
			maxY = vertex.Y > maxY ? vertex.Y : maxY;
			minY = vertex.Y < minY ? vertex.Y : minY;
		}

		CenterX = (maxX + minX) / 2;
		CenterY = (maxY + minY) / 2;
		Width = maxX - minX;
		Height = maxY - minY;
	}


	bool NavMeshRegion::IsPointInside(float inX, float inY)
	{
		return (inX > CenterX - Width / 2) && (inX < CenterX + Width / 2)
			&& (inY > CenterY - Height / 2) && (inY < CenterY + Height / 2);
	}


	void NavMeshRegion::Split(size_t inNumParts, std::vector<NavMeshRegion>& outParts)
	{
		if (inNumParts == 1)
		{
			outParts.push_back(*this);
			return;
		}

		NavMeshRegion part1, part2;
		float share = static_cast<float>(inNumParts / 2) / inNumParts;
		if (Width > Height)
		{
			part1.CenterY = part2.CenterY = CenterY;
			part1.Height = part2.Height = Height;

			part1.Width = share * Width;
			part2.Width = Width - part1.Width;

			part1.CenterX = CenterX - part1.Width / 2;
			part2.CenterX = CenterX + part2.Width / 2;
		}
		else
		{
			part1.CenterX = part2.CenterX = CenterX;
			part1.Width = part2.Width = Width;

			part1.Height = share * Height;
			part2.Height = Height - part1.Height;

			part1.CenterY = CenterY - part1.Height / 2;
			part2.CenterY = CenterY + part2.Height / 2;
		}

		part1.Split(inNumParts / 2, outParts);
		part2.Split(inNumParts - inNumParts / 2, outParts);
	}


	std::vector<NavMeshRegion> NavMeshRegion::Split(size_t inNumParts)
	{
		std::vector<NavMeshRegion> navMeshRegionsBuffer;
		Split(inNumParts, navMeshRegionsBuffer);
		return navMeshRegionsBuffer;
	}


	bool NavMeshRegion::IsValid()
	{
		return (Width > 0) && (Height > 0);
	}
}
