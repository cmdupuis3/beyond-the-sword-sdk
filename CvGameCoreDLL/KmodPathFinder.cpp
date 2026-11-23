#include "CvGameCoreDLL.h"
#include "KmodPathFinder.h"
#include "CvGameCoreUtils.h"
#include "CvGlobals.h"
#include "CvMap.h"
#include "CvPlot.h"
#include "CvSelectionGroup.h"
#include "CvUnit.h"
#include "CvInitCore.h"
#include "FProfiler.h"

#ifdef BRIDGED_PROFILE
#include <map>
#include <stdio.h>
// For the profile: how much searching a request takes, and how much of that is looking
// through the open list for the next node. Added up by ProcessNode for the request in hand.
static unsigned int s_uiProfileNodes = 0;
static unsigned int s_uiProfileOpenNodes = 0;
static unsigned long long s_uiProfileOpenTicks = 0;

// The nodes processed for the request, by the turn of the path on which the node is reached.
// A request limited to so many turns would not process the nodes of later turns.
#define PATH_PROBE_TURN_BUCKETS		(8)
static unsigned int s_auiProfileTurnNodes[PATH_PROBE_TURN_BUCKETS];
static const char* const s_aszProfileTurnBucket[PATH_PROBE_TURN_BUCKETS] = { "t1", "t2", "t3", "t4", "t5", "t6-9", "t10", "t11+" };

static int pathProbeTurnBucket(int iTurns)
{
	return ((iTurns <= 5) ? (std::max(1, iTurns) - 1) : ((iTurns <= 9) ? 5 : ((iTurns == 10) ? 6 : 7)));
}

const char* g_szPathProbeFunction = NULL;
int g_iPathProbeLine = 0;

// How many of the units differ: by unit type alone (bPromotions false), or by unit type,
// moves and promotions. Units that do not differ get the same answers from the path functions.
static unsigned int pathProbeKinds(const std::vector<CvPathUnit>& aUnits, bool bPromotions)
{
	unsigned int uiKinds = 0;

	for (size_t i = 0; i < aUnits.size(); i++)
	{
		bool bSeen = false;

		for (size_t j = 0; (j < i) && !bSeen; j++)
		{
			bSeen = (aUnits[i].pUnit->getUnitType() == aUnits[j].pUnit->getUnitType());

			if (bSeen && bPromotions)
			{
				bSeen = (aUnits[i].iMaxMoves == aUnits[j].iMaxMoves);

				for (int iPromotion = 0; bSeen && (iPromotion < GC.getNumPromotionInfos()); iPromotion++)
				{
					bSeen = (aUnits[i].pUnit->isHasPromotion((PromotionTypes)iPromotion) == aUnits[j].pUnit->isHasPromotion((PromotionTypes)iPromotion));
				}
			}
		}

		if (!bSeen)
		{
			uiKinds++;
		}
	}

	return uiKinds;
}
#endif

CvPathSettings::CvPathSettings(const CvSelectionGroup* pGroup, int iFlags, int iMaxPath, int iHW)
	: pGroup(const_cast<CvSelectionGroup*>(pGroup)), iFlags(iFlags), iMaxPath(iMaxPath), iHeuristicWeight(iHW),
	iDestX(INVALID_PLOT_COORD), iDestY(INVALID_PLOT_COORD), pKept(NULL)
// I'm really sorry about the const_cast. I can't fix the const-correctness of all the relevant functions,
// because some of them are dllexports. The original code essentially does the same thing anyway, with void* casts.
{
	// empty. (there use to be something here, but we don't need it anymore.)
}

//
KmodPathFinder::KmodPathFinder() :
	node_data(0),
	dest_x(INVALID_PLOT_COORD),
	dest_y(INVALID_PLOT_COORD),
	start_x(INVALID_PLOT_COORD),
	start_y(INVALID_PLOT_COORD),
	dest_cost(false),
	end_node(0),
	map_width(0),
	map_height(0)
{
	// Unfortunately, the pathfinder is constructed before the map width and height are determined.

	// Ideally the pathfinder would be initialised with a given CvMap, and then not refer to any global objects.
	// But that is not easy to do with the current code-base. Instead I'll just check the pathfinder settings
	// at particular times to make sure the map hasn't changed.

}

KmodPathFinder::~KmodPathFinder()
{
	free(node_data);
}

bool KmodPathFinder::ValidateNodeMap()
{
	PROFILE_FUNC();

	const int iWidth = GC.getMapINLINE().getGridWidthINLINE();
	const int iHeight = GC.getMapINLINE().getGridHeightINLINE();

	if (iWidth <= 0 || iHeight <= 0)
		return false;

	if (map_width != iWidth || map_height != iHeight)
	{
		// The old nodes are about to be moved or freed. Nothing may point at them afterwards.
		open_list.clear();
		mapped_nodes.clear();
		end_node = NULL;
		start_x = start_y = dest_x = dest_y = INVALID_PLOT_COORD;

		FAStarNode* new_data = (FAStarNode*)realloc(node_data, sizeof(*node_data)*iWidth*iHeight);
		if (!new_data)
		{
			free(node_data);
			node_data = NULL;
			map_width = map_height = 0;
			return false;
		}
		node_data = new_data;
		map_width = iWidth;
		map_height = iHeight;
		// A node of all zero bytes is one that has not been mapped. (see AddStartNode)
		memset((void*)node_data, 0, sizeof(*node_data)*map_width*map_height);
	}
	return true;
}

