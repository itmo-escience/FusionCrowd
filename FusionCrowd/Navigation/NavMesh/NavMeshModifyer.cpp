#include "NavMeshModifyer.h"
#include "NavMeshLocalizer.h"
#include <algorithm>

using namespace DirectX::SimpleMath;
namespace FusionCrowd {
	NavMeshModifyer::NavMeshModifyer(NavMesh& navmesh, std::shared_ptr<NavMeshLocalizer> localizer) : _localizer(localizer), _navmesh(navmesh)
	{
	}

	NavMeshModifyer::~NavMeshModifyer()
	{
	}

	/*
	TODO:
	nodes with double modification
	*/
	float NavMeshModifyer::SplitPolyByNodes(FCArray<NavMeshVetrex> & polygon) {
		float res = 0;
		_global_polygon = std::vector<Vector2>(polygon.size());
		_modifications = std::vector<NodeModificator*>();
		std::vector<unsigned int> nodes_ids = std::vector<unsigned int>(polygon.size());
		if (!IsClockwise(polygon)) {
			for (int i = 0; i < polygon.size(); i++) {
				Vector2 v = Vector2(polygon[i].X, polygon[i].Y);
				_global_polygon[i] = v;
			}
		}
		else {
			for (int i = 0; i < polygon.size(); i++) {
				Vector2 v = Vector2(polygon[i].X, polygon[i].Y);
				_global_polygon[_global_polygon.size() - i - 1] = v;
			}
		}

		for (int i = 0; i < polygon.size(); i++) {
			nodes_ids[i] = _localizer->getNodeId(_global_polygon[i]);
		}

		int start_pos = polygon.size() - 1;
		bool one_node = true;
		if (nodes_ids[0] == nodes_ids[start_pos]) {
			while (nodes_ids[0] == nodes_ids[start_pos - 1]) {
				if (nodes_ids[start_pos] == -1) one_node = false;
				start_pos--;
				if (start_pos == 0) {
					break;
				}
			}
		}
		else {
			start_pos = 0;
			one_node = false;
		}
		if (start_pos != 0) one_node = false;
		if (one_node) {
			NodeModificator* modificator = new NodeModificator();
			modificator->modification_type = CUT_POLY;
			modificator->node = GetNodeById(nodes_ids[0]);
			modificator->polygon_to_cut = _global_polygon;
			modificator->polygon_vertex_ids = std::vector<unsigned int>(_global_polygon.size());
			for (int i = 0; i < _global_polygon.size(); i++) {
				modificator->polygon_vertex_ids[i] = i + _navmesh.vCount;
			}
			_modifications.push_back(modificator);
			return 111111;
		}
		//if poly not on one node
		int i = start_pos;
		do {
			unsigned int prev_point_id = (i - 1 + _global_polygon.size()) % _global_polygon.size();
			Vector2  prev_point = _global_polygon[prev_point_id];
			Vector2  cur_point = _global_polygon[i];
			float minx = std::min(prev_point.x, cur_point.x);
			float maxx = std::max(prev_point.x, cur_point.x);
			float miny = std::min(prev_point.y, cur_point.y);
			float maxy = std::max(prev_point.y, cur_point.y);
			//check for line i-1 - i crossing whole nodes
			auto crossing_nodes_ids = _localizer->findNodesCrossingBB(BoundingBox(minx, miny, maxx, maxy));
			for (int i = 0; i < _navmesh.nCount; i++) {
				if (std::find(crossing_nodes_ids.begin(),
					crossing_nodes_ids.end(),
					_navmesh.nodes[i]._id) != crossing_nodes_ids.end()) {
					auto cross_points = FindPolyAndSegmentCrosspoints(prev_point, cur_point, &_navmesh.nodes[i]._poly);
					if (cross_points.size() == 2) {
						//create split modificator
						NodeModificator* modificator = new NodeModificator();
						modificator->modification_type = SPLIT;
						modificator->node = &_navmesh.nodes[i];
						modificator->polygon_to_cut = std::vector<Vector2>();
						modificator->polygon_to_cut.push_back(cross_points[0]);
						modificator->polygon_to_cut.push_back(cross_points[1]);
						modificator->polygon_vertex_ids = std::vector<unsigned int>();
						modificator->polygon_vertex_ids.push_back(AddVertex(cross_points[0]));
						modificator->polygon_vertex_ids.push_back(AddVertex(cross_points[1]));
						_modifications.push_back(modificator);
					}
				}
			}
			//skip vertexes not on navmesh
			if (nodes_ids[i] == -1) {
					i = (i + 1) % _global_polygon.size();
					continue;
			}
			//add curve modificator
			NodeModificator* modificator = new NodeModificator();
			modificator->modification_type = CUT_CURVE;
			modificator->node = GetNodeById(nodes_ids[i]);
			modificator->polygon_to_cut = std::vector<Vector2>();
			modificator->polygon_vertex_ids = std::vector<unsigned int>();
			Vector2 prev_cross_point = FindPolyAndSegmentCrosspoints(
				_global_polygon[prev_point_id],
				_global_polygon[i],
				&GetNodeById(nodes_ids[i])->_poly)[0];
			modificator->polygon_vertex_ids.push_back(AddVertex(prev_cross_point));
			modificator->polygon_to_cut.push_back(prev_cross_point);
			int previ = i;
			do {
				previ = i;
				modificator->polygon_vertex_ids.push_back(_navmesh.vCount + i);
				modificator->polygon_to_cut.push_back(_global_polygon[i]);
				i = (i + 1) % _global_polygon.size();
			} while (nodes_ids[i] == nodes_ids[previ]);

			Vector2 post_cross_point = FindPolyAndSegmentCrosspoints(
				_global_polygon[previ],
				_global_polygon[i],
				&GetNodeById(nodes_ids[previ])->_poly)[0];
			modificator->polygon_vertex_ids.push_back(AddVertex(post_cross_point));
			modificator->polygon_to_cut.push_back(post_cross_point);
			_modifications.push_back(modificator);
		} while (i != start_pos);
		return 	res;
	}

