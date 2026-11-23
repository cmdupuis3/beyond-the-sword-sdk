#pragma once

// utils.h

#ifndef CIV4_GAMECORE_UTILS_H
#define CIV4_GAMECORE_UTILS_H


//#include "CvStructs.h"
#include "CvGlobals.h"
#include "CvMap.h"

#ifndef _USRDLL
// use non inline functions when not in the dll
#define getMapINLINE	getMap
#define getGridHeightINLINE	getGridHeight
#define getGridWidthINLINE	getGridWidth
#define isWrapYINLINE	isWrapY
#define isWrapXINLINE	isWrapX
#define plotINLINE	plot
#define getX_INLINE	getX
#define getY_INLINE	getY

#endif

class CvPlot;
class CvCity;
class CvUnit;
class CvString;
class CvRandom;
class CvSelectionGroup;
class FAStarNode;
class FAStar;
class CvInfoBase;


// The offsets and lookups of the plot grid. They never change, so the DLL's own code reads
// them here, where the compiler has the values; CvGlobals::init() copies them for the exe
// and Python, which ask CvGlobals (getPlotDirectionX() and the rest).
namespace CvPlotTables
{
	inline constexpr int aiPlotDirectionX[NUM_DIRECTION_TYPES] =
	{
		0,	// DIRECTION_NORTH
		1,	// DIRECTION_NORTHEAST
		1,	// DIRECTION_EAST
		1,	// DIRECTION_SOUTHEAST
		0,	// DIRECTION_SOUTH
		-1,	// DIRECTION_SOUTHWEST
		-1,	// DIRECTION_WEST
		-1,	// DIRECTION_NORTHWEST
	};

	inline constexpr int aiPlotDirectionY[NUM_DIRECTION_TYPES] =
	{
		1,	// DIRECTION_NORTH
		1,	// DIRECTION_NORTHEAST
		0,	// DIRECTION_EAST
		-1,	// DIRECTION_SOUTHEAST
		-1,	// DIRECTION_SOUTH
		-1,	// DIRECTION_SOUTHWEST
		0,	// DIRECTION_WEST
		1,	// DIRECTION_NORTHWEST
	};

	inline constexpr int aiPlotCardinalDirectionX[NUM_CARDINALDIRECTION_TYPES] =
	{
		0,	// CARDINALDIRECTION_NORTH
		1,	// CARDINALDIRECTION_EAST
		0,	// CARDINALDIRECTION_SOUTH
		-1,	// CARDINALDIRECTION_WEST
	};

	inline constexpr int aiPlotCardinalDirectionY[NUM_CARDINALDIRECTION_TYPES] =
	{
		1,	// CARDINALDIRECTION_NORTH
		0,	// CARDINALDIRECTION_EAST
		-1,	// CARDINALDIRECTION_SOUTH
		0,	// CARDINALDIRECTION_WEST
	};

	inline constexpr int aiCityPlotX[NUM_CITY_PLOTS] =
	{
		0,
		0, 1, 1, 1, 0,-1,-1,-1,
		0, 1, 2, 2, 2, 1, 0,-1,-2,-2,-2,-1,
	};

	inline constexpr int aiCityPlotY[NUM_CITY_PLOTS] =
	{
		0,
		1, 1, 0,-1,-1,-1, 0, 1,
		2, 2, 1, 0,-1,-2,-2,-2,-1, 0, 1, 2,
	};

	inline constexpr int aiCityPlotPriority[NUM_CITY_PLOTS] =
	{
		0,
		1, 2, 1, 2, 1, 2, 1, 2,
		3, 4, 4, 3, 4, 4, 3, 4, 4, 3, 4, 4,
	};

	inline constexpr int aaiXYCityPlot[CITY_PLOTS_DIAMETER][CITY_PLOTS_DIAMETER] =
	{
		{-1, 17, 18, 19, -1,},

		{16, 6, 7, 8, 20,},

		{15, 5, 0, 1, 9,},

		{14, 4, 3, 2, 10,},

		{-1, 13, 12, 11, -1,}
	};

	inline constexpr DirectionTypes aeTurnRightDirection[NUM_DIRECTION_TYPES] =
	{
		DIRECTION_NORTHEAST,	// DIRECTION_NORTH
		DIRECTION_EAST,				// DIRECTION_NORTHEAST
		DIRECTION_SOUTHEAST,	// DIRECTION_EAST
		DIRECTION_SOUTH,			// DIRECTION_SOUTHEAST
		DIRECTION_SOUTHWEST,	// DIRECTION_SOUTH
		DIRECTION_WEST,				// DIRECTION_SOUTHWEST
		DIRECTION_NORTHWEST,	// DIRECTION_WEST
		DIRECTION_NORTH,			// DIRECTION_NORTHWEST
	};