//
// Looks up, once for the request, what the path functions would otherwise look up for every
// step: the group's units and the things about them that a step's cost and moves depend on.
// None of it can change while a request is being served.
//
void KmodPathFinder::PrepareKept()
{
	kept.aUnits.clear();
	kept.bAIControlled = settings.pGroup->AI_isControlled();
	kept.eDefenseTeam = settings.pGroup->getTeam();

	for (CLLNode<IDInfo>* pUnitNode = settings.pGroup->headUnitNode(); pUnitNode != NULL; pUnitNode = settings.pGroup->nextUnitNode(pUnitNode))
	{
		CvPathUnit kUnit;
		kUnit.pUnit = ::getUnit(pUnitNode->m_data);
		kUnit.iMaxMoves = kUnit.pUnit->maxMoves();
		kUnit.eTeam = kUnit.pUnit->getTeam();
		kUnit.bCanFight = kUnit.pUnit->canFight();
		kUnit.bCanAttack = kUnit.pUnit->canAttack();
		kUnit.bNoDefensiveBonus = kUnit.pUnit->noDefensiveBonus();
		kUnit.bRiver = kUnit.pUnit->isRiver();
		kept.aUnits.push_back(kUnit);
	}

	kept.pStepFromPlot = NULL;
	kept.pStepToPlot = NULL;
	kept.aiStepMovementCost.resize(kept.aUnits.size());

	// a new stamp makes every defence modifier kept from earlier requests out of date
	if ((int)kept.auiDefenseStamp.size() != (map_width * map_height))
	{
		kept.auiDefenseStamp.assign(map_width * map_height, 0);
		kept.aiDefenseModifier.assign(map_width * map_height, 0);
		kept.uiDefenseStamp = 0;
	}

	kept.uiDefenseStamp++;

	if (kept.uiDefenseStamp == 0)
	{
		kept.auiDefenseStamp.assign(map_width * map_height, 0);
		kept.uiDefenseStamp = 1;
	}
}