	NavMeshNode* NavMeshModifyer::GetNodeById(unsigned int id) {
		for (int i = 0; i < _navmesh.nCount; i++) {
			if (_navmesh.nodes[i]._id == id) return &_navmesh.nodes[i];
		}
		return &_navmesh.nodes[0];
	}

	float NavMeshModifyer::CutPolygonFromMesh(FCArray<NavMeshVetrex> & polygon) {
		float res = -8888;
		_addedvertices = std::vector<Vector2>();
		_nodes_ids_to_delete = std::vector<unsigned int>();
		_addednodes = std::vector<NavMeshNode*>();
		_addededges = std::vector<NavMeshEdge*>();
		_addedobstacles = std::vector<NavMeshObstacle*>();
		SplitPolyByNodes(polygon);
		for (auto mod : _modifications) {
			Initialize(mod);
			if (mod->modification_type == CUT_CURVE) {
				FillAddedVertices();
				CutCurveFromCurrentNode();
			}
			if (mod->modification_type == CUT_POLY) {
				FillAddedVertices();
				res = CutPolyFromCurrentNode();
			}
			if (mod->modification_type == SPLIT) {
				SplitNode();
			}
			_nodes_ids_to_delete.push_back(mod->node->getID());
		}
		Finalize();
		return _navmesh.obstacles.size();
	}

	int NavMeshModifyer::SplitNode() {
		auto v0 = _local_polygon[0];
		auto v1 = _local_polygon[1];
		unsigned int v0id = _local_polygon_vertex_ids[0];
		unsigned int v1id = _local_polygon_vertex_ids[1];

		NavMeshNode* node0 = new NavMeshNode(), *node1 = new NavMeshNode();
		int n0size = 2, n1size = 2;
		for (int i = 0; i < _current_node_poly->vertCount; i++) {
			auto vertex = _navmesh.vertices[_current_node_poly->vertIDs[i]];
			if (IsPointUnderLine(v0, v1, vertex))
				n0size++;
			else
				n1size++;
		}
		node0->setID(_navmesh.nCount + _addednodes.size());
		node1->setID(_navmesh.nCount + _addednodes.size() + 1);
		node0->_poly.vertCount = n0size;
		node0->_poly.vertIDs = new unsigned int[n0size];
		node1->_poly.vertCount = n1size;
		node1->_poly.vertIDs = new unsigned int[n1size];

		int added0 = 0, added1 = 0;
		for (int i = 0; i < _current_node_poly->vertCount; i++) {
			auto vertex = _navmesh.vertices[_current_node_poly->vertIDs[i]];
			if (IsPointUnderLine(v0, v1, vertex)) {
				node0->_poly.vertIDs[added0] = _current_node_poly->vertIDs[i];
				added0++;
			}
			else {
				node1->_poly.vertIDs[added1] = _current_node_poly->vertIDs[i];
				added1++;
			}
		}
		bool addingque = IsSegmentsIntersects(
			_navmesh.vertices[node0->_poly.vertIDs[0]],v0,
			_navmesh.vertices[node0->_poly.vertIDs[1]],v1);
		if (addingque) {
			node1->_poly.vertIDs[added1] = v1id;
			node1->_poly.vertIDs[added1 + 1] = v0id;
			node0->_poly.vertIDs[added0] = v0id;
			node0->_poly.vertIDs[added0 + 1] = v1id;
		} else {
			node1->_poly.vertIDs[added1] = v0id;
			node1->_poly.vertIDs[added1 + 1] = v1id;
			node0->_poly.vertIDs[added0] = v1id;
			node0->_poly.vertIDs[added0 + 1] = v0id;
		}
		//TODO edge and obst IDs
		//copy obstacles
#pragma region copy_obstacles
		std::vector<NavMeshObstacle*> node0_obst = std::vector<NavMeshObstacle*>(),
			node1_obst = std::vector<NavMeshObstacle*>();
		for (int i = 0; i < _current_node->_obstCount; i++) {
			auto obst = _current_node->_obstacles[i];
			Vector2 ov0 = obst->_point;
			Vector2 ov1 = ov0 + (obst->_length*(obst->_unitDir));
			bool ov0side = IsPointUnderLine(v0, v1, ov0);
			bool ov1side = IsPointUnderLine(v0, v1, ov1);
			if (ov0side == ov1side) {
				NavMeshObstacle* nobst = new NavMeshObstacle();
				nobst->_point = obst->_point;
				nobst->_unitDir = obst->_unitDir;
				nobst->_length = obst->_length;
				if (ov0side) {
					nobst->setNode(node0);
					node0_obst.push_back(nobst);
				}
				else {
					nobst->setNode(node1);
					node1_obst.push_back(nobst);
				}
			}
			else {
				Vector2 divpoint;
				if (ov0.x == ov1.x) {
					if (v0.x == ov0.x) divpoint = v0;
					else divpoint = v1;
				}
				else {
					float k = (ov0.y - ov1.y) / (ov0.x - ov1.x);
					float c = ov0.y - k * ov0.x;
					if (v0.y == k * v0.x + c) divpoint = v0;
					else divpoint = v1;
				}
				NavMeshObstacle* nobst0 = new NavMeshObstacle();
				nobst0->_point = ov0;
				nobst0->_unitDir = obst->_unitDir;
				nobst0->_length = (divpoint - ov0).Length();
				NavMeshObstacle* nobst1 = new NavMeshObstacle();
				nobst1->_point = divpoint;
				nobst1->_unitDir = obst->_unitDir;
				nobst1->_length = (ov1 - divpoint).Length();
				if (ov0side) {
					nobst0->setNode(node0);
					node0_obst.push_back(nobst0);
					nobst1->setNode(node1);
					node1_obst.push_back(nobst1);
				}
				else {
					nobst0->setNode(node1);
					node1_obst.push_back(nobst0);
					nobst1->setNode(node0);
					node0_obst.push_back(nobst1);
				}
			}
		}
		NavMeshObstacle* nobst0 = new NavMeshObstacle();
		nobst0->_point = v0;
		nobst0->_unitDir = (v1 - v0) / (v1 - v0).Length();
		nobst0->_length = (v1 - v0).Length();
		nobst0->setNode(node0);
		node0_obst.push_back(nobst0);
		NavMeshObstacle* nobst1 = new NavMeshObstacle();
		nobst1->_point = v0;
		nobst1->_unitDir = (v1 - v0) / (v1 - v0).Length();
		nobst1->_length = (v1 - v0).Length();
		nobst1->setNode(node1);
		node1_obst.push_back(nobst1);

		node0->_obstacles = new NavMeshObstacle*[node0_obst.size()];
		node0->_obstCount = node0_obst.size();
		for (int i = 0; i < node0_obst.size(); i++) {
			node0->_obstacles[i] = node0_obst[i];
			_addedobstacles.push_back(node0_obst[i]);
		}
		node1->_obstacles = new NavMeshObstacle*[node1_obst.size()];
		node1->_obstCount = node0_obst.size();
		for (int i = 0; i < node0_obst.size(); i++) {
			node1->_obstacles[i] = node1_obst[i];
			_addedobstacles.push_back(node1_obst[i]);
		}
#pragma endregion
		//TODO second node edge modification
		//copy edges
#pragma region copy_edges
		std::vector<NavMeshEdge*> node0_edges = std::vector<NavMeshEdge*>(),
			node1_edges = std::vector<NavMeshEdge*>();
		for (int i = 0; i < _current_node->_edgeCount; i++) {
			auto edge = _current_node->_edges[i];
			NavMeshNode* neighbour_node =
				edge->getFirstNode() == _current_node ? edge->getSecondNode() : edge->getFirstNode();
			Vector2 ov0 = edge->getP0();
			Vector2 ov1 = edge->getP1();
			bool ov0side = IsPointUnderLine(v0, v1, ov0);
			bool ov1side = IsPointUnderLine(v0, v1, ov1);
			if (ov0side == ov1side) {
				NavMeshEdge* nedge = new NavMeshEdge();
				nedge->setPoint(edge->getP0());
				nedge->setDirection(edge->getDirection());
				//TODO distance, width
				//nobst->_length = obst->_length;
				if (ov0side) {
					nedge->setNodes(node0, neighbour_node);
					node0_edges.push_back(nedge);
				}
				else {
					nedge->setNodes(node1, neighbour_node);
					node1_edges.push_back(nedge);
				}
			}
			else {
				Vector2 divpoint;
				if (ov0.x == ov1.x) {
					if (v0.x == ov0.x) divpoint = v0;
					else divpoint = v1;
				}
				else {
					float k = (ov0.y - ov1.y) / (ov0.x - ov1.x);
					float c = ov0.y - k * ov0.x;
					if (v0.y == k * v0.x + c) divpoint = v0;
					else divpoint = v1;
				}
				NavMeshEdge* nedge0 = new NavMeshEdge();
				nedge0->setPoint(ov0);
				nedge0->setDirection(edge->getDirection());
				//TODO distance width
				//nobst0->_length = (divpoint - ov0).Length();
				NavMeshEdge* nedge1 = new NavMeshEdge();
				nedge1->setPoint(divpoint);
				nedge1->setDirection(edge->getDirection());
				//TODO distance width
				//nobst0->_length = (divpoint - ov0).Length();
				if (ov0side) {
					nedge0->setNodes(node0, neighbour_node);
					node0_edges.push_back(nedge0);
					nedge1->setNodes(node1, neighbour_node);
					node1_edges.push_back(nedge1);
				}
				else {
					nedge0->setNodes(node1, neighbour_node);
					node1_edges.push_back(nedge0);
					nedge1->setNodes(node0, neighbour_node);
					node0_edges.push_back(nedge1);
				}
			}
		}
		node0->_edges = new NavMeshEdge*[node0_edges.size()];
		node0->_edgeCount = node0_edges.size();
		for (int i = 0; i < node0_edges.size(); i++) {
			node0->_edges[i] = node0_edges[i];
			_addededges.push_back(node0_edges[i]);
		}
		node1->_edges = new NavMeshEdge*[node1_edges.size()];
		node1->_edgeCount = node1_edges.size();
		for (int i = 0; i < node1_edges.size(); i++) {
			node1->_edges[i] = node1_edges[i];
			_addededges.push_back(node1_edges[i]);
		}
#pragma endregion
		_addednodes.push_back(node0);
		_addednodes.push_back(node1);
		return 0;
	}