	inline constexpr DirectionTypes aeTurnLeftDirection[NUM_DIRECTION_TYPES] =
	{
		DIRECTION_NORTHWEST,	// DIRECTION_NORTH
		DIRECTION_NORTH,			// DIRECTION_NORTHEAST
		DIRECTION_NORTHEAST,	// DIRECTION_EAST
		DIRECTION_EAST,				// DIRECTION_SOUTHEAST
		DIRECTION_SOUTHEAST,	// DIRECTION_SOUTH
		DIRECTION_SOUTH,			// DIRECTION_SOUTHWEST
		DIRECTION_SOUTHWEST,	// DIRECTION_WEST
		DIRECTION_WEST,				// DIRECTION_NORTHWEST
	};

	inline constexpr DirectionTypes aaeXYDirection[DIRECTION_DIAMETER][DIRECTION_DIAMETER] =
	{
		DIRECTION_SOUTHWEST, DIRECTION_WEST,	DIRECTION_NORTHWEST,
		DIRECTION_SOUTH,     NO_DIRECTION,    DIRECTION_NORTH,
		DIRECTION_SOUTHEAST, DIRECTION_EAST,	DIRECTION_NORTHEAST,
	};
}

#ifndef SQR
#define SQR(x) ( (x)*(x))
#endif

#undef max
#undef min

//sign function taken from FirePlace - JW
template<class T> __forceinline T getSign( T x ) { return (( x < 0 ) ? T(-1) : x > 0 ? T(1) : T(0)); };

inline int range(int iNum, int iLow, int iHigh)
{
	FAssertMsg(iHigh >= iLow, "High should be higher than low");

	if (iNum < iLow)
	{
		return iLow;
	}
	else if (iNum > iHigh)
	{
		return iHigh;
	}
	else
	{
		return iNum;
	}
}

inline float range(float fNum, float fLow, float fHigh)
{
	FAssertMsg(fHigh >= fLow, "High should be higher than low");

	if (fNum < fLow)
	{
		return fLow;
	}
	else if (fNum > fHigh)
	{
		return fHigh;
	}
	else
	{
		return fNum;
	}
}

inline int coordDistance(int iFrom, int iTo, int iRange, bool bWrap)
{
	if (bWrap && (abs(iFrom - iTo) > (iRange / 2)))
	{
		return (iRange - abs(iFrom - iTo));
	}

	return abs(iFrom - iTo);
}

inline int wrapCoordDifference(int iDiff, int iRange, bool bWrap)
{
	if (bWrap)
	{
		if (iDiff > (iRange / 2))
		{
			return (iDiff - iRange);
		}
		else if (iDiff < -(iRange / 2))
		{
			return (iDiff + iRange);
		}
	}

	return iDiff;
}

inline int xDistance(int iFromX, int iToX)
{
	return coordDistance(iFromX, iToX, GC.getMapINLINE().getGridWidthINLINE(), GC.getMapINLINE().isWrapXINLINE());
}

inline int yDistance(int iFromY, int iToY)
{
	return coordDistance(iFromY, iToY, GC.getMapINLINE().getGridHeightINLINE(), GC.getMapINLINE().isWrapYINLINE());
}

inline int dxWrap(int iDX)																													// Exposed to Python
{
	return wrapCoordDifference(iDX, GC.getMapINLINE().getGridWidthINLINE(), GC.getMapINLINE().isWrapXINLINE());
}

inline int dyWrap(int iDY)																													// Exposed to Python
{
	return wrapCoordDifference(iDY, GC.getMapINLINE().getGridHeightINLINE(), GC.getMapINLINE().isWrapYINLINE());
}

// 4 | 4 | 3 | 3 | 3 | 4 | 4
// -------------------------
// 4 | 3 | 2 | 2 | 2 | 3 | 4
// -------------------------
// 3 | 2 | 1 | 1 | 1 | 2 | 3
// -------------------------
// 3 | 2 | 1 | 0 | 1 | 2 | 3
// -------------------------
// 3 | 2 | 1 | 1 | 1 | 2 | 3
// -------------------------
// 4 | 3 | 2 | 2 | 2 | 3 | 4
// -------------------------
// 4 | 4 | 3 | 3 | 3 | 4 | 4
//
// Returns the distance between plots according to the pattern above...
inline int plotDistance(int iX1, int iY1, int iX2, int iY2)													// Exposed to Python
{
	int iDX;
	int iDY;

	iDX = xDistance(iX1, iX2);
	iDY = yDistance(iY1, iY2);

	return (std::max(iDX, iDY) + (std::min(iDX, iDY) / 2));
}

