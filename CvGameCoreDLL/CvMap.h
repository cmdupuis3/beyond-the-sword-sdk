#pragma once

#ifndef CIV4_MAP_H
#define CIV4_MAP_H

//
//	FILE:	 CvMap.h
//	AUTHOR:  Soren Johnson
//	PURPOSE: Game map class
//-----------------------------------------------------------------------------
//	Copyright (c) 2004 Firaxis Games, Inc. All rights reserved.
//-----------------------------------------------------------------------------
//


#include "CvArea.h"
#include "CvPlot.h"


class FAStar;
class CvPlotGroup;


inline int coordRange(int iCoord, int iRange, bool bWrap)
{
	if (bWrap)
	{
		if (iRange != 0)
		{
			if (iCoord < 0 )
			{
				return (iRange + (iCoord % iRange));
			}
			else if (iCoord >= iRange)
			{
				return (iCoord % iRange);
			}
		}
	}

	return iCoord;
}


//
// holds initialization info
//
struct CvMapInitData
{
	int m_iGridW;						// in game plots
	int m_iGridH;						// in game plots
	int m_iTopLatitude;
	int m_iBottomLatitude;

	bool m_bWrapX;
	bool m_bWrapY;

	CvMapInitData(int iGridW=0, int iGridH=0, int iTopLatitude=90, int iBottomLatitude=-90, bool bWrapX=false, bool bWrapY=false) :
		m_iGridH(iGridH),m_iGridW(iGridW),m_iTopLatitude(iTopLatitude),m_iBottomLatitude(iBottomLatitude),m_bWrapY(bWrapY),m_bWrapX(bWrapX)
	{ }
};


//
// CvMap
//
class CvSelectionGroup;
class CvMap
{

	friend class CyMap;

public:

	DllExport CvMap();
	DllExport virtual ~CvMap();

	DllExport void init(CvMapInitData* pInitData=NULL);
	DllExport void setupGraphical();
	DllExport void reset(CvMapInitData* pInitData);

protected:

	void uninit();
	void setup();

public:

	DllExport void erasePlots();																			// Exposed to Python
	void setRevealedPlots(TeamTypes eTeam, bool bNewValue, bool bTerrainOnly = false);		// Exposed to Python
	void setAllPlotTypes(PlotTypes ePlotType);												// Exposed to Python

	void doTurn();																			

	DllExport void updateFlagSymbols();

	DllExport void updateFog();
	DllExport void updateVisibility();																// Exposed to Python
	DllExport void updateSymbolVisibility();
	void updateSymbols();
	DllExport void updateMinimapColor();															// Exposed to Python
	void updateSight(bool bIncrement);
	void updateIrrigated();
	DllExport void updateCenterUnit();
	void updateWorkingCity();
	void updateMinOriginalStartDist(CvArea* pArea);										// Exposed to Python
	void updateYield();

	void verifyUnitValidPlot();

	void combinePlotGroups(PlayerTypes ePlayer, CvPlotGroup* pPlotGroup1, CvPlotGroup* pPlotGroup2);	

	CvPlot* syncRandPlot(int iFlags = 0, int iArea = -1, int iMinUnitDistance = -1, int iTimeout = 100);// Exposed to Python 

	DllExport CvCity* findCity(int iX, int iY, PlayerTypes eOwner = NO_PLAYER, TeamTypes eTeam = NO_TEAM, bool bSameArea = true, bool bCoastalOnly = false, TeamTypes eTeamAtWarWith = NO_TEAM, DirectionTypes eDirection = NO_DIRECTION, CvCity* pSkipCity = NULL);	// Exposed to Python
	DllExport CvSelectionGroup* findSelectionGroup(int iX, int iY, PlayerTypes eOwner = NO_PLAYER, bool bReadyToSelect = false, bool bWorkers = false);				// Exposed to Python

	CvArea* findBiggestArea(bool bWater);																						// Exposed to Python

	int getMapFractalFlags();																												// Exposed to Python
	bool findWater(CvPlot* pPlot, int iRange, bool bFreshWater);										// Exposed to Python

	DllExport bool isPlot(int iX, int iY) const;																		// Exposed to Python
#ifdef _USRDLL
	inline int isPlotINLINE(int iX, int iY) const
	{
		return ((iX >= 0) && (iX < getGridWidthINLINE()) && (iY >= 0) && (iY < getGridHeightINLINE()));
	}
#endif
	DllExport int numPlots() const; 																								// Exposed to Python
#ifdef _USRDLL
	inline int numPlotsINLINE() const
	{
		return getGridWidthINLINE() * getGridHeightINLINE();
	}
#endif
	DllExport int plotNum(int iX, int iY) const;																		// Exposed to Python
#ifdef _USRDLL
	inline int plotNumINLINE(int iX, int iY) const
	{
		return ((iY * getGridWidthINLINE()) + iX);
	}
#endif
	int plotX(int iIndex) const;																										// Exposed to Python
	int plotY(int iIndex) const;																										// Exposed to Python