bool KmodPathFinder::GeneratePath(int x1, int y1, int x2, int y2)
{
	PROFILE_FUNC();

	// only while this request is served: see the end of the function
	settings.pKept = NULL;

	FASSERT_BOUNDS(0, map_width , x1, "GeneratePath");
	FASSERT_BOUNDS(0, map_height, y1, "GeneratePath");
	FASSERT_BOUNDS(0, map_width , x2, "GeneratePath");
	FASSERT_BOUNDS(0, map_height, y2, "GeneratePath");

	end_node = NULL;

	if (!settings.pGroup)
		return false;

#ifdef BRIDGED_PROFILE
	static BridgedProfileSample aSamples[5] = {
		{ "path: K-Mod requests for a destination that is not valid" }, { "path: K-Mod requests that started a tree" }, { "path: K-Mod requests that carried on a tree" },
		{ "path: K-Mod nodes processed" }, { "path: K-Mod open list searches; calls = thousand open nodes looked at" } };
	static unsigned int uiProfileOpenNodesCarry = 0;	// the count is too big for the profile's 32 bits
	bool bProfileNewTree = false;
	const unsigned long long uiProfileRequestStart = bridgedProfileTicks();
	s_uiProfileNodes = 0;
	s_uiProfileOpenNodes = 0;
	s_uiProfileOpenTicks = 0;
	memset(s_auiProfileTurnNodes, 0, sizeof(s_auiProfileTurnNodes));
#endif

	if (!pathDestValid(x2, y2, &settings, 0))
	{
#ifdef BRIDGED_PROFILE
		bridgedProfileAdd(&aSamples[0], 0);
#endif
		return false;
	}

	// PATH_STOCK_STEPS is a global define that leaves the path functions looking everything
	// up for every step, as the stock code does. The paths are the same either way.
	static int iStockSteps = -1;
	if (iStockSteps == -1)
	{
		iStockSteps = ((GC.getDefineINT("PATH_STOCK_STEPS") != 0) ? 1 : 0);
	}
	if (iStockSteps == 0)
	{
		PrepareKept();
		settings.pKept = &kept;
	}

	if (x1 != start_x || y1 != start_y)
	{
		// Note: it may be possible to salvage some of the old data to get more speed.
		// eg. If the moves recorded on the node match the group,
		// just delete everything that isn't a direct descendant of the new start.
		// and then subtract the start cost & moves off all the remaining nodes.
		Reset(); // but this is easier.
	}
	bool bRecalcHeuristics = false;

	// stock cost model: a tree can serve a new destination only if no step in it was costed
	// by where the path ends. The stock pathCost has one such term: a step that attacks the
	// destination costs more, by an amount that depends on the plot attacked from. While that
	// term can apply (pathCostUsesDest) the tree is good for its own destination only, so it
	// is dropped when the destination changes to or from such a plot, and when the term comes
	// to apply or ceases to apply to an unchanged destination.
	const bool bDestCost = pathCostUsesDest(settings.pGroup, GC.getMapINLINE().plotSorenINLINE(x2, y2));

	if (dest_x != x2 || dest_y != y2)
	{
		bRecalcHeuristics = true;

		if (dest_cost || bDestCost)
			Reset();
	}
	else if (dest_cost != bDestCost)
	{
		Reset();
	}
	dest_cost = bDestCost;

	start_x = x1;
	start_y = y1;
	dest_x = x2;
	dest_y = y2;
	settings.iDestX = x2;
	settings.iDestY = y2;

	if (GetNode(x1, y1).m_bOnStack)
	{
		// stock cost model: pathAdd says what moves a path starts with, so that there is one
		// definition of it. (The stock one is not what K-Mod's CvSelectionGroup::maxMoves gives.)
		FAStarNode start_state;
		pathAdd(0, &start_state, ASNC_INITIALADD, &settings, 0);
		if (start_state.m_iData1 != GetNode(x1,  y1).m_iData1)
		{
			Reset();
			FAssert(!GetNode(x1,  y1).m_bOnStack);
		}
		// Note: This condition isn't actually enough to catch all signifiant changes.
		// We really need to check max moves /and/ moves left /and/ base moves.
		// but I don't feel like doing all that at the moment.
		// (CvSelectionGroup resets the pathfinder when the units of its group change.)
	}

	if (!GetNode(x1,  y1).m_bOnStack)
	{
		AddStartNode();
		bRecalcHeuristics = true;
#ifdef BRIDGED_PROFILE
		bProfileNewTree = true;
#endif
	}
	//else (not else. maybe start == dest)
	{
		// check if the end plot is already mapped.
		if (GetNode(x2,  y2).m_bOnStack)
			end_node = &GetNode(x2,  y2);
	}

#ifdef BRIDGED_PROFILE
	// probe of failed searches: the open nodes the request starts with, and the heuristics
	const unsigned int uiProfileOpenAtStart = (unsigned int)open_list.size();
	const unsigned long long uiProfileRecalcStart = bridgedProfileTicks();
#endif
	if (bRecalcHeuristics)
		RecalculateHeuristics();
#ifdef BRIDGED_PROFILE
	if (bRecalcHeuristics)
	{
		static BridgedProfileSample kRecalc = { "path probe: heuristics worked out again; calls = open nodes" };
		bridgedProfileAdd(&kRecalc, bridgedProfileTicks() - uiProfileRecalcStart, uiProfileOpenAtStart);
	}
#endif

	while (ProcessNode())
	{
		// nothing
	}

#ifdef BRIDGED_PROFILE
	bridgedProfileAdd(&aSamples[bProfileNewTree ? 1 : 2], 0);
	bridgedProfileAdd(&aSamples[3], 0, s_uiProfileNodes);
	uiProfileOpenNodesCarry += s_uiProfileOpenNodes;
	bridgedProfileAdd(&aSamples[4], s_uiProfileOpenTicks, uiProfileOpenNodesCarry / 1000);
	uiProfileOpenNodesCarry %= 1000;

	// the searched requests by how far the destination is and by how many turns the path
	// takes: the time of the request, and in the "nodes" rows the nodes processed as calls
	{
		static BridgedProfileSample aDistanceSamples[6] = {
			{ "path by distance: 1-2 plots" }, { "path by distance: 3-4 plots" }, { "path by distance: 5-8 plots" },
			{ "path by distance: 9-16 plots" }, { "path by distance: 17-32 plots" }, { "path by distance: 33 plots or more" } };
		static BridgedProfileSample aDistanceNodes[6] = {
			{ "path nodes by distance: 1-2 plots" }, { "path nodes by distance: 3-4 plots" }, { "path nodes by distance: 5-8 plots" },
			{ "path nodes by distance: 9-16 plots" }, { "path nodes by distance: 17-32 plots" }, { "path nodes by distance: 33 plots or more" } };
		static BridgedProfileSample aTurnSamples[5] = {
			{ "path by turns: 1 turn" }, { "path by turns: 2 turns" }, { "path by turns: 3-4 turns" }, { "path by turns: 5-8 turns" }, { "path by turns: 9 turns or more" } };
		static BridgedProfileSample aTurnNodes[5] = {
			{ "path nodes by turns: 1 turn" }, { "path nodes by turns: 2 turns" }, { "path nodes by turns: 3-4 turns" }, { "path nodes by turns: 5-8 turns" }, { "path nodes by turns: 9 turns or more" } };
		static BridgedProfileSample aFailed[2] = { { "path: searched and found no path" }, { "path nodes: searched and found no path" } };

		const unsigned long long uiRequestTicks = bridgedProfileTicks() - uiProfileRequestStart;

		// probe for a road corridor: requests more than 12 plots away, by whether they
		// started a tree and whether a land AI group asked with both ends on a route
		{
			static BridgedProfileSample aProbe[4] = {
				{ "path probe, far: started a tree, both ends on a route" }, { "path probe, far: started a tree, other" },
				{ "path probe, far: carried on a tree, both ends on a route" }, { "path probe, far: carried on a tree, other" } };
			static BridgedProfileSample aProbeNodes[4] = {
				{ "path probe nodes, far: started a tree, both ends on a route" }, { "path probe nodes, far: started a tree, other" },
				{ "path probe nodes, far: carried on a tree, both ends on a route" }, { "path probe nodes, far: carried on a tree, other" } };
			static BridgedProfileSample aProbeBig[2] = {
				{ "path probe, far, both ends on a route: requests of 300 nodes or more" }, { "path probe nodes, far, both ends on a route: in requests of 300 nodes or more" } };
			if (stepDistance(x1, y1, x2, y2) > 12)
			{
				const CvPlot* pFrom = GC.getMapINLINE().plotSorenINLINE(x1, y1);
				const CvPlot* pTo = GC.getMapINLINE().plotSorenINLINE(x2, y2);
				const bool bRoads = (settings.pGroup->getDomainType() == DOMAIN_LAND) && settings.pGroup->AI_isControlled()
					&& pFrom->isRoute() && pTo->isRoute() && (pFrom->getArea() == pTo->getArea());
				const int iProbe = (bProfileNewTree ? 0 : 2) + (bRoads ? 0 : 1);
				bridgedProfileAdd(&aProbe[iProbe], uiRequestTicks);
				bridgedProfileAdd(&aProbeNodes[iProbe], 0, s_uiProfileNodes);
				if (bRoads && (s_uiProfileNodes >= 300))
				{
					bridgedProfileAdd(&aProbeBig[0], uiRequestTicks);
					bridgedProfileAdd(&aProbeBig[1], 0, s_uiProfileNodes);
				}
			}
		}

		// probe: the searched requests by who asked (with the nodes by the turn they are
		// reached on), by the group's domain, and by how many units the group has; for groups
		// of two or more, how many of the units differ
		{
			struct ProbeSite
			{
				BridgedProfileSample kRequests;
				BridgedProfileSample akNodes[PATH_PROBE_TURN_BUCKETS];
			};
			static std::map<int, ProbeSite> aSites;		// by line of CvUnitAI.cpp; 0: asked from elsewhere
			char szName[64];

			ProbeSite& kSite = aSites[(g_szPathProbeFunction != NULL) ? g_iPathProbeLine : 0];
			const char* szFunction = ((g_szPathProbeFunction != NULL) ? g_szPathProbeFunction : "elsewhere");

			if (!kSite.kRequests.bRegistered)
			{
				sprintf(szName, "path site %.40s:%d", szFunction, g_iPathProbeLine);
				kSite.kRequests.szName = szName;
			}
			bridgedProfileAdd(&kSite.kRequests, uiRequestTicks);

			for (int i = 0; i < PATH_PROBE_TURN_BUCKETS; i++)
			{
				if (s_auiProfileTurnNodes[i] > 0)
				{
					if (!kSite.akNodes[i].bRegistered)
					{
						sprintf(szName, "path site nodes %.30s:%d %s", szFunction, g_iPathProbeLine, s_aszProfileTurnBucket[i]);
						kSite.akNodes[i].szName = szName;
					}
					bridgedProfileAdd(&kSite.akNodes[i], 0, s_auiProfileTurnNodes[i]);
				}
			}

			static BridgedProfileSample aDomain[3] = { { "path by domain: land" }, { "path by domain: sea" }, { "path by domain: other" } };
			static BridgedProfileSample aDomainNodes[3] = { { "path nodes by domain: land" }, { "path nodes by domain: sea" }, { "path nodes by domain: other" } };
			const DomainTypes eDomain = settings.pGroup->getDomainType();
			const int iDomain = ((eDomain == DOMAIN_LAND) ? 0 : ((eDomain == DOMAIN_SEA) ? 1 : 2));
			bridgedProfileAdd(&aDomain[iDomain], uiRequestTicks);
			bridgedProfileAdd(&aDomainNodes[iDomain], 0, s_uiProfileNodes);

			if (settings.pKept != NULL)
			{
				static BridgedProfileSample aUnits[4] = {
					{ "path by units in the group: 1" }, { "path by units in the group: 2-3" }, { "path by units in the group: 4-7" }, { "path by units in the group: 8 or more" } };
				static BridgedProfileSample aUnitNodes[4] = {
					{ "path nodes by units in the group: 1" }, { "path nodes by units in the group: 2-3" }, { "path nodes by units in the group: 4-7" }, { "path nodes by units in the group: 8 or more" } };
				static BridgedProfileSample aStack[4] = {
					{ "path probe, groups of 2 or more: nodes" }, { "path probe, groups of 2 or more: nodes x units" },
					{ "path probe, groups of 2 or more: nodes x unit types" }, { "path probe, groups of 2 or more: nodes x kinds of unit" } };
				const unsigned int uiUnits = (unsigned int)kept.aUnits.size();
				const int iUnitBucket = ((uiUnits <= 1) ? 0 : ((uiUnits <= 3) ? 1 : ((uiUnits <= 7) ? 2 : 3)));

				bridgedProfileAdd(&aUnits[iUnitBucket], uiRequestTicks);
				bridgedProfileAdd(&aUnitNodes[iUnitBucket], 0, s_uiProfileNodes);

				if ((uiUnits >= 2) && (s_uiProfileNodes > 0))
				{
					bridgedProfileAdd(&aStack[0], 0, s_uiProfileNodes);
					bridgedProfileAdd(&aStack[1], 0, s_uiProfileNodes * uiUnits);
					bridgedProfileAdd(&aStack[2], 0, s_uiProfileNodes * pathProbeKinds(kept.aUnits, false));
					bridgedProfileAdd(&aStack[3], 0, s_uiProfileNodes * pathProbeKinds(kept.aUnits, true));
				}
			}
		}

		if (end_node && (settings.iMaxPath < 0 || end_node->m_iData2 <= settings.iMaxPath))
		{
			const int iDistance = stepDistance(x1, y1, x2, y2);
			const int iDistanceBucket = ((iDistance <= 2) ? 0 : ((iDistance <= 4) ? 1 : ((iDistance <= 8) ? 2 : ((iDistance <= 16) ? 3 : ((iDistance <= 32) ? 4 : 5)))));
			const int iTurns = end_node->m_iData2;
			const int iTurnBucket = ((iTurns <= 1) ? 0 : ((iTurns == 2) ? 1 : ((iTurns <= 4) ? 2 : ((iTurns <= 8) ? 3 : 4))));

			bridgedProfileAdd(&aDistanceSamples[iDistanceBucket], uiRequestTicks);
			bridgedProfileAdd(&aDistanceNodes[iDistanceBucket], 0, s_uiProfileNodes);
			bridgedProfileAdd(&aTurnSamples[iTurnBucket], uiRequestTicks);
			bridgedProfileAdd(&aTurnNodes[iTurnBucket], 0, s_uiProfileNodes);
		}
		else
		{
			bridgedProfileAdd(&aFailed[0], uiRequestTicks);
			bridgedProfileAdd(&aFailed[1], 0, s_uiProfileNodes);

			// probe: the failed searches by how many nodes they processed, by whether the
			// tree was new, carried on or already searched out (no open nodes to start with),
			// by whether the destination was reached beyond the turn limit, and by who asked
			{
				static BridgedProfileSample aBySize[5] = {
					{ "path fail: 0 nodes processed" }, { "path fail: 1-9 nodes" }, { "path fail: 10-99 nodes" }, { "path fail: 100-999 nodes" }, { "path fail: 1000 nodes or more" } };
				static BridgedProfileSample aBySizeNodes[5] = {
					{ "path fail nodes: 0 nodes processed" }, { "path fail nodes: 1-9 nodes" }, { "path fail nodes: 10-99 nodes" }, { "path fail nodes: 100-999 nodes" }, { "path fail nodes: 1000 nodes or more" } };
				const unsigned int uiNodes = s_uiProfileNodes;
				const int iSize = ((uiNodes == 0) ? 0 : ((uiNodes < 10) ? 1 : ((uiNodes < 100) ? 2 : ((uiNodes < 1000) ? 3 : 4))));
				bridgedProfileAdd(&aBySize[iSize], uiRequestTicks);
				bridgedProfileAdd(&aBySizeNodes[iSize], 0, uiNodes);

				static BridgedProfileSample aByTree[3] = {
					{ "path fail: started a tree" }, { "path fail: carried on a tree with open nodes" }, { "path fail: carried on a tree already searched out" } };
				const int iTree = (bProfileNewTree ? 0 : ((uiProfileOpenAtStart > 0) ? 1 : 2));
				bridgedProfileAdd(&aByTree[iTree], uiRequestTicks);

				static BridgedProfileSample aByLimit[3] = {
					{ "path fail: no turn limit" }, { "path fail: turn limit, destination not reached" }, { "path fail: turn limit, destination reached in more turns" } };
				const int iLimit = ((settings.iMaxPath < 0) ? 0 : ((end_node == NULL) ? 1 : 2));
				bridgedProfileAdd(&aByLimit[iLimit], uiRequestTicks);

				static std::map<int, BridgedProfileSample> aFailSites;
				BridgedProfileSample& kFailSite = aFailSites[(g_szPathProbeFunction != NULL) ? g_iPathProbeLine : 0];
				if (!kFailSite.bRegistered)
				{
					char szFailName[64];
					sprintf(szFailName, "path fail site %.40s:%d", ((g_szPathProbeFunction != NULL) ? g_szPathProbeFunction : "elsewhere"), g_iPathProbeLine);
					kFailSite.szName = szFailName;
				}
				bridgedProfileAdd(&kFailSite, uiRequestTicks);
			}
		}
	}
#endif

	// what was looked up for this request is not to be used by anything that asks later
	settings.pKept = NULL;

	if (end_node && (settings.iMaxPath < 0 || end_node->m_iData2 <= settings.iMaxPath))
		return true;

	return false;
}