// 3 | 3 | 3 | 3 | 3 | 3 | 3
// -------------------------
// 3 | 2 | 2 | 2 | 2 | 2 | 3
// -------------------------
// 3 | 2 | 1 | 1 | 1 | 2 | 3
// -------------------------
// 3 | 2 | 1 | 0 | 1 | 2 | 3
// -------------------------
// 3 | 2 | 1 | 1 | 1 | 2 | 3
// -------------------------
// 3 | 2 | 2 | 2 | 2 | 2 | 3
// -------------------------
// 3 | 3 | 3 | 3 | 3 | 3 | 3
//
// Returns the distance between plots according to the pattern above...
inline int stepDistance(int iX1, int iY1, int iX2, int iY2)													// Exposed to Python
{
	return std::max(xDistance(iX1, iX2), yDistance(iY1, iY2));
}

inline CvPlot* plotDirection(int iX, int iY, DirectionTypes eDirection)							// Exposed to Python
{
	if(eDirection == NO_DIRECTION)
	{
		return GC.getMapINLINE().plotINLINE(iX, iY);
	}
	else
	{
		return GC.getMapINLINE().plotINLINE((iX + CvPlotTables::aiPlotDirectionX[eDirection]), (iY + CvPlotTables::aiPlotDirectionY[eDirection]));
	}
}

inline CvPlot* plotCardinalDirection(int iX, int iY, CardinalDirectionTypes eCardinalDirection)	// Exposed to Python
{
	return GC.getMapINLINE().plotINLINE((iX + CvPlotTables::aiPlotCardinalDirectionX[eCardinalDirection]), (iY + CvPlotTables::aiPlotCardinalDirectionY[eCardinalDirection]));
}

inline CvPlot* plotXY(int iX, int iY, int iDX, int iDY)																// Exposed to Python
{
	return GC.getMapINLINE().plotINLINE((iX + iDX), (iY + iDY));
}

inline DirectionTypes directionXY(int iDX, int iDY)																		// Exposed to Python
{
	if ((abs(iDX) > DIRECTION_RADIUS) || (abs(iDY) > DIRECTION_RADIUS))
	{
		return NO_DIRECTION;
	}
	else
	{
		return CvPlotTables::aaeXYDirection[iDX + DIRECTION_RADIUS][iDY + DIRECTION_RADIUS];
	}
}

inline DirectionTypes directionXY(const CvPlot* pFromPlot, const CvPlot* pToPlot)			// Exposed to Python
{
	return directionXY(dxWrap(pToPlot->getX_INLINE() - pFromPlot->getX_INLINE()), dyWrap(pToPlot->getY_INLINE() - pFromPlot->getY_INLINE()));
}

CvPlot* plotCity(int iX, int iY, int iIndex);																			// Exposed to Python
int plotCityXY(int iDX, int iDY);																									// Exposed to Python
int plotCityXY(const CvCity* pCity, const CvPlot* pPlot);													// Exposed to Python

CardinalDirectionTypes getOppositeCardinalDirection(CardinalDirectionTypes eDir);	// Exposed to Python 
DirectionTypes cardinalDirectionToDirection(CardinalDirectionTypes eCard);				// Exposed to Python
DllExport bool isCardinalDirection(DirectionTypes eDirection);															// Exposed to Python
DllExport DirectionTypes estimateDirection(int iDX, int iDY);																// Exposed to Python
DllExport DirectionTypes estimateDirection(const CvPlot* pFromPlot, const CvPlot* pToPlot);
DllExport float directionAngle(DirectionTypes eDirection);

DllExport bool atWar(TeamTypes eTeamA, TeamTypes eTeamB);												// Exposed to Python
bool isPotentialEnemy(TeamTypes eOurTeam, TeamTypes eTheirTeam);			// Exposed to Python

DllExport CvCity* getCity(IDInfo city);	// Exposed to Python
DllExport CvUnit* getUnit(IDInfo unit);	// Exposed to Python

bool isBeforeUnitCycle(const CvUnit* pFirstUnit, const CvUnit* pSecondUnit);
bool isPromotionValid(PromotionTypes ePromotion, UnitTypes eUnit, bool bLeader);	// Exposed to Python

int getPopulationAsset(int iPopulation);								// Exposed to Python
int getLandPlotsAsset(int iLandPlots);									// Exposed to Python
int getPopulationPower(int iPopulation);								// Exposed to Python
int getPopulationScore(int iPopulation);								// Exposed to Python
int getLandPlotsScore(int iLandPlots);									// Exposed to Python
int getTechScore(TechTypes eTech);											// Exposed to Python
int getWonderScore(BuildingClassTypes eWonderClass);		// Exposed to Python