	/*Adds all created nodes and vertexes*/
	int NavMeshModifyer::Finalize() {
		//add created vertices
		Vector2* updvertices = new Vector2[_navmesh.vCount + _global_polygon.size() + _addedvertices.size()];
		for (int i = 0; i < _navmesh.vCount; i++) {
			updvertices[i] = _navmesh.vertices[i];
		}
		delete[] _navmesh.vertices;
		for (int i = 0; i < _global_polygon.size(); i++) {
			updvertices[_navmesh.vCount + i] = _global_polygon[i];
		}
		for (int i = 0; i < _addedvertices.size(); i++) {
			updvertices[_navmesh.vCount + _global_polygon.size() + i] = _addedvertices[i];
		}
		_navmesh.vCount = _navmesh.vCount + _global_polygon.size() + _addedvertices.size();
		_navmesh.vertices = updvertices;

		for (auto n : _addednodes) {
			n->setVertices(_navmesh.vertices);
		}

		//add nodes
		//calculate nodes centers, fill poly vertices
		for (auto n : _addednodes) {
			Vector2 center = Vector2(0, 0);
			for (int i = 0; i < n->_poly.vertCount; i++) {
				center += _navmesh.vertices[n->_poly.vertIDs[i]];
			}
			n->_poly.vertices = _navmesh.vertices;
			center /= (float) n->_poly.vertCount;
			n->setCenter(center);
		}
		NavMeshNode* tmpNodes = new NavMeshNode[_navmesh.nCount + _addednodes.size() - _nodes_ids_to_delete.size()];
		int deleted = 0;
		for (size_t i = 0; i < _navmesh.nCount; i++)
		{
			if (std::find(
				_nodes_ids_to_delete.begin(),
				_nodes_ids_to_delete.end(),
				_navmesh.nodes[i]._id) == _nodes_ids_to_delete.end()) {
				tmpNodes[i - deleted] = _navmesh.nodes[i];
			}
			else {
				deleted++;
			}
		}
		delete[] _navmesh.nodes;
		for (int i = 0; i < _addednodes.size(); i++) {
			tmpNodes[_navmesh.nCount + i - _nodes_ids_to_delete.size()] = *_addednodes[i];
		}
		_navmesh.nCount += _addednodes.size() - _nodes_ids_to_delete.size();
		_navmesh.nodes = tmpNodes;

		//add and delete obstacles
		std::vector<NavMeshObstacle> tmp_obstacles = std::vector<NavMeshObstacle>();
		for (auto o : _navmesh.obstacles) {
			if (std::find(
				_nodes_ids_to_delete.begin(),
				_nodes_ids_to_delete.end(),
				o.getNode()->_id) == _nodes_ids_to_delete.end()) {
				tmp_obstacles.push_back(o);
			}

		}
		for (int i = 0; i < _addedobstacles.size();i++) {
			tmp_obstacles.push_back(*_addedobstacles[i]);
		}
		_navmesh.obstCount = tmp_obstacles.size();
		_navmesh.obstacles = tmp_obstacles;
		//add and delete edges
		std::vector<NavMeshEdge> vtmp_edges = std::vector<NavMeshEdge>();
		for (int i = 0; i < _navmesh.eCount; i++) {
			if (std::find(
				_nodes_ids_to_delete.begin(),
				_nodes_ids_to_delete.end(),
				_navmesh.edges[i].getFirstNode()->_id)
				== _nodes_ids_to_delete.end() &&
				std::find(
				_nodes_ids_to_delete.begin(),
				_nodes_ids_to_delete.end(),
				_navmesh.edges[i].getSecondNode()->_id)
				== _nodes_ids_to_delete.end()) {
					vtmp_edges.push_back(_navmesh.edges[i]);
			}
		}
		for (int i = 0; i < _addededges.size(); i++) {
			vtmp_edges.push_back(*_addededges[i]);
		}
		_navmesh.edges = new NavMeshEdge[vtmp_edges.size()];
		for (int i = 0; i < vtmp_edges.size(); i++) {
			_navmesh.edges[i] = vtmp_edges[i];
		}
		_navmesh.eCount = vtmp_edges.size();
		return 0;
	}