bool KmodPathFinder::GeneratePath(const CvPlot* pToPlot)
{
	if (!settings.pGroup || !pToPlot)
		return false;
	return GeneratePath(settings.pGroup->plot()->getX_INLINE(), settings.pGroup->plot()->getY_INLINE(),
		pToPlot->getX_INLINE(), pToPlot->getY_INLINE());
}

int KmodPathFinder::GetPathTurns() const
{
	FAssert(end_node);
	return end_node ? end_node->m_iData2 : 0;
}

int KmodPathFinder::GetFinalMoves() const
{
	FAssert(end_node);
	return end_node ? end_node->m_iData1 : 0;
}

CvPlot* KmodPathFinder::GetPathFirstPlot() const
{
	FAssert(end_node);
	if (!end_node)
		return NULL;

	FAStarNode* node = end_node;

	if (!node->m_pParent)
		return GC.getMapINLINE().plotSorenINLINE(node->m_iX, node->m_iY);

	while (node->m_pParent->m_pParent)
	{
		node = node->m_pParent;
	}

	return GC.getMapINLINE().plotSorenINLINE(node->m_iX, node->m_iY);
}

CvPlot* KmodPathFinder::GetPathEndTurnPlot() const
{
	FAssert(end_node);

	FAStarNode* node = end_node;

	FAssert(!node || node->m_iData2 == 1 || node->m_pParent);

	while (node && node->m_iData2 > 1)
	{
		node = node->m_pParent;
	}
	FAssert(node);
	return node ? GC.getMapINLINE().plotSorenINLINE(node->m_iX, node->m_iY) : NULL;
}