ImprovementTypes finalImprovementUpgrade(ImprovementTypes eImprovement, int iCount = 0);		// Exposed to Python

int getWorldSizeMaxConscript(CivicTypes eCivic);								// Exposed to Python

bool isReligionTech(TechTypes eTech);														// Exposed to Python

bool isTechRequiredForUnit(TechTypes eTech, UnitTypes eUnit);							// Exposed to Python
bool isTechRequiredForBuilding(TechTypes eTech, BuildingTypes eBuilding);	// Exposed to Python
bool isTechRequiredForProject(TechTypes eTech, ProjectTypes eProject);		// Exposed to Python

bool isWorldUnitClass(UnitClassTypes eUnitClass);											// Exposed to Python
bool isTeamUnitClass(UnitClassTypes eUnitClass);											// Exposed to Python
bool isNationalUnitClass(UnitClassTypes eUnitClass);									// Exposed to Python
bool isLimitedUnitClass(UnitClassTypes eUnitClass);										// Exposed to Python

bool isWorldWonderClass(BuildingClassTypes eBuildingClass);						// Exposed to Python
bool isTeamWonderClass(BuildingClassTypes eBuildingClass);						// Exposed to Python
bool isNationalWonderClass(BuildingClassTypes eBuildingClass);				// Exposed to Python
bool isLimitedWonderClass(BuildingClassTypes eBuildingClass);					// Exposed to Python
int limitedWonderClassLimit(BuildingClassTypes eBuildingClass);

bool isWorldProject(ProjectTypes eProject);														// Exposed to Python
bool isTeamProject(ProjectTypes eProject);														// Exposed to Python
bool isLimitedProject(ProjectTypes eProject);													// Exposed to Python

__int64 getBinomialCoefficient(int iN, int iK);
int getCombatOdds(CvUnit* pAttacker, CvUnit* pDefender);							// Exposed to Python

int getEspionageModifier(TeamTypes eOurTeam, TeamTypes eTargetTeam);							// Exposed to Python

DllExport void setTradeItem(TradeData* pItem, TradeableItems eItemType = TRADE_ITEM_NONE, int iData = 0);

bool isPlotEventTrigger(EventTriggerTypes eTrigger);

TechTypes getDiscoveryTech(UnitTypes eUnit, PlayerTypes ePlayer);

void setListHelp(wchar* szBuffer, const wchar* szStart, const wchar* szItem, const wchar* szSeparator, bool bFirst);
void setListHelp(CvWString& szBuffer, const wchar* szStart, const wchar* szItem, const wchar* szSeparator, bool bFirst);
void setListHelp(CvWStringBuffer& szBuffer, const wchar* szStart, const wchar* szItem, const wchar* szSeparator, bool bFirst);