	/*Creates arrays*/
	int NavMeshModifyer::Initialize(NodeModificator * modificator) {
		_current_node_poly = &modificator->node->_poly;
		_current_node = modificator->node;
		_local_polygon = modificator->polygon_to_cut;
		_local_polygon_vertex_ids = modificator->polygon_vertex_ids;
		return 0;
	}

	/*Cut whole poligon from onde node*/
	int NavMeshModifyer::CutPolyFromCurrentNode() {
		for (int j = 0; j < _local_polygon.size(); j++) {
			NavMeshNode* updnode = new NavMeshNode();
			updnode->setID(_navmesh.nCount + _addednodes.size());
			int vert_count = 0;
			Vector2 j0vert = _local_polygon[j];
			Vector2 j1vert = _local_polygon[(j + 1) % _local_polygon.size()];
			Vector2 j2vert = _local_polygon[(j + 2) % _local_polygon.size()];
			bool node_side0 = !IsPointUnderLine(j0vert, j1vert, j2vert);
			bool node_side1 = IsPointUnderLine(j1vert, j2vert, j0vert);
			for (int i = 0; i < _current_node_poly->vertCount; i++) {
				auto vertex = _navmesh.vertices[_current_node_poly->vertIDs[i]];
				if (IsPointUnderLine(j0vert, j1vert, vertex, node_side0, true) &&
					IsPointUnderLine(j1vert, j2vert, vertex, node_side1)) {
					vert_count++;
				}
			}
			vert_count += 4;
			updnode->_poly.vertCount = vert_count;
			updnode->_poly.vertIDs = new unsigned int[vert_count];

			int addedids = 0;
			for (int i = 0; i < _current_node_poly->vertCount; i++) {
				auto vertex = _navmesh.vertices[_current_node_poly->vertIDs[i]];
				if (IsPointUnderLine(j0vert, j1vert, vertex, node_side0, true) &&
					IsPointUnderLine(j1vert, j2vert, vertex, node_side1)) {
					updnode->_poly.vertIDs[addedids] = _current_node_poly->vertIDs[i];
					addedids++;
				}
				if (_current_node_poly->vertIDs[i] == crosspoints_prev_vertex_ids[j]) {
					updnode->_poly.vertIDs[addedids] = crosspoints_ids[j];//j0j1 crosspoint
					addedids++;
					updnode->_poly.vertIDs[addedids] = _local_polygon_vertex_ids[(j + 1) % _local_polygon.size()]; //polygon node j1
					addedids++;
					updnode->_poly.vertIDs[addedids] = _local_polygon_vertex_ids[(j + 2) % _local_polygon.size()]; //polygon node j2
					addedids++;
					updnode->_poly.vertIDs[addedids] = crosspoints_ids[(j + 1) % crosspoints_ids.size()]; //j1j2 crosspoint
					addedids++;
				}
			}
			//TODO: obstacles and edges
			CopyVortexObstacles(updnode, j, j0vert, j1vert, j2vert, node_side0, node_side1);
#pragma region copy_obstacles/*
			std::vector<NavMeshObstacle*> tmp_obst = std::vector<NavMeshObstacle*>();
			NavMeshObstacle *obst = new NavMeshObstacle();
			obst->setNode(updnode);
			Vector2 p0 = _local_polygon[(j + 1) % _local_polygon.size()];
			Vector2 p1 = _local_polygon[(j + 2) % _local_polygon.size()];
			obst->_point = p0;
			obst->_unitDir = (p1 - p0) / (p1 - p0).Length();
			obst->_length = (p1 - p0).Length();
			tmp_obst.push_back(obst);

			for (int i = 0; i < _current_node->_obstCount; i++) {
				NavMeshObstacle *obst = _current_node->_obstacles[i];
				Vector2 p0 = obst->getP0();
				Vector2 p1 = obst->getP1();
				bool p0side = IsPointUnderLine(j0vert, j1vert, p0, node_side0, true) &&
					IsPointUnderLine(j1vert, j2vert, p0, node_side1);
				bool p1side = IsPointUnderLine(j0vert, j1vert, p1, node_side0, true) &&
					IsPointUnderLine(j1vert, j2vert, p1, node_side1);
				if (p0side && p1side) {
					obst->setNode(updnode);
					NavMeshObstacle *nobst = new NavMeshObstacle();
					nobst->setNode(updnode);
					nobst->_point = p0;
					nobst->_unitDir = (p1 - p0) / (p1 - p0).Length();
					nobst->_length = (p1 - p0).Length();
					tmp_obst.push_back(nobst);
				}
				else {
					Vector2 v0 = crosspoints[j];
					Vector2 v1 = crosspoints[(j + 1) % crosspoints.size()];
					if (p0side || p1side) {
						Vector2 divpoint;
						if (p0.x == p1.x) {
							if (v0.x == p0.x) divpoint = v0;
							else divpoint = v1;
						}
						else {
							float k = (p0.y - p1.y) / (p0.x - p1.x);
							float c = p0.y - k * p0.x;
							if (v0.y == k * v0.x + c) divpoint = v0;
							else divpoint = v1;
						}
						NavMeshObstacle *nobst = new NavMeshObstacle();
						nobst->setNode(updnode);
						nobst->_point = divpoint;
						tmp_obst.push_back(nobst);
						if (p0side) {
							nobst->_unitDir = (p0 - divpoint) / (p0 - divpoint).Length();
							nobst->_length = (p0 - divpoint).Length();
						}
						else {
							nobst->_unitDir = (p1 - divpoint) / (p1 - divpoint).Length();
							nobst->_length = (p1 - divpoint).Length();
						}
					}
					else {
						//is both crosspoints on obst?
						bool create_obst = true;
						if (p0.x == p1.x) {
							if (v0.x != p0.x) create_obst = false;
						}
						else {
							float k = (p0.y - p1.y) / (p0.x - p1.x);
							float c = p0.y - k * p0.x;
							if (v0.y != k * v0.x + c) create_obst = false;
						}
						if (p0.x == p1.x) {
							if (v1.x != p0.x) create_obst = false;
						}
						else {
							float k = (p0.y - p1.y) / (p0.x - p1.x);
							float c = p0.y - k * p0.x;
							if (v1.y != k * v1.x + c) create_obst = false;
						}
						if (create_obst) {
							NavMeshObstacle *nobst = new NavMeshObstacle();
							nobst->setNode(updnode);
							nobst->_point = v0;
							nobst->_unitDir = (v1 - v0) / (v1 - v0).Length();
							nobst->_length = (v1 - v0).Length();
							tmp_obst.push_back(nobst);
						}
					}
				}
			}

			updnode->_obstacles = new NavMeshObstacle*[tmp_obst.size()];
			updnode->_obstCount = tmp_obst.size();
			for (int i = 0; i < tmp_obst.size(); i++) {
				updnode->_obstacles[i] = tmp_obst[i];
				_addedobstacles.push_back(tmp_obst[i]);
			}*/
#pragma endregion
			_addednodes.push_back(updnode);
		}
		return _addedobstacles.size();
	}