	DllExport int pointXToPlotX(float fX);
	DllExport float plotXToPointX(int iX);

	DllExport int pointYToPlotY(float fY);
	DllExport float plotYToPointY(int iY);

	float getWidthCoords();
	float getHeightCoords();

	int maxPlotDistance();																								// Exposed to Python
	int maxStepDistance();																								// Exposed to Python

	DllExport int getGridWidth() const;																		// Exposed to Python
#ifdef _USRDLL
	inline int getGridWidthINLINE() const
	{
		return m_iGridWidth;
	}
#endif
	DllExport int getGridHeight() const;																	// Exposed to Python
#ifdef _USRDLL
	inline int getGridHeightINLINE() const
	{
		return m_iGridHeight;
	}
#endif
	DllExport int getLandPlots();																					// Exposed to Python
	void changeLandPlots(int iChange);

	DllExport int getOwnedPlots();																				// Exposed to Python
	void changeOwnedPlots(int iChange);

	int getTopLatitude();																									// Exposed to Python
	int getBottomLatitude();																							// Exposed to Python

	int getNextRiverID();																									// Exposed to Python
	void incrementNextRiverID();																					// Exposed to Python

	DllExport bool isWrapX();																							// Exposed to Python
#ifdef _USRDLL
	inline bool isWrapXINLINE() const
	{
		return m_bWrapX;
	}
#endif
	DllExport bool isWrapY();																							// Exposed to Python
#ifdef _USRDLL
	inline bool isWrapYINLINE() const
	{
		return m_bWrapY;
	}
#endif
	DllExport bool isWrap();
#ifdef _USRDLL
	inline bool isWrapINLINE() const
	{
		return m_bWrapX || m_bWrapY;
	}
#endif
	DllExport WorldSizeTypes getWorldSize();															// Exposed to Python
	DllExport ClimateTypes getClimate();																	// Exposed to Python
	DllExport SeaLevelTypes getSeaLevel();																// Exposed to Python

	DllExport int getNumCustomMapOptions();
	DllExport CustomMapOptionTypes getCustomMapOption(int iOption);				// Exposed to Python

	int getNumBonuses(BonusTypes eIndex);																	// Exposed to Python
	void changeNumBonuses(BonusTypes eIndex, int iChange);

	int getNumBonusesOnLand(BonusTypes eIndex);														// Exposed to Python
	void changeNumBonusesOnLand(BonusTypes eIndex, int iChange);

	DllExport CvPlot* plotByIndex(int iIndex) const;											// Exposed to Python
#ifdef _USRDLL
	inline CvPlot* plotByIndexINLINE(int iIndex) const
	{
		return (((iIndex >= 0) && (iIndex < (getGridWidthINLINE() * getGridHeightINLINE()))) ? &(m_pMapPlots[iIndex]) : NULL);
	}
#endif
	DllExport CvPlot* plot(int iX, int iY) const;													// Exposed to Python
#ifdef _USRDLL
	__forceinline CvPlot* plotINLINE(int iX, int iY) const
	{
		if ((iX == INVALID_PLOT_COORD) || (iY == INVALID_PLOT_COORD))
		{
			return NULL;
		}
		int iMapX = coordRange(iX, getGridWidthINLINE(), isWrapXINLINE());
		int iMapY = coordRange(iY, getGridHeightINLINE(), isWrapYINLINE());
		return ((isPlotINLINE(iMapX, iMapY)) ? &(m_pMapPlots[plotNumINLINE(iMapX, iMapY)]) : NULL);
	}
	__forceinline CvPlot* plotSorenINLINE(int iX, int iY) const
	{
		if ((iX == INVALID_PLOT_COORD) || (iY == INVALID_PLOT_COORD))
		{
			return NULL;
		}
		return &(m_pMapPlots[plotNumINLINE(iX, iY)]);
	}
#endif
	DllExport CvPlot* pointToPlot(float fX, float fY);

	int getIndexAfterLastArea();														// Exposed to Python
	DllExport int getNumAreas();														// Exposed to Python
	DllExport int getNumLandAreas();
	CvArea* getArea(int iID);																// Exposed to Python
	CvArea* addArea();
	void deleteArea(int iID);
	// iteration
	CvArea* firstArea(int *pIterIdx, bool bRev=false);								// Exposed to Python
	CvArea* nextArea(int *pIterIdx, bool bRev=false);									// Exposed to Python

	void recalculateAreas();																		// Exposed to Python

	void resetPathDistance();																		// Exposed to Python
	int calculatePathDistance(CvPlot *pSource, CvPlot *pDest);	// Exposed to Python

	// Serialization:
	DllExport virtual void read(FDataStreamBase* pStream);
	DllExport virtual void write(FDataStreamBase* pStream);