//
// The weight of the heuristic for a group: the search takes every step to the destination
// to cost at least this many times PATH_MOVEMENT_WEIGHT, and is only right if none costs
// less.
//
// The stock pathCost charges a step for the movement points it uses: what the plot costs to
// enter, or, when that leaves the group none, all the points it had left. Stock takes a
// step to cost one point at the least, because a group can have as little as one point
// left. But every number of points a step can cost is made of the same few: what a plot
// costs to enter (a whole move, or half of one for a unit with a double move), what a step
// along a route on the map costs, the moves a unit has in a turn, and the points the group
// has left now. So is every number of points the group can have left along a path, and the
// greatest number that divides all of them divides whatever a step costs. A step costs
// something, so it costs at least that: 30 or 60 points before any route is on the map,
// and 2 once there are railroads beside faster roads. With that weight the search heads for
// the destination instead of spreading round the start.
//
// Tier 2 (see BridgedTiers.h): a path costs what it did, but the plots are looked at in
// another order, so one of two paths of equal cost can be chosen differently.
// PATH_STOCK_HEURISTIC=1 is the off switch; a REPRODUCE_BTS build keeps the weight 1.
//
static bool pathStrongHeuristic()
{
#ifdef REPRODUCE_BTS
	return false;
#else
	static int iOff = -1;

	if (iOff == -1)
	{
		iOff = ((GC.getDefineINT("PATH_STOCK_HEURISTIC") != 0) ? 1 : 0);
	}

	return (iOff == 0);
#endif
}

static int pathGcd(int iA, int iB)
{
	while (iB != 0)
	{
		const int iRest = iA % iB;
		iA = iB;
		iB = iRest;
	}

	return iA;
}