	int NavMeshModifyer::CutCurveFromCurrentNode() {
		for (int j = 0; j < _local_polygon.size() -2; j++) {
			NavMeshNode* updnode = new NavMeshNode();
			updnode->setID(_navmesh.nCount + _addednodes.size());
			int vert_count = 0;
			Vector2 j0vert = _local_polygon[j];
			Vector2 j1vert = _local_polygon[(j + 1) % _local_polygon.size()];
			Vector2 j2vert = _local_polygon[(j + 2) % _local_polygon.size()];
			bool node_side0 = !IsPointUnderLine(j0vert, j1vert, j2vert);
			bool node_side1 = IsPointUnderLine(j1vert, j2vert, j0vert);
			for (int i = 0; i < _current_node_poly->vertCount; i++) {
				auto vertex = _navmesh.vertices[_current_node_poly->vertIDs[i]];
				if (IsPointUnderLine(j0vert, j1vert, vertex, node_side0, true) &&
					IsPointUnderLine(j1vert, j2vert, vertex, node_side1)) {
					vert_count++;
				}
			}
			vert_count += 4;
			updnode->_poly.vertCount = vert_count;
			updnode->_poly.vertIDs = new unsigned int[vert_count];

			int addedids = 0;

			for (int i = 0; i < _current_node_poly->vertCount; i++) {
				auto vertex = _navmesh.vertices[_current_node_poly->vertIDs[i]];
				bool vertex_added = false;
				if (IsPointUnderLine(j0vert, j1vert, vertex, node_side0, true) &&
					IsPointUnderLine(j1vert, j2vert, vertex, node_side1)) {
					updnode->_poly.vertIDs[addedids] = _current_node_poly->vertIDs[i];
					addedids++;
					vertex_added = true;
				}
				if (_current_node_poly->vertIDs[i] == crosspoints_prev_vertex_ids[j]) {
					updnode->_poly.vertIDs[addedids] = crosspoints_ids[j];//j0j1 crosspoint
					addedids++;
					updnode->_poly.vertIDs[addedids] = _local_polygon_vertex_ids[(j + 1) % _local_polygon_vertex_ids.size()]; //polygon node j1
					addedids++;
					updnode->_poly.vertIDs[addedids] = _local_polygon_vertex_ids[(j + 2) % _local_polygon_vertex_ids.size()]; //polygon node j2
					addedids++;
					updnode->_poly.vertIDs[addedids] = crosspoints_ids[(j+1)%crosspoints_ids.size()]; //j1j2 crosspoint
					addedids++;
				}
			}

			CopyVortexObstacles(updnode, j, j0vert, j1vert, j2vert, node_side0, node_side1);
			//TODO: obstacles and edges
			_addednodes.push_back(updnode);
		}

		//add node splited by v0 v1 line

		NavMeshNode* updnode = new NavMeshNode();
		updnode->setID(_navmesh.nCount + _local_polygon.size() - 2);
		int vert_count = 0;
		Vector2 j0vert = _local_polygon[0];
		Vector2 j1vert = _local_polygon[1];
		Vector2 j2vert = _local_polygon[2];
		bool node_side0 = IsPointUnderLine(j0vert, j1vert, j2vert);
		//for correct vertex adding
		std::vector<Vector2> tmpvertex = std::vector<Vector2>();
		int post_index = 0;
		for (int i = 0; i < _current_node_poly->vertCount; i++) {
			auto vertex = _navmesh.vertices[_current_node_poly->vertIDs[i]];
			if (IsPointUnderLine(j0vert, j1vert, vertex, node_side0, true)) {
				vert_count++;
				tmpvertex.push_back(vertex);
			}
			if (_current_node_poly->vertIDs[i] == crosspoints_prev_vertex_ids[_local_polygon.size() - 1]) {
				post_index = vert_count;
			}
		}
		vert_count += 3;
		updnode->_poly.vertCount = vert_count;
		updnode->_poly.vertIDs = new unsigned int[vert_count];

		Vector2 prev = tmpvertex[(post_index - 1 + tmpvertex.size()) % tmpvertex.size()];
		Vector2 post = tmpvertex[post_index % tmpvertex.size()];
		bool que = !IsSegmentsIntersects(prev, _local_polygon[1], _local_polygon[0], post);

		int addedids = 0;
		int res =post.x==6 && post.y == 1?7 :8;
		for (int i = 0; i < _current_node_poly->vertCount; i++) {
			auto vertex = _navmesh.vertices[_current_node_poly->vertIDs[i]];
			bool added = false;
			if (IsPointUnderLine(j0vert, j1vert, vertex, node_side0, true) ) {
				updnode->_poly.vertIDs[addedids] = _current_node_poly->vertIDs[i];
				added = true;
				addedids++;
			}
			if (_current_node_poly->vertIDs[i] == crosspoints_prev_vertex_ids[_local_polygon.size() - 1]) {
				if (!added) que = !que;
				if (que) {
					updnode->_poly.vertIDs[addedids] = crosspoints_ids[0]; //j0j1 crosspoint
					addedids++;
					updnode->_poly.vertIDs[addedids] = _local_polygon_vertex_ids[1]; //polygon node j1
					addedids++;
					updnode->_poly.vertIDs[addedids] = _local_polygon_vertex_ids[0]; //polygon node j0
					addedids++;
				}
				else {
					updnode->_poly.vertIDs[addedids] = _local_polygon_vertex_ids[0]; //polygon node j0
					addedids++;
					updnode->_poly.vertIDs[addedids] = _local_polygon_vertex_ids[1]; //polygon node j1
					addedids++;
					updnode->_poly.vertIDs[addedids] = crosspoints_ids[0]; //j0j1 crosspoint
					addedids++;

				}
			}
		}
		CopyVortexObstacles(updnode, -1, j0vert, j1vert, j2vert, node_side0, false, true);
		_addednodes.push_back(updnode);

		return res;
	}