	void rebuild(int iGridW, int iGridH, int iTopLatitude, int iBottomLatitude, bool bWrapX, bool bWrapY, WorldSizeTypes eWorldSize, ClimateTypes eClimate, SeaLevelTypes eSeaLevel, int iNumCustomMapOptions, CustomMapOptionTypes * eCustomMapOptions);		// Exposed to Python

	// Which teams have attacking units or territory where: see the team index in CvMap.cpp
	bool isTeamAttackerNear(const CvPlot* pPlot, int iRange, unsigned long long uiTeams, bool bTestMoves) const;
	void getTeamOwnedPlots(TeamTypes eTeam, std::vector<CvPlot*>& apPlots) const;
	bool isTeamBorderNear(const CvPlot* pPlot, int iRange, unsigned long long uiTeams) const;
	void changeBlockUnits(int iX, int iY, TeamTypes eTeam, int iChange);
	void changeBlockOwnedPlots(int iX, int iY, TeamTypes eTeam, int iChange);
	void updatePlotTeams(const CvPlot* pPlot);
	void setTeamIndexDirty();

	// City-site scores kept by the players: which plots need another look, a bit per player.
	// See CvPlayerAI::AI_refreshFoundValues().
	void foundValuesChanged(const CvPlot* pPlot, int iScoreRange, int iDeadlockRange);
	void setFoundValuesAllDirty();
	void setFoundValuesDirty(PlayerTypes ePlayer);
	void setFoundValuesDirtyInArea(PlayerTypes ePlayer, int iArea);
	void ensureFoundValueDirtyBits();
	bool isFoundValueScoreDirty(int iPlot, PlayerTypes ePlayer) const { return (((m_aiFoundScoreDirty[iPlot] >> ePlayer) & 1) != 0); }
	bool isFoundValueDeadlockDirty(int iPlot, PlayerTypes ePlayer) const { return (((m_aiFoundDeadlockDirty[iPlot] >> ePlayer) & 1) != 0); }
	void clearFoundValueDirty(int iPlot, PlayerTypes ePlayer)
	{
		m_aiFoundScoreDirty[iPlot] &= ~(1ULL << ePlayer);
		m_aiFoundDeadlockDirty[iPlot] &= ~(1ULL << ePlayer);
	}
protected:

	int m_iGridWidth;
	int m_iGridHeight;
	int m_iLandPlots;
	int m_iOwnedPlots;
	int m_iTopLatitude;
	int m_iBottomLatitude;
	int m_iNextRiverID;

	bool m_bWrapX;
	bool m_bWrapY;

	int* m_paiNumBonus;
	int* m_paiNumBonusOnLand;

	CvPlot* m_pMapPlots;

	FFreeListTrashArray<CvArea> m_areas;
	enum { TEAM_BLOCK_SHIFT = 3 };										// blocks of 8 x 8 plots
	enum { TEAM_REACH_WORDS = 4 };										// reach of 1, 2, 3 and 4 or more plots
	enum { TEAM_INDEX_MAX_RANGE = 12 };									// beyond this the questions are not answered from the index
	std::vector<unsigned long long> m_aiFoundScoreDirty;		// per plot, a bit per player: score it again
	std::vector<unsigned long long> m_aiFoundDeadlockDirty;	// per plot, a bit per player: count its deadlocked bonuses again
	mutable bool m_bTeamIndexDirty;
	mutable int m_iTeamBlocksWide;
	mutable std::vector<unsigned short> m_aiBlockTeamUnits;				// per block and team: attacking units there
	mutable std::vector<unsigned short> m_aiBlockTeamOwnedPlots;		// per block and team: plots owned there
	mutable std::vector<unsigned long long> m_aiBlockUnitTeams;			// per block: a bit for each team with such a unit
	mutable std::vector<unsigned long long> m_aiBlockOwnerTeams;		// per block: a bit for each team owning a plot
	mutable std::vector<unsigned long long> m_aiPlotUnitTeams;			// per plot: as for the blocks
	mutable std::vector<unsigned long long> m_aiPlotOwnerTeams;
	mutable std::vector<unsigned long long> m_aiPlotReachTeams;			// per plot, TEAM_REACH_WORDS words: teams with such a unit that reaches that far

	void rebuildTeamIndex() const;
	unsigned long long calculatePlotUnitTeams(const CvPlot* pPlot, unsigned long long* puiReachTeams) const;
	void changeBlockTeamCount(std::vector<unsigned short>& aiCounts, std::vector<unsigned long long>& aiBlockTeams, int iX, int iY, TeamTypes eTeam, int iChange);
	bool isTeamInBlocks(const std::vector<unsigned long long>& aiBlockTeams, int iX, int iY, int iRange, unsigned long long uiTeams) const;
	void getWindowCoords(int iX, int iY, int iRange, int* aiMapX, int* aiRowStart) const;

	void calculateAreas();

};

#endif