// PlotUnitFunc's...
bool PUF_isGroupHead( const CvUnit* pUnit, int iData1 = -1, int iData2 = -1);
bool PUF_isPlayer( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_isTeam( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_isCombatTeam(const CvUnit* pUnit, int iData1, int iData2);
bool PUF_isOtherPlayer( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_isOtherTeam( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_isEnemy( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_isVisible( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_isVisibleDebug( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_canSiege( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_isPotentialEnemy( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_canDeclareWar( const CvUnit* pUnit, int iData1 = -1, int iData2 = -1);
bool PUF_canDefend( const CvUnit* pUnit, int iData1 = -1, int iData2 = -1);
bool PUF_cannotDefend( const CvUnit* pUnit, int iData1 = -1, int iData2 = -1);
bool PUF_canDefendGroupHead( const CvUnit* pUnit, int iData1 = -1, int iData2 = -1);
bool PUF_canDefendEnemy( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_canDefendPotentialEnemy( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_canAirAttack( const CvUnit* pUnit, int iData1 = -1, int iData2 = -1);
bool PUF_canAirDefend( const CvUnit* pUnit, int iData1 = -1, int iData2 = -1);
bool PUF_isFighting( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_isAnimal( const CvUnit* pUnit, int iData1 = -1, int iData2 = -1);
bool PUF_isMilitaryHappiness( const CvUnit* pUnit, int iData1 = -1, int iData2 = -1);
bool PUF_isInvestigate( const CvUnit* pUnit, int iData1 = -1, int iData2 = -1);
bool PUF_isCounterSpy( const CvUnit* pUnit, int iData1 = -1, int iData2 = -1);
bool PUF_isSpy( const CvUnit* pUnit, int iData1 = -1, int iData2 = -1);
bool PUF_isUnitType( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_isDomainType( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_isUnitAIType( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_isCityAIType( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_isNotCityAIType( const CvUnit* pUnit, int iData1, int iData2 = -1);
bool PUF_isSelected( const CvUnit* pUnit, int iData1 = -1, int iData2 = -1);
bool PUF_makeInfoBarDirty(CvUnit* pUnit, int iData1 = -1, int iData2 = -1);
bool PUF_isNoMission(const CvUnit* pUnit, int iData1 = -1, int iData2 = -1);
bool PUF_isFiniteRange(const CvUnit* pUnit, int iData1 = -1, int iData2 = -1);

// FAStarFunc...
int potentialIrrigation(FAStarNode* parent, FAStarNode* node, int data, const void* pointer, FAStar* finder);
int checkFreshWater(FAStarNode* parent, FAStarNode* node, int data, const void* pointer, FAStar* finder);
int changeIrrigated(FAStarNode* parent, FAStarNode* node, int data, const void* pointer, FAStar* finder);
// Counts the changes to anything an AI attitude is worked out from: see CvPlayerAI::AI_getAttitudeVal().
// Every write to such a value must call invalidateAttitudeCache().
extern int g_iAttitudeEpoch;
inline void invalidateAttitudeCache() { g_iAttitudeEpoch++; }

int pathDestValid(int iToX, int iToY, const void* pointer, FAStar* finder);
int pathHeuristic(int iFromX, int iFromY, int iToX, int iToY);
int pathCost(FAStarNode* parent, FAStarNode* node, int data, const void* pointer, FAStar* finder);
bool pathCostUsesDest(CvSelectionGroup* pSelectionGroup, const CvPlot* pDestPlot);
int pathValid_join(FAStarNode* parent, FAStarNode* node, CvSelectionGroup* pSelectionGroup, int iFlags);
int pathValid_source(FAStarNode* parent, CvSelectionGroup* pSelectionGroup, int iFlags);
int pathValid(FAStarNode* parent, FAStarNode* node, int data, const void* pointer, FAStar* finder);
int pathAdd(FAStarNode* parent, FAStarNode* node, int data, const void* pointer, FAStar* finder);
int stepDestValid(int iToX, int iToY, const void* pointer, FAStar* finder);
int stepHeuristic(int iFromX, int iFromY, int iToX, int iToY);
int stepValid(FAStarNode* parent, FAStarNode* node, int data, const void* pointer, FAStar* finder);
int stepCost(FAStarNode* parent, FAStarNode* node, int data, const void* pointer, FAStar* finder);
int stepAdd(FAStarNode* parent, FAStarNode* node, int data, const void* pointer, FAStar* finder);
int routeValid(FAStarNode* parent, FAStarNode* node, int data, const void* pointer, FAStar* finder);
bool isRouteFinderPlot(PlayerTypes ePlayer, CvPlot* pPlot);	// routeValid()'s test of the plot stepped into
int borderValid(FAStarNode* parent, FAStarNode* node, int data, const void* pointer, FAStar* finder);
int areaValid(FAStarNode* parent, FAStarNode* node, int data, const void* pointer, FAStar* finder);
int joinArea(FAStarNode* parent, FAStarNode* node, int data, const void* pointer, FAStar* finder);
int plotGroupValid(FAStarNode* parent, FAStarNode* node, int data, const void* pointer, FAStar* finder);
int countPlotGroup(FAStarNode* parent, FAStarNode* node, int data, const void* pointer, FAStar* finder);

int baseYieldToSymbol(int iNumYieldTypes, int iYieldStack);

bool isPickableName(const TCHAR* szName);

DllExport int* shuffle(int iNum, CvRandom& rand);
void shuffleArray(int* piShuffle, int iNum, CvRandom& rand);

int getTurnMonthForGame(int iGameTurn, int iStartYear, CalendarTypes eCalendar, GameSpeedTypes eSpeed);
int getTurnYearForGame(int iGameTurn, int iStartYear, CalendarTypes eCalendar, GameSpeedTypes eSpeed);

void getDirectionTypeString(CvWString& szString, DirectionTypes eDirectionType);
void getCardinalDirectionTypeString(CvWString& szString, CardinalDirectionTypes eDirectionType);
void getActivityTypeString(CvWString& szString, ActivityTypes eActivityType);
void getMissionTypeString(CvWString& szString, MissionTypes eMissionType);
void getMissionAIString(CvWString& szString, MissionAITypes eMissionAI);
void getUnitAIString(CvWString& szString, UnitAITypes eUnitAI);

#endif