	/*Returns cross point with current_poly in dirrection v0->v1*/
	Vector2 NavMeshModifyer::FindVortexCrossPoint(Vector2 v0, Vector2 v1, int& out_prev_cross_id) {
		//calculate line coef
		float k0 = 0.0;
		float c0 = 0.0;
		bool vertical = false;
		if (v0.x == v1.x) {
			vertical = true;
		}
		else {
			k0 = (v0.y - v1.y) / (v0.x - v1.x);
			c0 = v0.y - k0 * v0.x;
		}
		for (int i = 0; i < _current_node_poly->vertCount; i++) {
			Vector2 pnode0 = Vector2();
			Vector2 pnode1 = Vector2();
			if (i < _current_node_poly->vertCount - 1) {
				pnode0.x = _current_node_poly->vertices[_current_node_poly->vertIDs[i]].x;
				pnode0.y = _current_node_poly->vertices[_current_node_poly->vertIDs[i]].y;
				pnode1.x = _current_node_poly->vertices[_current_node_poly->vertIDs[i + 1]].x;
				pnode1.y = _current_node_poly->vertices[_current_node_poly->vertIDs[i + 1]].y;
			}
			else {
				pnode0.x = _current_node_poly->vertices[_current_node_poly->vertIDs[i]].x;
				pnode0.y = _current_node_poly->vertices[_current_node_poly->vertIDs[i]].y;
				pnode1.x = _current_node_poly->vertices[_current_node_poly->vertIDs[0]].x;
				pnode1.y = _current_node_poly->vertices[_current_node_poly->vertIDs[0]].y;
			}

			//line cross point claculation
			float xcross = 0.0;
			float ycross = 0.0;
			if (vertical) {
				if (pnode0.x == pnode1.x) continue;
				float k1 = (pnode0.y - pnode1.y) / (pnode0.x - pnode1.x);
				float c1 = pnode0.y - k1 * pnode0.x;
				xcross = v0.x;
				ycross = k1 * xcross + c1;
			} else {
				if (pnode0.x == pnode1.x) {
					xcross = pnode0.x;
					ycross = k0 * xcross + c0;
				}
				else {
					float k1 = (pnode0.y - pnode1.y) / (pnode0.x - pnode1.x);
					if (k1 == k0) continue;
					float c1 = pnode0.y - k1 * pnode0.x;

					xcross = (c1 - c0) / (k0 - k1);
					ycross = k1 * xcross + c1;
				}
			}

			//is cross point on segment?
			if (
				((xcross>=pnode0.x && xcross<=pnode1.x) ||
				(xcross<=pnode0.x && xcross>=pnode1.x)) &&
				((ycross >= pnode0.y && ycross <= pnode1.y) ||
				(ycross <= pnode0.y && ycross >= pnode1.y))
				) {
				//is cross point in v0->v1 direction?
				if (
					(v1.x > v0.x && xcross >= v1.x) ||
					(v1.x < v0.x && xcross <= v1.x) ||
					(v1.x == v0.x && v1.y> v0.y && ycross >= v1.y) ||
					(v1.x == v0.x && v1.y < v0.y && ycross <= v1.y)
					){
					out_prev_cross_id = _current_node_poly->vertIDs[i];
					return Vector2(xcross, ycross);
				}
				else continue;
			}
		}
		return Vector2(0, 0);
	}