static int pathHeuristicWeight(const CvSelectionGroup* pGroup)
{
	if (!pathStrongHeuristic())
	{
		return 1;
	}

	const int iNumTerrains = GC.getNumTerrainInfos();
	const int iNumFeatures = GC.getNumFeatureInfos();
	const int iNumRoutes = GC.getNumRouteInfos();

	// a plot that costs nothing to enter is charged one point (CvPlot::movementCost())
	for (int iI = 0; iI < iNumTerrains; iI++)
	{
		if (GC.getTerrainInfo((TerrainTypes)iI).getMovementCost() <= 0)
		{
			return 1;
		}
	}
	for (int iI = 0; iI < iNumFeatures; iI++)
	{
		if (GC.getFeatureInfo((FeatureTypes)iI).getMovementCost() <= 0)
		{
			return 1;
		}
	}

	int iWeight = GC.getMOVE_DENOMINATOR();

	for (CLLNode<IDInfo>* pUnitNode = pGroup->headUnitNode(); pUnitNode != NULL; pUnitNode = pGroup->nextUnitNode(pUnitNode))
	{
		const CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);

		if (pLoopUnit == NULL)
		{
			continue;
		}

		iWeight = pathGcd(iWeight, pLoopUnit->maxMoves());

		if (pLoopUnit->movesLeft() > 0)
		{
			iWeight = pathGcd(iWeight, pLoopUnit->movesLeft());
		}

		if (pLoopUnit->flatMovementCost() || (pLoopUnit->getDomainType() == DOMAIN_AIR))
		{
			continue;	// every plot costs it a whole move
		}

		// half a move for a plot the unit has a double move on
		bool bDoubleMove = pLoopUnit->isHillsDoubleMove();
		for (int iI = 0; !bDoubleMove && (iI < iNumTerrains); iI++)
		{
			bDoubleMove = pLoopUnit->isTerrainDoubleMove((TerrainTypes)iI);
		}
		for (int iI = 0; !bDoubleMove && (iI < iNumFeatures); iI++)
		{
			bDoubleMove = pLoopUnit->isFeatureDoubleMove((FeatureTypes)iI);
		}
		if (bDoubleMove)
		{
			iWeight = pathGcd(iWeight, GC.getMOVE_DENOMINATOR() / 2);
		}

		// a step from a plot with one route to a plot with another, of the routes on the map
		const CvTeam& kTeam = GET_TEAM(pLoopUnit->getTeam());
		for (int iI = 0; iI < iNumRoutes; iI++)
		{
			if (!GC.getMapINLINE().isRouteSeen((RouteTypes)iI))
			{
				continue;
			}

			for (int iJ = iI; iJ < iNumRoutes; iJ++)
			{
				if (!GC.getMapINLINE().isRouteSeen((RouteTypes)iJ))
				{
					continue;
				}

				const int iRouteCost = std::max(GC.getRouteInfo((RouteTypes)iI).getMovementCost() + kTeam.getRouteChange((RouteTypes)iI), GC.getRouteInfo((RouteTypes)iJ).getMovementCost() + kTeam.getRouteChange((RouteTypes)iJ));
				const int iRouteFlatCost = std::max(GC.getRouteInfo((RouteTypes)iI).getFlatMovementCost() * pLoopUnit->baseMoves(), GC.getRouteInfo((RouteTypes)iJ).getFlatMovementCost() * pLoopUnit->baseMoves());
				const int iStepCost = std::min(iRouteCost, iRouteFlatCost);

				if (iStepCost < 1)
				{
					return 1;
				}

				iWeight = pathGcd(iWeight, iStepCost);
			}
		}
	}

	return std::max(1, iWeight);
}


void KmodPathFinder::SetSettings(const CvPathSettings& new_settings)
{
	// whenever settings are changed, check that we have the right map size.
	if (!ValidateNodeMap())
	{
		FAssertMsg(false, "Failed to validate node map for pathfinder.");
		settings.pGroup = NULL;
		return;
	}

	// some flags are not relevant to pathfinder. We should try to strip those out to avoid unnecessary resets.
	// stock cost model: of the stock callbacks only pathDestValid reads MOVE_DECLARE_WAR, and it
	// is asked afresh for every request, so that flag does not matter to the tree either.
	int relevant_flags = ~(MOVE_DIRECT_ATTACK | MOVE_DECLARE_WAR); // any bar these

	if (settings.pGroup != new_settings.pGroup)
	{
		Reset();
	}
	else if ((settings.iFlags & relevant_flags) != (new_settings.iFlags & relevant_flags))
	{
		Reset();
	}

	const int iOldHeuristicWeight = settings.iHeuristicWeight;
	settings = new_settings;

	if (settings.iHeuristicWeight < 0)
	{
		// stock cost model: the heuristic for a plot is this weight times PATH_MOVEMENT_WEIGHT
		// for each step to the destination, and the search is only right if no path can cost
		// less than that. The stock pathCost charges a step that uses up the group's movement
		// points for the points it had left, not for what the plot costs to enter. The group
		// can have as little as one point left, so a step can cost as little as
		// PATH_MOVEMENT_WEIGHT + PATH_STEP_WEIGHT whatever routes exist, and the weight has to
		// be 1. The exe's pathfinder uses the same heuristic. (K-Mod's weight, the cheapest
		// route step, overestimates in that case.)
		// pathHeuristicWeight() gives 1 too unless every number of points a step of this
		// group can cost is known to be a multiple of something larger.
		settings.iHeuristicWeight = pathHeuristicWeight(settings.pGroup);
#ifdef BRIDGED_PROFILE
		{
			static BridgedProfileSample aSamples[4] = { { "path heuristic: requests with weight 1" }, { "path heuristic: requests with weight 2 to 9" }, { "path heuristic: requests with weight 10 to 29" }, { "path heuristic: requests with weight 30 or more" } };
			bridgedProfileAdd(&aSamples[(settings.iHeuristicWeight <= 1) ? 0 : ((settings.iHeuristicWeight < 10) ? 1 : ((settings.iHeuristicWeight < 30) ? 2 : 3))], 0);
		}
#endif
	}

	if (settings.iHeuristicWeight != iOldHeuristicWeight)
	{
		// the open nodes hold totals worked out with the old weight.
		Reset();
	}
}

