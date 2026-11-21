#pragma once

// KmodPathFinder is the unit pathfinder of K-Mod (version 1.46), written by karadoc.
// Unlike the pathfinder in the game exe it lives in the DLL, any number of instances can
// exist, and it keeps its search tree between requests: while the group, the flags that
// matter and the starting plot stay the same, a request for another destination carries on
// from the plots already mapped.
//
// It is used here with the stock path callbacks in CvGameCoreUtils.cpp (pathDestValid,
// pathHeuristic, pathCost, pathValid_join, pathValid_source, pathAdd), not with K-Mod's
// rewritten ones. The places where that needs something K-Mod's code did not have are
// marked "stock cost model" in KmodPathFinder.cpp. Other changes from K-Mod's code:
//  - every member is initialised;
//  - Reset() clears only the nodes that were mapped, not the whole map;
//  - whether a plot can be moved out of (pathValid_source) is asked when its node is taken
//    off the open list, as the exe's pathfinder does, not when the node is first mapped;
//  - the heuristic weight is 1, as in the exe (see SetSettings), so K-Mod's
//    InitHeuristicWeights() and MinimumStepCost() are not here.

#include <vector>
#include "FAStarNode.h"

class CvSelectionGroup;
class CvPlot;

struct CvPathSettings
{
	CvPathSettings(const CvSelectionGroup* pGroup = 0, int iFlags = 0, int iMaxPath = -1, int iHW = -1);

	CvSelectionGroup* pGroup;
	int iFlags;
	int iMaxPath;
	int iHeuristicWeight;

	// The destination of the request being served. KmodPathFinder::GeneratePath fills these
	// in; the stock pathCost reads them, because it charges extra for a step that attacks
	// the destination.
	int iDestX;
	int iDestY;
};

class FAStarNode;

class KmodPathFinder
{
public:
	KmodPathFinder();
	~KmodPathFinder();

	bool ValidateNodeMap(); // Called when SetSettings is used.

	bool GeneratePath(int x1, int y1, int x2, int y2);
	bool GeneratePath(const CvPlot* pToPlot); // just a wrapper for convenience
	FAStarNode* GetEndNode() const { FAssert(end_node); return end_node; } // Note: the returned pointer becomes invalid if the pathfinder is destroyed.
	bool IsPathComplete() const { return end_node; }
	int GetPathTurns() const;
	int GetFinalMoves() const;
	CvPlot* GetPathFirstPlot() const;
	CvPlot* GetPathEndTurnPlot() const;
	void SetSettings(const CvPathSettings& new_settings);
	void SetSettings(const CvSelectionGroup* pGroup, int iFlags = 0, int iMaxPath = -1, int iHW=-1) { SetSettings(CvPathSettings(pGroup, iFlags, iMaxPath, iHW)); }
	const CvSelectionGroup* GetGroup() const { return settings.pGroup; } // the group of the last SetSettings; may no longer exist
	void Reset();

protected:
	void AddStartNode();
	void RecalculateHeuristics();
	bool ProcessNode();
	void ForwardPropagate(FAStarNode* head, int cost_delta);
	typedef std::vector<FAStarNode*> OpenList_t;

	FAStarNode& GetNode(int x, int y) { return node_data[y*map_width+x]; }
	FAStarNode* node_data;
	OpenList_t open_list;
	std::vector<FAStarNode*> mapped_nodes; // every node with m_bOnStack set, for Reset

	int dest_x, dest_y;
	int start_x, start_y;
	bool dest_cost; // stock cost model: pathCostUsesDest was true for dest_x, dest_y when the tree was last used
	FAStarNode* end_node;
	CvPathSettings settings;
	int map_width, map_height;
};