	/*Adds crosspoints for polygon cut*/
	void NavMeshModifyer::FillAddedVertices() {
		crosspoints_prev_vertex_ids = std::vector<unsigned int>(_local_polygon.size());
		crosspoints_ids = std::vector<unsigned int>(_local_polygon.size());
		crosspoints = std::vector<Vector2>(_local_polygon.size());
		int prev_cross_id = 0;
		for (int i = 0; i < _local_polygon.size(); i++) {
			Vector2 crosspoint = FindVortexCrossPoint(_local_polygon[i], _local_polygon[(i + 1) % _local_polygon.size()], prev_cross_id);
			crosspoints_ids[i] = AddVertex(crosspoint);
			crosspoints[i] = crosspoint;
			crosspoints_prev_vertex_ids[i] = prev_cross_id;
		}
	}

	/*Returns is point under line v0->v1 (or point.x< line.x if line is vertical)*/
	bool NavMeshModifyer::IsPointUnderLine(Vector2 v0, Vector2 v1, Vector2 point, bool reverse, bool strict) {
		bool vertical = false;
		float k = 0.0;
		float c = 0.0;
		if (v0.x == v1.x) {
			vertical = true;
		}
		else {
			k = (v0.y - v1.y) / (v0.x - v1.x);
			c = v0.y - k * v0.x;
		}
		if (vertical) {
			if (strict)
				return reverse ? point.x > v0.x : point.x < v0.x;
			else
				return reverse ? point.x >= v0.x : point.x <= v0.x;
		}
		if (strict) {
			if (reverse)
				return point.y > k*point.x + c;
			else
				return point.y < k*point.x + c;
		} else
			if (reverse)
				return point.y >= k*point.x + c;
			else
				return point.y <= k*point.x + c;
	}

	bool NavMeshModifyer::IsClockwise(FCArray<NavMeshVetrex> & polygon) {
		float sum = 0;
		for (int i = 0; i < polygon.size(); i++) {
			NavMeshVetrex v0 = polygon[i];
			NavMeshVetrex v1 = polygon[(i+1) % polygon.size()];
			sum += (v1.X-v0.X)*(v0.Y + v1.Y);
		}

		return sum > 0;
	}

	bool NavMeshModifyer::IsTriangleClockwise(Vector2 v0, Vector2 v1, Vector2 v2){
		float sum = 0;
		sum += (v1.x - v0.x)*(v0.y + v1.y);
		sum += (v2.x - v1.x)*(v1.y + v2.y);
		sum += (v0.x - v2.x)*(v2.y + v0.y);
		return sum > 0;

	}

	//adds vertex and returns its ID
	unsigned int NavMeshModifyer::AddVertex(Vector2 v) {
		_addedvertices.push_back(v);
		return _navmesh.vCount + _global_polygon.size() + _addedvertices.size() - 1;
	}

	std::vector<Vector2> NavMeshModifyer::FindPolyAndSegmentCrosspoints(Vector2 v0, Vector2 v1, NavMeshPoly* poly) {
		std::vector<Vector2> res = std::vector<Vector2>();
		//calculate line coef
		float k0 = 0.0;
		float c0 = 0.0;
		bool vertical = false;
		if (v0.x == v1.x) {
			vertical = true;
		}
		else {
			k0 = (v0.y - v1.y) / (v0.x - v1.x);
			c0 = v0.y - k0 * v0.x;
		}
		for (int i = 0; i < poly->vertCount; i++) {
			Vector2 pnode0 = Vector2();
			Vector2 pnode1 = Vector2();
			if (i < poly->vertCount - 1) {
				pnode0.x = poly->vertices[poly->vertIDs[i]].x;
				pnode0.y = poly->vertices[poly->vertIDs[i]].y;
				pnode1.x = poly->vertices[poly->vertIDs[i + 1]].x;
				pnode1.y = poly->vertices[poly->vertIDs[i + 1]].y;
			}
			else {
				pnode0.x = poly->vertices[poly->vertIDs[i]].x;
				pnode0.y = poly->vertices[poly->vertIDs[i]].y;
				pnode1.x = poly->vertices[poly->vertIDs[0]].x;
				pnode1.y = poly->vertices[poly->vertIDs[0]].y;
			}

			//line cross point claculation
			float xcross = 0.0;
			float ycross = 0.0;
			if (vertical) {
				if (pnode0.x == pnode1.x) continue;
				float k1 = (pnode0.y - pnode1.y) / (pnode0.x - pnode1.x);
				float c1 = pnode0.y - k1 * pnode0.x;
				xcross = v0.x;
				ycross = k1 * xcross + c1;
			}
			else {
				if (pnode0.x == pnode1.x) {
					xcross = pnode0.x;
					ycross = k0 * xcross + c0;
				}
				else {
					float k1 = (pnode0.y - pnode1.y) / (pnode0.x - pnode1.x);
					if (k1 == k0) continue;
					float c1 = pnode0.y - k1 * pnode0.x;

					xcross = (c1 - c0) / (k0 - k1);
					ycross = k1 * xcross + c1;
				}
			}

			//is cross point on poly segment?
			if (
				((xcross >= pnode0.x && xcross <= pnode1.x) ||
				(xcross <= pnode0.x && xcross >= pnode1.x)) &&
					((ycross >= pnode0.y && ycross <= pnode1.y) ||
				(ycross <= pnode0.y && ycross >= pnode1.y))
				) {
				//is cross point on v0v1 segment?
				if (
					((xcross >= v0.x && xcross <= v1.x) ||
					(xcross <= v0.x && xcross >= v1.x)) &&
						((ycross >= v0.y && ycross <= v1.y) ||
					(ycross <= v0.y && ycross >= v1.y))
					) {
					res.push_back(Vector2(xcross, ycross));
				}
			}
		}
		return res;
	}