void KmodPathFinder::Reset()
{
	// Only the nodes that were mapped differ from an unmapped one in a way that matters.
	// (K-Mod cleared the whole map here; this runs before most unit updates.)
	for (size_t i = 0; i < mapped_nodes.size(); i++)
	{
		memset((void*)mapped_nodes[i], 0, sizeof(FAStarNode));
	}
	mapped_nodes.clear();
	open_list.clear();
	end_node = NULL;
	// settings is set separately.
}

void KmodPathFinder::AddStartNode()
{
	FASSERT_BOUNDS(0, map_width , start_x, "KmodPathFinder::AddStartNode");
	FASSERT_BOUNDS(0, map_height, start_y, "KmodPathFinder::AddStartNode");

	// add initial node.
	FAStarNode* start_node = &GetNode(start_x, start_y);
	start_node->m_iX = start_x;
	start_node->m_iY = start_y;
	pathAdd(0, start_node, ASNC_INITIALADD, &settings, 0);
	start_node->m_iKnownCost = 0;

	open_list.push_back(start_node);

	// Note: I'd like to use NO_FASTARLIST as a signal that the node is uninitialised, but unfortunately
	// the default value for m_eFAStarListType is FASTARLIST_OPEN, because FASTARLIST_OPEN == 0.
	// Hence KmodPathFinder uses m_bOnStack to mean the node is connected. m_eFAStarListType is initialised manually.

	start_node->m_eFAStarListType = FASTARLIST_OPEN; // This means nothing. But maybe one day I'll use it.
	start_node->m_bOnStack = true; // This means the node is connected and ready to be used.
	mapped_nodes.push_back(start_node);
}

void KmodPathFinder::RecalculateHeuristics()
{
	// recalculate heuristic cost for all open nodes.
	for (OpenList_t::iterator i = open_list.begin(); i != open_list.end(); ++i)
	{
		int h = settings.iHeuristicWeight * pathHeuristic((*i)->m_iX, (*i)->m_iY, dest_x, dest_y);
		(*i)->m_iHeuristicCost = h;
		(*i)->m_iTotalCost = h + (*i)->m_iKnownCost;
	}
}

bool KmodPathFinder::ProcessNode()
{
#ifdef BRIDGED_PROFILE
	const unsigned long long uiProfileStart = bridgedProfileTicks();
#endif

	OpenList_t::iterator best_it = open_list.end();
	{
		int iLowestCost = end_node ? end_node->m_iKnownCost : MAX_INT;
		for (OpenList_t::iterator it = open_list.begin(); it != open_list.end(); ++it)
		{
			if ((*it)->m_iTotalCost < iLowestCost &&
				(settings.iMaxPath < 0 || (*it)->m_iData2 <= settings.iMaxPath))
			{
				best_it = it;
				iLowestCost = (*it)->m_iTotalCost;
			}
		}
	}

#ifdef BRIDGED_PROFILE
	s_uiProfileOpenNodes += (unsigned int)open_list.size();
	s_uiProfileOpenTicks += bridgedProfileTicks() - uiProfileStart;
#endif

	// if we didn't find a suitable node to process, then quit.
	if (best_it == open_list.end())
		return false;

#ifdef BRIDGED_PROFILE
	s_uiProfileNodes++;
#endif

	FAStarNode* parent_node = (*best_it);

#ifdef BRIDGED_PROFILE
	s_auiProfileTurnNodes[pathProbeTurnBucket(parent_node->m_iData2)]++;
#endif

	// erase the node from the open_list.
	// Note: this needs to be done before pushing new entries, otherwise the iterator will be invalid.
	open_list.erase(best_it);
	parent_node->m_eFAStarListType = FASTARLIST_CLOSED;

	FAssert(&GetNode(parent_node->m_iX, parent_node->m_iY) == parent_node);

	// Whether the group can move on from this plot depends on the turn and the moves it
	// arrives with (the stock danger test does), and those are the node's best only now that
	// it is its turn to be processed. The exe's pathfinder asks at the same moment. K-Mod
	// asked when the node was first mapped, by whichever neighbour came first.
	if (!pathValid_source(parent_node, settings.pGroup, settings.iFlags))
		return true; // This node is a dead end.

	// open a new node for each direction coming off the chosen node.
	for (int i = 0; i < NUM_DIRECTION_TYPES; i++)
	{
		CvPlot* pAdjacentPlot = plotDirection(parent_node->m_iX, parent_node->m_iY, (DirectionTypes)i);
		if (!pAdjacentPlot)
			continue;

		const int& x = pAdjacentPlot->getX_INLINE(); // convenience
		const int& y = pAdjacentPlot->getY_INLINE(); //

		if (parent_node->m_pParent && parent_node->m_pParent->m_iX == x && parent_node->m_pParent->m_iY == y)
			continue; // no need to backtrack.

		//int iPlotNum = GC.getMapINLINE().plotNumINLINE(x, y);
		//FAssert(iPlotNum >= 0 && iPlotNum < GC.getMapINLINE().numPlotsINLINE());

		FAStarNode* child_node = &GetNode(x, y);
		bool bNewNode = !child_node->m_bOnStack;

		if (bNewNode)
		{
			child_node->m_iX = x;
			child_node->m_iY = y;
			pathAdd(parent_node, child_node, ASNC_NEWADD, &settings, 0);

			if (pathValid_join(parent_node, child_node, settings.pGroup , settings.iFlags))
			{
				// This path to the new node is valid. So we need to fill in the data.
				child_node->m_iKnownCost = MAX_INT;
				child_node->m_iHeuristicCost = settings.iHeuristicWeight * pathHeuristic(x, y, dest_x, dest_y);
				// total cost will be set when the parent is set.

				child_node->m_bOnStack = true;
				mapped_nodes.push_back(child_node);

				// (a node that turns out to be a dead end is found to be one when it is processed.)
				open_list.push_back(child_node);
				child_node->m_eFAStarListType = FASTARLIST_OPEN;
			}
			else
			{
				// can't get to the plot from here.
				child_node = NULL;
			}
		}
		else
		{
			if (!pathValid_join(parent_node, child_node, settings.pGroup , settings.iFlags))
				child_node = NULL;
		}

		if (child_node == NULL)
			continue;

		FAssert(child_node->m_iX == x && child_node->m_iY == y);

		if (x == dest_x && y == dest_y)
			end_node = child_node; // We've found our destination - but we still need to finish our calculations

		if (parent_node->m_iKnownCost < child_node->m_iKnownCost)
		{
			int iNewCost = parent_node->m_iKnownCost + pathCost(parent_node, child_node, 666, &settings, 0);
			FAssert(iNewCost > 0);

			if (iNewCost < child_node->m_iKnownCost)
			{
				int cost_delta = iNewCost - child_node->m_iKnownCost; // new minus old. Negative value.

				child_node->m_iKnownCost = iNewCost;
				child_node->m_iTotalCost = child_node->m_iKnownCost + child_node->m_iHeuristicCost;

				// remove child from the list of the previous parent.
				if (child_node->m_pParent)
				{
					FAssert(!bNewNode);
					FAStarNode* x_parent = child_node->m_pParent;
					// x_parent just lost one of its children. We have to break the news to them.
					// this would easier if we had stl instead of bog arrays.
					int temp = x_parent->m_iNumChildren;
					for (int j = 0; j < x_parent->m_iNumChildren; j++)
					{
						if (x_parent->m_apChildren[j] == child_node)
						{
							// found it.
							for (j++ ; j < x_parent->m_iNumChildren; j++)
								x_parent->m_apChildren[j-1] = x_parent->m_apChildren[j];
							x_parent->m_apChildren[j-1] = 0; // not necessary, but easy enough to keep things neat.

							x_parent->m_iNumChildren--;
						}
					}
					FAssert(x_parent->m_iNumChildren == temp -1);
					// recalculate movement points
					pathAdd(parent_node, child_node, ASNC_PARENTADD_UP, &settings, 0);
				}

				// add child to the list of the new parent
				FAssert(parent_node->m_iNumChildren < NUM_DIRECTION_TYPES);
				parent_node->m_apChildren[parent_node->m_iNumChildren] = child_node;
				parent_node->m_iNumChildren++;
				child_node->m_pParent = parent_node;

				// update the new (reduced) costs for all the grandchildren.
				FAssert(child_node->m_iNumChildren == 0 || !bNewNode);
				ForwardPropagate(child_node, cost_delta);

				FAssert(child_node->m_iKnownCost > parent_node->m_iKnownCost);
			}
		}
		// else parent has higher cost. So there must already be a faster route to the child.
	}
	return true;
}

void KmodPathFinder::ForwardPropagate(FAStarNode* head, int cost_delta)
{
	//FAssert(cost_delta < 0 || head->m_iNumChildren == 0);
	// Note: there are some legitimate cases in which the cost_delta can be positive.
	// For example, suppose a shorter path is found to the parent plot, but the path involves
	// resting on less attractive plots. And suppose the addition moves saved by the shorter path
	// are then spent anyway to take the final step onto the destination...
	// In that case, although the path the parent plot has been upgraded, the path to the destination
	// is actually degraded (ie. it has a higher total cost). I can't think of a way to solve this problem.
	// (but I don't think it is very important.)

	// change the known cost of all children by cost_delta, recursively
	for (int i = 0; i < head->m_iNumChildren; i++)
	{
		FAssert(head->m_apChildren[i]->m_pParent == head);

		// recalculate movement points.
		int iOldMoves = head->m_apChildren[i]->m_iData1;
		int iOldTurns = head->m_apChildren[i]->m_iData2;
		int iNewDelta = cost_delta;
		pathAdd(head, head->m_apChildren[i], ASNC_PARENTADD_UP, &settings, 0);

		// if the moves don't match, we may need to recalculate the path cost.
		//if (iOldMoves != head->m_apChildren[i]->m_iData1)
		{
			// Strictly, the cost shouldn't depend on our path history, but it does - because I wanted to use
			// the path history for path symmetry breaking.
			// But anyway, according to the profiler, this is only going to cost us about a milisecond per turn.
			// (The stock pathCost depends on it too: a step is costed by the moves the group has before it.)
			int iPathCost = pathCost(head, head->m_apChildren[i], 666, &settings, 0);
			iNewDelta = head->m_iKnownCost + iPathCost - head->m_apChildren[i]->m_iKnownCost;
			//FAssert(iNewDelta <= 0);
		}

		head->m_apChildren[i]->m_iKnownCost += iNewDelta;
		head->m_apChildren[i]->m_iTotalCost += iNewDelta;

		FAssert(head->m_apChildren[i]->m_iKnownCost > head->m_iKnownCost);

		if (iNewDelta != 0 || iOldMoves != head->m_apChildren[i]->m_iData1 || iOldTurns != head->m_apChildren[i]->m_iData2)
			ForwardPropagate(head->m_apChildren[i], iNewDelta);
	}
}