	bool NavMeshModifyer::IsSegmentsIntersects(Vector2 v00, Vector2 v01, Vector2 v10, Vector2 v11) {
		//calculate line coef
		float k0 = 0.0;
		float c0 = 0.0;
		bool vertical = false;
		if (v00.x == v01.x) {
			vertical = true;
		}
		else {
			k0 = (v00.y - v01.y) / (v00.x - v01.x);
			c0 = v00.y - k0 * v00.x;
		}
		Vector2 pnode0 = v10;
		Vector2 pnode1 = v11;
		//line cross point claculation
		float xcross = 0.0;
		float ycross = 0.0;
		if (vertical) {
			if (pnode0.x == pnode1.x) return false;
			float k1 = (pnode0.y - pnode1.y) / (pnode0.x - pnode1.x);
			float c1 = pnode0.y - k1 * pnode0.x;
			xcross = v00.x;
			ycross = k1 * xcross + c1;
		}
		else {
			if (pnode0.x == pnode1.x) {
				xcross = pnode0.x;
				ycross = k0 * xcross + c0;
			}
			else {
				float k1 = (pnode0.y - pnode1.y) / (pnode0.x - pnode1.x);
				if (k1 == k0) return false;
				float c1 = pnode0.y - k1 * pnode0.x;

				xcross = (c1 - c0) / (k0 - k1);
				ycross = k1 * xcross + c1;
			}
		}

		//is cross point on poly segment?
		if (
			((xcross >= pnode0.x && xcross <= pnode1.x) ||
			(xcross <= pnode0.x && xcross >= pnode1.x)) &&
				((ycross >= pnode0.y && ycross <= pnode1.y) ||
			(ycross <= pnode0.y && ycross >= pnode1.y))
			) {
			//is cross point on v0v1 segment?
			if (
				((xcross >= v00.x && xcross <= v01.x) ||
				(xcross <= v00.x && xcross >= v01.x)) &&
					((ycross >= v00.y && ycross <= v01.y) ||
				(ycross <= v00.y && ycross >= v01.y))
				) {
				return true;
			}
			else return false;
		}
	}


	void NavMeshModifyer::CopyVortexObstacles(NavMeshNode* updnode, int j,
		Vector2 j0vert, Vector2 j1vert, Vector2 j2vert, bool node_side0, bool node_side1, bool onestrict) {

		std::vector<NavMeshObstacle*> tmp_obst = std::vector<NavMeshObstacle*>();
		NavMeshObstacle *obst = new NavMeshObstacle();
		obst->setNode(updnode);
		Vector2 p0 = _local_polygon[(j + 1) % _local_polygon.size()];
		Vector2 p1 = _local_polygon[(j + 2) % _local_polygon.size()];
		obst->_point = p0;
		obst->_unitDir = (p1 - p0) / (p1 - p0).Length();
		obst->_length = (p1 - p0).Length();
		tmp_obst.push_back(obst);

		for (int i = 0; i < _current_node->_obstCount; i++) {
			NavMeshObstacle *obst = _current_node->_obstacles[i];
			Vector2 p0 = obst->getP0();
			Vector2 p1 = obst->getP1();
			bool p0side = IsPointUnderLine(j0vert, j1vert, p0, node_side0, true) &&
				IsPointUnderLine(j1vert, j2vert, p0, node_side1);
			bool p1side = IsPointUnderLine(j0vert, j1vert, p1, node_side0, true) &&
				IsPointUnderLine(j1vert, j2vert, p1, node_side1);
			if (onestrict) {
				p0side = IsPointUnderLine(j0vert, j1vert, p0, node_side0, true);
				p1side = IsPointUnderLine(j0vert, j1vert, p1, node_side0, true);
			}
			if (p0side && p1side) {
				obst->setNode(updnode);
				NavMeshObstacle *nobst = new NavMeshObstacle();
				nobst->setNode(updnode);
				nobst->_point = p0;
				nobst->_unitDir = (p1 - p0) / (p1 - p0).Length();
				nobst->_length = (p1 - p0).Length();
				tmp_obst.push_back(nobst);
			}
			else {
				Vector2 v0 = crosspoints[j];
				Vector2 v1 = crosspoints[(j + 1) % crosspoints.size()];
				if (onestrict) {
					v0 = j0vert;
				}
				if (p0side || p1side) {
					Vector2 divpoint;
					if (p0.x == p1.x) {
						if (v0.x == p0.x) divpoint = v0;
						else divpoint = v1;
					}
					else {
						float k = (p0.y - p1.y) / (p0.x - p1.x);
						float c = p0.y - k * p0.x;
						if (v0.y == k * v0.x + c) divpoint = v0;
						else divpoint = v1;
					}
					NavMeshObstacle *nobst = new NavMeshObstacle();
					nobst->setNode(updnode);
					nobst->_point = divpoint;
					tmp_obst.push_back(nobst);
					if (p0side) {
						nobst->_unitDir = (p0 - divpoint) / (p0 - divpoint).Length();
						nobst->_length = (p0 - divpoint).Length();
					}
					else {
						nobst->_unitDir = (p1 - divpoint) / (p1 - divpoint).Length();
						nobst->_length = (p1 - divpoint).Length();
					}
				}
				else {
					//is both crosspoints on obst?
					bool create_obst = true;
					if (p0.x == p1.x) {
						if (v0.x != p0.x) create_obst = false;
					}
					else {
						float k = (p0.y - p1.y) / (p0.x - p1.x);
						float c = p0.y - k * p0.x;
						if (v0.y != k * v0.x + c) create_obst = false;
					}
					if (p0.x == p1.x) {
						if (v1.x != p0.x) create_obst = false;
					}
					else {
						float k = (p0.y - p1.y) / (p0.x - p1.x);
						float c = p0.y - k * p0.x;
						if (v1.y != k * v1.x + c) create_obst = false;
					}
					if (create_obst) {
						NavMeshObstacle *nobst = new NavMeshObstacle();
						nobst->setNode(updnode);
						nobst->_point = v0;
						nobst->_unitDir = (v1 - v0) / (v1 - v0).Length();
						nobst->_length = (v1 - v0).Length();
						tmp_obst.push_back(nobst);
					}
				}
			}
		}

		updnode->_obstacles = new NavMeshObstacle*[tmp_obst.size()];
		updnode->_obstCount = tmp_obst.size();
		for (int i = 0; i < tmp_obst.size(); i++) {
			updnode->_obstacles[i] = tmp_obst[i];
			_addedobstacles.push_back(tmp_obst[i]);
		}
	}
}
