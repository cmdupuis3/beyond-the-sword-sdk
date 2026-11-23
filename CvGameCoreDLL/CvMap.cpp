//
//	FILE:	 CvMap.cpp
//	AUTHOR:  Soren Johnson
//	PURPOSE: Game map class
//-----------------------------------------------------------------------------
//	Copyright (c) 2004 Firaxis Games, Inc. All rights reserved.
//-----------------------------------------------------------------------------
//


#include "CvGameCoreDLL.h"
#include "CvMap.h"
#include "CvCity.h"
#include "CvPlotGroup.h"
#include "CvGlobals.h"
#include "CvGameAI.h"
#include "CvPlayerAI.h"
#include "CvRandom.h"
#include "CvGameCoreUtils.h"
#include "CvFractal.h"
#include "CvPlot.h"
#include "CvGameCoreUtils.h"
#include "CvMap.h"
#include "CvMapGenerator.h"
#include "FAStarNode.h"
#include "CvInitCore.h"
#include "CvInfos.h"
#include "FProfiler.h"
#include "CyArgsList.h"

#include "CvDLLEngineIFaceBase.h"
#include "CvDLLIniParserIFaceBase.h"
#include "CvDLLFAStarIFaceBase.h"
#include "CvDLLFAStarIFaceBase.h"
#include "CvDLLPythonIFaceBase.h"

// Public Functions...

CvMap::CvMap()
{
	CvMapInitData defaultMapData;

	m_paiNumBonus = NULL;
	m_paiNumBonusOnLand = NULL;

	m_pMapPlots = NULL;

	reset(&defaultMapData);
}


CvMap::~CvMap()
{
	uninit();
}

// FUNCTION: init()
// Initializes the map.
// Parameters:
//	pInitInfo					- Optional init structure (used for WB load)
// Returns:
//	nothing.
void CvMap::init(CvMapInitData* pInitInfo/*=NULL*/)
{
	int iX, iY;

	PROFILE("CvMap::init");
	gDLL->logMemState( CvString::format("CvMap::init begin - world size=%s, climate=%s, sealevel=%s, num custom options=%6", 
		GC.getWorldInfo(GC.getInitCore().getWorldSize()).getDescription(), 
		GC.getClimateInfo(GC.getInitCore().getClimate()).getDescription(), 
		GC.getSeaLevelInfo(GC.getInitCore().getSeaLevel()).getDescription(),
		GC.getInitCore().getNumCustomMapOptions()).c_str() );

	gDLL->getPythonIFace()->callFunction(gDLL->getPythonIFace()->getMapScriptModule(), "beforeInit");

	//--------------------------------
	// Init saved data
	reset(pInitInfo);

	//--------------------------------
	// Init containers
	m_areas.init();

	//--------------------------------
	// Init non-saved data
	setup();

	//--------------------------------
	// Init other game data
	gDLL->logMemState("CvMap before init plots");
	m_pMapPlots = new CvPlot[numPlotsINLINE()];
	for (iX = 0; iX < getGridWidthINLINE(); iX++)
	{
		gDLL->callUpdater();
		for (iY = 0; iY < getGridHeightINLINE(); iY++)
		{
			plotSorenINLINE(iX, iY)->init(iX, iY);
		}
	}
	calculateAreas();
	gDLL->logMemState("CvMap after init plots");
}


void CvMap::uninit()
{
	SAFE_DELETE_ARRAY(m_paiNumBonus);
	SAFE_DELETE_ARRAY(m_paiNumBonusOnLand);

	SAFE_DELETE_ARRAY(m_pMapPlots);

	m_areas.uninit();
}

// FUNCTION: reset()
// Initializes data members that are serialized.
void CvMap::reset(CvMapInitData* pInitInfo)
{
	int iI;

	//--------------------------------
	// Uninit class
	uninit();
	m_bSymbolsDeferred = false;
	m_bSymbolsDirty = false;
	m_bSymbolsStale = false;
	m_iCityLayoutUpdates = 0;
	m_iPlotLayoutUpdates = 0;
	m_bTeamIndexDirty = true;
	m_bAreaPlotsDirty = true;
	m_uiRoutesSeen = 0;
	setFoundValuesAllDirty();

	//
	// set grid size
	// initially set in terrain cell units
	//
	m_iGridWidth = (GC.getInitCore().getWorldSize() != NO_WORLDSIZE) ? GC.getWorldInfo(GC.getInitCore().getWorldSize()).getGridWidth (): 0;	//todotw:tcells wide
	m_iGridHeight = (GC.getInitCore().getWorldSize() != NO_WORLDSIZE) ? GC.getWorldInfo(GC.getInitCore().getWorldSize()).getGridHeight (): 0;

	// allow grid size override
	if (pInitInfo)
	{
		m_iGridWidth	= pInitInfo->m_iGridW;
		m_iGridHeight	= pInitInfo->m_iGridH;
	}
	else
	{
		// check map script for grid size override
		if (GC.getInitCore().getWorldSize() != NO_WORLDSIZE)
		{
			std::vector<int> out;
			CyArgsList argsList;
			argsList.add(GC.getInitCore().getWorldSize());
			bool ok = gDLL->getPythonIFace()->callFunction(gDLL->getPythonIFace()->getMapScriptModule(), "getGridSize", argsList.makeFunctionArgs(), &out);

			if (ok && !gDLL->getPythonIFace()->pythonUsingDefaultImpl() && out.size() == 2)
			{
				m_iGridWidth = out[0];
				m_iGridHeight = out[1];
				FAssertMsg(m_iGridWidth > 0 && m_iGridHeight > 0, "the width and height returned by python getGridSize() must be positive");
			}
		}

		// convert to plot dimensions
		if (GC.getNumLandscapeInfos() > 0)
		{
			m_iGridWidth *= GC.getLandscapeInfo(GC.getActiveLandscapeID()).getPlotsPerCellX();
			m_iGridHeight *= GC.getLandscapeInfo(GC.getActiveLandscapeID()).getPlotsPerCellY();
		}
	}

	m_iLandPlots = 0;
	m_iOwnedPlots = 0;

	if (pInitInfo)
	{
		m_iTopLatitude = pInitInfo->m_iTopLatitude;
		m_iBottomLatitude = pInitInfo->m_iBottomLatitude;
	}
	else
	{
		// Check map script for latitude override (map script beats ini file)

		long resultTop = -1, resultBottom = -1;
		bool okX = gDLL->getPythonIFace()->callFunction(gDLL->getPythonIFace()->getMapScriptModule(), "getTopLatitude", NULL, &resultTop);
		bool overrideX = !gDLL->getPythonIFace()->pythonUsingDefaultImpl();
		bool okY = gDLL->getPythonIFace()->callFunction(gDLL->getPythonIFace()->getMapScriptModule(), "getBottomLatitude", NULL, &resultBottom);
		bool overrideY = !gDLL->getPythonIFace()->pythonUsingDefaultImpl();

		if (okX && okY && overrideX && overrideY && resultTop != -1 && resultBottom != -1)
		{
			m_iTopLatitude = resultTop;
			m_iBottomLatitude = resultBottom;
		}
	}

	m_iTopLatitude = std::min(m_iTopLatitude, 90);
	m_iTopLatitude = std::max(m_iTopLatitude, -90);
	m_iBottomLatitude = std::min(m_iBottomLatitude, 90);
	m_iBottomLatitude = std::max(m_iBottomLatitude, -90);

	m_iNextRiverID = 0;

	//
	// set wrapping
	//
	m_bWrapX = true;
	m_bWrapY = false;
	if (pInitInfo)
	{
		m_bWrapX = pInitInfo->m_bWrapX;
		m_bWrapY = pInitInfo->m_bWrapY;
	}
	else
	{
		// Check map script for wrap override (map script beats ini file)

		long resultX = -1, resultY = -1;
		bool okX = gDLL->getPythonIFace()->callFunction(gDLL->getPythonIFace()->getMapScriptModule(), "getWrapX", NULL, &resultX);
		bool overrideX = !gDLL->getPythonIFace()->pythonUsingDefaultImpl();
		bool okY = gDLL->getPythonIFace()->callFunction(gDLL->getPythonIFace()->getMapScriptModule(), "getWrapY", NULL, &resultY);
		bool overrideY = !gDLL->getPythonIFace()->pythonUsingDefaultImpl();

		if (okX && okY && overrideX && overrideY && resultX != -1 && resultY != -1)
		{
			m_bWrapX = (resultX != 0);
			m_bWrapY = (resultY != 0);
		}
	}

	if (GC.getNumBonusInfos())
	{
		FAssertMsg((0 < GC.getNumBonusInfos()), "GC.getNumBonusInfos() is not greater than zero but an array is being allocated in CvMap::reset");
		FAssertMsg(m_paiNumBonus==NULL, "mem leak m_paiNumBonus");
		m_paiNumBonus = new int[GC.getNumBonusInfos()];
		FAssertMsg(m_paiNumBonusOnLand==NULL, "mem leak m_paiNumBonusOnLand");
		m_paiNumBonusOnLand = new int[GC.getNumBonusInfos()];
		for (iI = 0; iI < GC.getNumBonusInfos(); iI++)
		{
			m_paiNumBonus[iI] = 0;
			m_paiNumBonusOnLand[iI] = 0;
		}
	}

	m_areas.removeAll();
}


// FUNCTION: setup()
// Initializes all data that is not serialized but needs to be initialized after loading.
void CvMap::setup()
{
	PROFILE("CvMap::setup");

	gDLL->getFAStarIFace()->Initialize(&GC.getPathFinder(), getGridWidthINLINE(), getGridHeightINLINE(), isWrapXINLINE(), isWrapYINLINE(), pathDestValid, pathHeuristic, pathCost, pathValid, pathAdd, NULL, NULL);
	gDLL->getFAStarIFace()->Initialize(&GC.getInterfacePathFinder(), getGridWidthINLINE(), getGridHeightINLINE(), isWrapXINLINE(), isWrapYINLINE(), pathDestValid, pathHeuristic, pathCost, pathValid, pathAdd, NULL, NULL);
	gDLL->getFAStarIFace()->Initialize(&GC.getStepFinder(), getGridWidthINLINE(), getGridHeightINLINE(), isWrapXINLINE(), isWrapYINLINE(), stepDestValid, stepHeuristic, stepCost, stepValid, stepAdd, NULL, NULL);
	gDLL->getFAStarIFace()->Initialize(&GC.getRouteFinder(), getGridWidthINLINE(), getGridHeightINLINE(), isWrapXINLINE(), isWrapYINLINE(), NULL, NULL, NULL, routeValid, NULL, NULL, NULL);
	gDLL->getFAStarIFace()->Initialize(&GC.getBorderFinder(), getGridWidthINLINE(), getGridHeightINLINE(), isWrapXINLINE(), isWrapYINLINE(), NULL, NULL, NULL, borderValid, NULL, NULL, NULL);
	gDLL->getFAStarIFace()->Initialize(&GC.getAreaFinder(), getGridWidthINLINE(), getGridHeightINLINE(), isWrapXINLINE(), isWrapYINLINE(), NULL, NULL, NULL, areaValid, NULL, joinArea, NULL);
	gDLL->getFAStarIFace()->Initialize(&GC.getPlotGroupFinder(), getGridWidthINLINE(), getGridHeightINLINE(), isWrapXINLINE(), isWrapYINLINE(), NULL, NULL, NULL, plotGroupValid, NULL, countPlotGroup, NULL);

	// The K-Mod pathfinder (CvSelectionGroup::path_finder) sizes itself to the map when it is
	// next used. Anything it has kept is from the map this one replaces.
	CvSelectionGroup::dropPathSearch();
}


//////////////////////////////////////
// graphical only setup
//////////////////////////////////////
void CvMap::setupGraphical()
{
	if (!GC.IsGraphicsInitialized())
		return;

	if (m_pMapPlots != NULL)
	{
		int iI;
		for (iI = 0; iI < numPlotsINLINE(); iI++)
		{
			gDLL->callUpdater();	// allow windows msgs to update
			plotByIndexINLINE(iI)->setupGraphical();
		}
	}
}


void CvMap::erasePlots()
{
	int iI;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		plotByIndexINLINE(iI)->erase();
	}
}


void CvMap::setRevealedPlots(TeamTypes eTeam, bool bNewValue, bool bTerrainOnly)
{
	PROFILE_FUNC();

	int iI;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		plotByIndexINLINE(iI)->setRevealed(eTeam, bNewValue, bTerrainOnly, NO_TEAM, false);
	}

	GC.getGameINLINE().updatePlotGroups();
}


void CvMap::setAllPlotTypes(PlotTypes ePlotType)
{
	//float startTime = (float) timeGetTime();

	for(int i=0;i<numPlotsINLINE();i++)
	{
		plotByIndexINLINE(i)->setPlotType(ePlotType, false, false);
	}

	recalculateAreas();

	//rebuild landscape
	gDLL->getEngineIFace()->RebuildAllPlots();

	//mark minimap as dirty
	gDLL->getEngineIFace()->SetDirty(MinimapTexture_DIRTY_BIT, true);
	gDLL->getEngineIFace()->SetDirty(GlobeTexture_DIRTY_BIT, true);
	
	//float endTime = (float) timeGetTime();
	//OutputDebugString(CvString::format("[Jason] setAllPlotTypes: %f\n", endTime - startTime).c_str());
}


// XXX generalize these funcs? (macro?)
void CvMap::doTurn()
{
	PROFILE("CvMap::doTurn()")

	int iI;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		plotByIndexINLINE(iI)->doTurn();
	}
}


void CvMap::updateFlagSymbols()
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	int iI;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		pLoopPlot = plotByIndexINLINE(iI);

		if (pLoopPlot->isFlagDirty())
		{
			pLoopPlot->updateFlagSymbol();
			pLoopPlot->setFlagDirty(false);
		}
	}
}


void CvMap::updateFog()
{
	int iI;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		plotByIndexINLINE(iI)->updateFog();
	}
}


void CvMap::updateVisibility()
{
	int iI;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		plotByIndexINLINE(iI)->updateVisibility();
	}
}


void CvMap::updateSymbolVisibility()
{
	int iI;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		plotByIndexINLINE(iI)->updateSymbolVisibility();
	}
}


void CvMap::updateSymbols()
{
	int iI;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		plotByIndexINLINE(iI)->updateSymbols();
	}
}


bool CvMap::isStockLayoutUpdates() const
{
	static int iStock = -1;

	if (iStock == -1)
	{
		iStock = ((GC.getDefineINT("STOCK_LAYOUT_UPDATES") != 0) ? 1 : 0);
	}

	return (iStock != 0);
}


//
// Before it draws, the exe asks every city and every plot whether its layout has to be made
// anew (the buildings and houses of a city, the improvement and bonus of a plot), and makes
// all of those that say yes in that one frame. After a round of AI turns that is most cities
// and many plots, and the frame takes from a tenth of a second on a small map to over a
// second on a huge one late in the game: about three quarters of the time the exe keeps
// between two calls of CvGame::update().
//
// A city or plot the active team can see says yes at once, as before. One it cannot see
// (in the fog, where the layout is drawn as it was last made) says yes only while this
// allowance lasts, a few for each call of CvGame::update(); the rest stay marked and are
// asked again in the next frames. So their layouts are brought up to date over the next
// seconds, during the player's own turn, and no frame has to make them all.
// STOCK_LAYOUT_UPDATES=1 has everything say yes at once.
//
static const int CITY_LAYOUT_UPDATES_PER_CALL = 2;
static const int PLOT_LAYOUT_UPDATES_PER_CALL = 16;

bool CvMap::takeLayoutUpdate(bool bCity) const
{
	int& iLeft = (bCity ? m_iCityLayoutUpdates : m_iPlotLayoutUpdates);

	if (iLeft <= 0)
	{
		return false;
	}

	iLeft--;

	return true;
}


// Called at the end of CvGame::update().
void CvMap::resetLayoutUpdates()
{
	m_iCityLayoutUpdates = CITY_LAYOUT_UPDATES_PER_CALL;
	m_iPlotLayoutUpdates = PLOT_LAYOUT_UPDATES_PER_CALL;
}


bool CvMap::isStockSymbolUpdates() const
{
	static int iStock = -1;

	if (iStock == -1)
	{
		iStock = ((GC.getDefineINT("STOCK_SYMBOL_UPDATES") != 0) ? 1 : 0);
	}

	return (iStock != 0);
}


// The symbol updates kept back while CvGame::update() ran: see CvPlot::updateSymbols().
// Also makes the symbols that were left out because the active team could not see the
// plot, once it can: see CvPlot::updateSymbols() and CvPlot::updateFeatureSymbol().
// Revealing a plot does that itself; looking again here, each time the exe is about to
// get the screen, covers every other way a plot can come to be seen.
void CvMap::updateDeferredSymbols()
{
	PROFILE_FUNC();

	if (!m_bSymbolsDirty && !m_bSymbolsStale)
	{
		return;
	}
	m_bSymbolsDirty = false;

	bool bStale = false;
	for (int iI = 0; iI < numPlotsINLINE(); iI++)
	{
		if (plotByIndexINLINE(iI)->updateDeferredSymbols())
		{
			bStale = true;
		}
	}
	m_bSymbolsStale = bStale;
}


void CvMap::updateMinimapColor()
{
	int iI;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		plotByIndexINLINE(iI)->updateMinimapColor();
	}
}


void CvMap::updateSight(bool bIncrement)
{
	for (int iI = 0; iI < numPlotsINLINE(); iI++)
	{
		plotByIndexINLINE(iI)->updateSight(bIncrement, false);
	}

	GC.getGameINLINE().updatePlotGroups();
}


void CvMap::updateIrrigated()
{
	int iI;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		plotByIndexINLINE(iI)->updateIrrigated();
	}
}


void CvMap::updateCenterUnit()
{
	int iI;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		plotByIndexINLINE(iI)->updateCenterUnit();
	}
}


void CvMap::updateWorkingCity()
{
	int iI;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		plotByIndexINLINE(iI)->updateWorkingCity();
	}
}


void CvMap::updateMinOriginalStartDist(CvArea* pArea)
{
	PROFILE_FUNC();

	CvPlot* pStartingPlot;
	CvPlot* pLoopPlot;
	int iDist;
	int iI, iJ;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		pLoopPlot = plotByIndexINLINE(iI);

		if (pLoopPlot->area() == pArea)
		{
			pLoopPlot->setMinOriginalStartDist(-1);
		}
	}

	for (iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		pStartingPlot = GET_PLAYER((PlayerTypes)iI).getStartingPlot();

		if (pStartingPlot != NULL)
		{
			if (pStartingPlot->area() == pArea)
			{
				for (iJ = 0; iJ < numPlotsINLINE(); iJ++)
				{
					pLoopPlot = plotByIndexINLINE(iJ);

					if (pLoopPlot->area() == pArea)
					{
						
						//iDist = GC.getMapINLINE().calculatePathDistance(pStartingPlot, pLoopPlot);
						iDist = stepDistance(pStartingPlot->getX_INLINE(), pStartingPlot->getY_INLINE(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE());

						if (iDist != -1)
						{
						    //int iCrowDistance = plotDistance(pStartingPlot->getX_INLINE(), pStartingPlot->getY_INLINE(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE());
						    //iDist = std::min(iDist,  iCrowDistance * 2);
							if ((pLoopPlot->getMinOriginalStartDist() == -1) || (iDist < pLoopPlot->getMinOriginalStartDist()))
							{
								pLoopPlot->setMinOriginalStartDist(iDist);
							}
						}
					}
				}
			}
		}
	}
}


void CvMap::updateYield()
{
	int iI;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		plotByIndexINLINE(iI)->updateYield();
	}
}


void CvMap::verifyUnitValidPlot()
{
	for (int iI = 0; iI < numPlotsINLINE(); iI++)
	{
		plotByIndexINLINE(iI)->verifyUnitValidPlot();
	}
}


void CvMap::combinePlotGroups(PlayerTypes ePlayer, CvPlotGroup* pPlotGroup1, CvPlotGroup* pPlotGroup2)
{
	CLLNode<XYCoords>* pPlotNode;
	CvPlotGroup* pNewPlotGroup;
	CvPlotGroup* pOldPlotGroup;
	CvPlot* pPlot;

	FAssertMsg(pPlotGroup1 != NULL, "pPlotGroup is not assigned to a valid value");
	FAssertMsg(pPlotGroup2 != NULL, "pPlotGroup is not assigned to a valid value");

	if (pPlotGroup1 == pPlotGroup2)
	{
		return;
	}

	if (pPlotGroup1->getLengthPlots() > pPlotGroup2->getLengthPlots())
	{
		pNewPlotGroup = pPlotGroup1;
		pOldPlotGroup = pPlotGroup2;
	}
	else
	{
		pNewPlotGroup = pPlotGroup2;
		pOldPlotGroup = pPlotGroup1;
	}

	pPlotNode = pOldPlotGroup->headPlotsNode();
	while (pPlotNode != NULL)
	{
		pPlot = plotSorenINLINE(pPlotNode->m_data.iX, pPlotNode->m_data.iY);
		pNewPlotGroup->addPlot(pPlot);
		pPlotNode = pOldPlotGroup->deletePlotsNode(pPlotNode);
	}
}

CvPlot* CvMap::syncRandPlot(int iFlags, int iArea, int iMinUnitDistance, int iTimeout)
{
	CvPlot* pPlot;
	CvPlot* pTestPlot;
	CvPlot* pLoopPlot;
	bool bValid;
	int iCount;
	int iDX, iDY;

	pPlot = NULL;

	iCount = 0;

	while (iCount < iTimeout)
	{
		pTestPlot = plotSorenINLINE(GC.getGameINLINE().getSorenRandNum(getGridWidthINLINE(), "Rand Plot Width"), GC.getGameINLINE().getSorenRandNum(getGridHeightINLINE(), "Rand Plot Height"));

		FAssertMsg(pTestPlot != NULL, "TestPlot is not assigned a valid value");

		if ((iArea == -1) || (pTestPlot->getArea() == iArea))
		{
			bValid = true;

			if (bValid)
			{
				if (iMinUnitDistance != -1)
				{
					for (iDX = -(iMinUnitDistance); iDX <= iMinUnitDistance; iDX++)
					{
						for (iDY = -(iMinUnitDistance); iDY <= iMinUnitDistance; iDY++)
						{
							pLoopPlot	= plotXY(pTestPlot->getX_INLINE(), pTestPlot->getY_INLINE(), iDX, iDY);

							if (pLoopPlot != NULL)
							{
								if (pLoopPlot->isUnit())
								{
									bValid = false;
								}
							}
						}
					}
				}
			}

			if (bValid)
			{
				if (iFlags & RANDPLOT_LAND)
				{
					if (pTestPlot->isWater())
					{
						bValid = false;
					}
				}
			}

			if (bValid)
			{
				if (iFlags & RANDPLOT_UNOWNED)
				{
					if (pTestPlot->isOwned())
					{
						bValid = false;
					}
				}
			}

			if (bValid)
			{
				if (iFlags & RANDPLOT_ADJACENT_UNOWNED)
				{
					if (pTestPlot->isAdjacentOwned())
					{
						bValid = false;
					}
				}
			}

			if (bValid)
			{
				if (iFlags & RANDPLOT_ADJACENT_LAND)
				{
					if (!(pTestPlot->isAdjacentToLand()))
					{
						bValid = false;
					}
				}
			}

			if (bValid)
			{
				if (iFlags & RANDPLOT_PASSIBLE)
				{
					if (pTestPlot->isImpassable())
					{
						bValid = false;
					}
				}
			}

			if (bValid)
			{
				if (iFlags & RANDPLOT_NOT_VISIBLE_TO_CIV)
				{
					if (pTestPlot->isVisibleToCivTeam())
					{
						bValid = false;
					}
				}
			}

			if (bValid)
			{
				if (iFlags & RANDPLOT_NOT_CITY)
				{
					if (pTestPlot->isCity())
					{
						bValid = false;
					}
				}
			}

			if (bValid)
			{
				pPlot = pTestPlot;
				break;
			}
		}

		iCount++;
	}

	return pPlot;
}


CvCity* CvMap::findCity(int iX, int iY, PlayerTypes eOwner, TeamTypes eTeam, bool bSameArea, bool bCoastalOnly, TeamTypes eTeamAtWarWith, DirectionTypes eDirection, CvCity* pSkipCity)
{
	PROFILE_FUNC();

	CvCity* pLoopCity;
	CvCity* pBestCity;
	int iValue;
	int iBestValue;
	int iLoop;
	int iI;

	// XXX look for barbarian cities???

	iBestValue = MAX_INT;
	pBestCity = NULL;

	for (iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			if ((eOwner == NO_PLAYER) || (iI == eOwner))
			{
				if ((eTeam == NO_TEAM) || (GET_PLAYER((PlayerTypes)iI).getTeam() == eTeam))
				{
					for (pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop))
					{
						if (!bSameArea || (pLoopCity->area() == plotINLINE(iX, iY)->area()) || (bCoastalOnly && (pLoopCity->waterArea() == plotINLINE(iX, iY)->area())))
						{
							if (!bCoastalOnly || pLoopCity->isCoastal(GC.getMIN_WATER_SIZE_FOR_OCEAN()))
							{
								if ((eTeamAtWarWith == NO_TEAM) || atWar(GET_PLAYER((PlayerTypes)iI).getTeam(), eTeamAtWarWith))
								{
									if ((eDirection == NO_DIRECTION) || (estimateDirection(dxWrap(pLoopCity->getX_INLINE() - iX), dyWrap(pLoopCity->getY_INLINE() - iY)) == eDirection))
									{
										if ((pSkipCity == NULL) || (pLoopCity != pSkipCity))
										{
											iValue = plotDistance(iX, iY, pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE());

											if (iValue < iBestValue)
											{
												iBestValue = iValue;
												pBestCity = pLoopCity;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	return pBestCity;
}


CvSelectionGroup* CvMap::findSelectionGroup(int iX, int iY, PlayerTypes eOwner, bool bReadyToSelect, bool bWorkers)
{
	CvSelectionGroup* pLoopSelectionGroup;
	CvSelectionGroup* pBestSelectionGroup;
	int iValue;
	int iBestValue;
	int iLoop;
	int iI;

	// XXX look for barbarian cities???

	iBestValue = MAX_INT;
	pBestSelectionGroup = NULL;

	for (iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			if ((eOwner == NO_PLAYER) || (iI == eOwner))
			{
				for(pLoopSelectionGroup = GET_PLAYER((PlayerTypes)iI).firstSelectionGroup(&iLoop); pLoopSelectionGroup != NULL; pLoopSelectionGroup = GET_PLAYER((PlayerTypes)iI).nextSelectionGroup(&iLoop))
				{
					if (!bReadyToSelect || pLoopSelectionGroup->readyToSelect())
					{
						if (!bWorkers || pLoopSelectionGroup->hasWorker())
						{
							iValue = plotDistance(iX, iY, pLoopSelectionGroup->getX(), pLoopSelectionGroup->getY());

							if (iValue < iBestValue)
							{
								iBestValue = iValue;
								pBestSelectionGroup = pLoopSelectionGroup;
							}
						}
					}
				}
			}
		}
	}

	return pBestSelectionGroup;
}


CvArea* CvMap::findBiggestArea(bool bWater)
{
	CvArea* pLoopArea;
	CvArea* pBestArea;
	int iValue;
	int iBestValue;
	int iLoop;

	iBestValue = 0;
	pBestArea = NULL;

	for(pLoopArea = firstArea(&iLoop); pLoopArea != NULL; pLoopArea = nextArea(&iLoop))
	{
		if (pLoopArea->isWater() == bWater)
		{
			iValue = pLoopArea->getNumTiles();

			if (iValue > iBestValue)
			{
				iBestValue = iValue;
				pBestArea = pLoopArea;
			}
		}
	}

	return pBestArea;
}


int CvMap::getMapFractalFlags()
{
	int wrapX = 0;
	if (isWrapXINLINE())
	{
		wrapX = (int)CvFractal::FRAC_WRAP_X;
	}

	int wrapY = 0;
	if (isWrapYINLINE())
	{
		wrapY = (int)CvFractal::FRAC_WRAP_Y;
	}

	return (wrapX | wrapY);
}


//"Check plots for wetlands or seaWater.  Returns true if found"
bool CvMap::findWater(CvPlot* pPlot, int iRange, bool bFreshWater)
{
	PROFILE("CvMap::findWater()");

	CvPlot* pLoopPlot;
	int iDX, iDY;

	for (iDX = -(iRange); iDX <= iRange; iDX++)
	{
		for (iDY = -(iRange); iDY <= iRange; iDY++)
		{
			pLoopPlot	= plotXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iDX, iDY);

			if (pLoopPlot != NULL)
			{
				if (bFreshWater)
				{
					if (pLoopPlot->isFreshWater())
					{
						return true;
					}
				}
				else
				{
					if (pLoopPlot->isWater())
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}


bool CvMap::isPlot(int iX, int iY) const
{
	return isPlotINLINE(iX, iY);
}


int CvMap::numPlots() const																											 
{
	return numPlotsINLINE();
}


int CvMap::plotNum(int iX, int iY) const
{
	return plotNumINLINE(iX, iY);
}


int CvMap::plotX(int iIndex) const
{
	return (iIndex % getGridWidthINLINE());
}


int CvMap::plotY(int iIndex) const
{
	return (iIndex / getGridWidthINLINE());
}


int CvMap::pointXToPlotX(float fX)
{
	float fWidth, fHeight;
	gDLL->getEngineIFace()->GetLandscapeGameDimensions(fWidth, fHeight);
	return (int)(((fX + (fWidth/2.0f)) / fWidth) * getGridWidthINLINE());
}


float CvMap::plotXToPointX(int iX)
{
	float fWidth, fHeight;
	gDLL->getEngineIFace()->GetLandscapeGameDimensions(fWidth, fHeight);
	return ((iX * fWidth) / ((float)getGridWidthINLINE())) - (fWidth / 2.0f) + (GC.getPLOT_SIZE() / 2.0f);
}


int CvMap::pointYToPlotY(float fY)
{
	float fWidth, fHeight;
	gDLL->getEngineIFace()->GetLandscapeGameDimensions(fWidth, fHeight);
	return (int)(((fY + (fHeight/2.0f)) / fHeight) * getGridHeightINLINE());
}


float CvMap::plotYToPointY(int iY)
{
	float fWidth, fHeight;
	gDLL->getEngineIFace()->GetLandscapeGameDimensions(fWidth, fHeight);
	return ((iY * fHeight) / ((float)getGridHeightINLINE())) - (fHeight / 2.0f) + (GC.getPLOT_SIZE() / 2.0f);
}


float CvMap::getWidthCoords()																	
{
	return (GC.getPLOT_SIZE() * ((float)getGridWidthINLINE()));
}


float CvMap::getHeightCoords()																	
{
	return (GC.getPLOT_SIZE() * ((float)getGridHeightINLINE()));
}


int CvMap::maxPlotDistance()
{
	return std::max(1, plotDistance(0, 0, ((isWrapXINLINE()) ? (getGridWidthINLINE() / 2) : (getGridWidthINLINE() - 1)), ((isWrapYINLINE()) ? (getGridHeightINLINE() / 2) : (getGridHeightINLINE() - 1))));
}


int CvMap::maxStepDistance()
{
	return std::max(1, stepDistance(0, 0, ((isWrapXINLINE()) ? (getGridWidthINLINE() / 2) : (getGridWidthINLINE() - 1)), ((isWrapYINLINE()) ? (getGridHeightINLINE() / 2) : (getGridHeightINLINE() - 1))));
}


int CvMap::getGridWidth() const
{
	return getGridWidthINLINE();
}


int CvMap::getGridHeight() const
{
	return getGridHeightINLINE();
}


int CvMap::getLandPlots()
{
	return m_iLandPlots;
}


void CvMap::changeLandPlots(int iChange)
{
	m_iLandPlots = (m_iLandPlots + iChange);
	FAssert(getLandPlots() >= 0);
}


int CvMap::getOwnedPlots()
{
	return m_iOwnedPlots;
}


void CvMap::changeOwnedPlots(int iChange)
{
	m_iOwnedPlots = (m_iOwnedPlots + iChange);
	FAssert(getOwnedPlots() >= 0);
}


int CvMap::getTopLatitude()
{
	return m_iTopLatitude;
}


int CvMap::getBottomLatitude()
{
	return m_iBottomLatitude;
}


int CvMap::getNextRiverID()
{
	return m_iNextRiverID;
}


void CvMap::incrementNextRiverID()
{
	m_iNextRiverID++;
}


bool CvMap::isWrapX()
{
	return isWrapXINLINE();
}


bool CvMap::isWrapY()
{
	return isWrapYINLINE();
}

bool CvMap::isWrap()
{
	return isWrapINLINE();
}

WorldSizeTypes CvMap::getWorldSize()
{
	return GC.getInitCore().getWorldSize();
}


ClimateTypes CvMap::getClimate()
{
	return GC.getInitCore().getClimate();
}


SeaLevelTypes CvMap::getSeaLevel()
{
	return GC.getInitCore().getSeaLevel();
}



int CvMap::getNumCustomMapOptions()
{
	return GC.getInitCore().getNumCustomMapOptions();
}


CustomMapOptionTypes CvMap::getCustomMapOption(int iOption)
{
	return GC.getInitCore().getCustomMapOption(iOption);
}


int CvMap::getNumBonuses(BonusTypes eIndex)													
{
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumBonusInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	return m_paiNumBonus[eIndex];
}


void CvMap::changeNumBonuses(BonusTypes eIndex, int iChange)									
{
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumBonusInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	m_paiNumBonus[eIndex] = (m_paiNumBonus[eIndex] + iChange);
	FAssert(getNumBonuses(eIndex) >= 0);
}


int CvMap::getNumBonusesOnLand(BonusTypes eIndex)													
{
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumBonusInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	return m_paiNumBonusOnLand[eIndex];
}


void CvMap::changeNumBonusesOnLand(BonusTypes eIndex, int iChange)									
{
	FAssertMsg(eIndex >= 0, "eIndex is expected to be non-negative (invalid Index)");
	FAssertMsg(eIndex < GC.getNumBonusInfos(), "eIndex is expected to be within maximum bounds (invalid Index)");
	m_paiNumBonusOnLand[eIndex] = (m_paiNumBonusOnLand[eIndex] + iChange);
	FAssert(getNumBonusesOnLand(eIndex) >= 0);
}


CvPlot* CvMap::plotByIndex(int iIndex) const
{
	return plotByIndexINLINE(iIndex);
}


CvPlot* CvMap::plot(int iX, int iY) const
{
	return plotINLINE(iX, iY);
}


CvPlot* CvMap::pointToPlot(float fX, float fY)													
{
	return plotINLINE(pointXToPlotX(fX), pointYToPlotY(fY));
}


int CvMap::getIndexAfterLastArea()																
{
	return m_areas.getIndexAfterLast();
}


int CvMap::getNumAreas()																		
{
	return m_areas.getCount();
}


int CvMap::getNumLandAreas()
{
	CvArea* pLoopArea;
	int iNumLandAreas;
	int iLoop;

	iNumLandAreas = 0;

	for(pLoopArea = GC.getMap().firstArea(&iLoop); pLoopArea != NULL; pLoopArea = GC.getMap().nextArea(&iLoop))
	{
		if (!(pLoopArea->isWater()))
		{
			iNumLandAreas++;
		}
	}

	return iNumLandAreas;
}


CvArea* CvMap::getArea(int iID)																
{
	return m_areas.getAt(iID);
}


CvArea* CvMap::addArea()
{
	return m_areas.add();
}


void CvMap::deleteArea(int iID)
{
	m_areas.removeAt(iID);
}


CvArea* CvMap::firstArea(int *pIterIdx, bool bRev)
{
	return !bRev ? m_areas.beginIter(pIterIdx) : m_areas.endIter(pIterIdx);
}


CvArea* CvMap::nextArea(int *pIterIdx, bool bRev)
{
	return !bRev ? m_areas.nextIter(pIterIdx) : m_areas.prevIter(pIterIdx);
}


void CvMap::recalculateAreas()
{
	PROFILE("CvMap::recalculateAreas");

	setFoundValuesAllDirty();	// city-site scores read which area a plot is in

	int iI;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		plotByIndexINLINE(iI)->setArea(FFreeList::INVALID_INDEX);
	}

	m_areas.removeAll();

	calculateAreas();
}


const std::vector<int>& CvMap::getAreaPlotIndices(int iArea) const
{
	if (m_bAreaPlotsDirty)
	{
		PROFILE("CvMap::getAreaPlotIndices: lists made");

		m_aaiAreaPlots.clear();

		for (int iI = 0; iI < numPlotsINLINE(); iI++)
		{
			m_aaiAreaPlots[m_pMapPlots[iI].getArea()].push_back(iI);
		}

		m_bAreaPlotsDirty = false;
	}

	static const std::vector<int> aiNone;
	std::map<int, std::vector<int> >::const_iterator it = m_aaiAreaPlots.find(iArea);
	return ((it != m_aaiAreaPlots.end()) ? it->second : aiNone);
}


void CvMap::resetPathDistance()
{
	gDLL->getFAStarIFace()->ForceReset(&GC.getStepFinder());
}


int CvMap::calculatePathDistance(CvPlot *pSource, CvPlot *pDest)
{
	FAStarNode* pNode;

	if (pSource == NULL || pDest == NULL)
	{
		return -1;
	}

	if (gDLL->getFAStarIFace()->GeneratePath(&GC.getStepFinder(), pSource->getX_INLINE(), pSource->getY_INLINE(), pDest->getX_INLINE(), pDest->getY_INLINE(), false, 0, true))
	{
		pNode = gDLL->getFAStarIFace()->GetLastNode(&GC.getStepFinder());

		if (pNode != NULL)
		{
			return pNode->m_iData1;
		}
	}

	return -1; // no passable path exists
}

//
// read object from a stream
// used during load
//
void CvMap::read(FDataStreamBase* pStream)
{
	CvMapInitData defaultMapData;

	// Init data before load
	reset(&defaultMapData);

	uint uiFlag=0;
	pStream->Read(&uiFlag);	// flags for expansion

	pStream->Read(&m_iGridWidth);
	pStream->Read(&m_iGridHeight);
	pStream->Read(&m_iLandPlots);
	pStream->Read(&m_iOwnedPlots);
	pStream->Read(&m_iTopLatitude);
	pStream->Read(&m_iBottomLatitude);
	pStream->Read(&m_iNextRiverID);

	pStream->Read(&m_bWrapX);
	pStream->Read(&m_bWrapY);

	FAssertMsg((0 < GC.getNumBonusInfos()), "GC.getNumBonusInfos() is not greater than zero but an array is being allocated");
	pStream->Read(GC.getNumBonusInfos(), m_paiNumBonus);
	pStream->Read(GC.getNumBonusInfos(), m_paiNumBonusOnLand);

	if (numPlotsINLINE() > 0)
	{
		m_pMapPlots = new CvPlot[numPlotsINLINE()];
		int iI;
		for (iI = 0; iI < numPlotsINLINE(); iI++)
		{
			m_pMapPlots[iI].read(pStream);
		}
	}

	// call the read of the free list CvArea class allocations
	ReadStreamableFFreeListTrashArray(m_areas, pStream);

	setup();
	// the plots' owners and unit lists were read without going through setOwner() and addUnit()
	m_bTeamIndexDirty = true;
	// and their areas and routes without going through setArea() and setRouteType()
	m_bAreaPlotsDirty = true;
	for (int iI = 0; iI < numPlotsINLINE(); iI++)
	{
		noteRoute(m_pMapPlots[iI].getRouteType());
	}
	setFoundValuesAllDirty();
}

// save object to a stream
// used during save
//
void CvMap::write(FDataStreamBase* pStream)
{
	uint uiFlag=0;
	pStream->Write(uiFlag);		// flag for expansion

	pStream->Write(m_iGridWidth);
	pStream->Write(m_iGridHeight);
	pStream->Write(m_iLandPlots);
	pStream->Write(m_iOwnedPlots);
	pStream->Write(m_iTopLatitude);
	pStream->Write(m_iBottomLatitude);
	pStream->Write(m_iNextRiverID);

	pStream->Write(m_bWrapX);
	pStream->Write(m_bWrapY);

	FAssertMsg((0 < GC.getNumBonusInfos()), "GC.getNumBonusInfos() is not greater than zero but an array is being allocated");
	pStream->Write(GC.getNumBonusInfos(), m_paiNumBonus);
	pStream->Write(GC.getNumBonusInfos(), m_paiNumBonusOnLand);

	int iI;	
	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		m_pMapPlots[iI].write(pStream);
	}

	// call the read of the free list CvArea class allocations
	WriteStreamableFFreeListTrashArray(m_areas, pStream);
}


//
// used for loading WB maps
//
void CvMap::rebuild(int iGridW, int iGridH, int iTopLatitude, int iBottomLatitude, bool bWrapX, bool bWrapY, WorldSizeTypes eWorldSize, ClimateTypes eClimate, SeaLevelTypes eSeaLevel, int iNumCustomMapOptions, CustomMapOptionTypes * aeCustomMapOptions)
{
	CvMapInitData initData(iGridW, iGridH, iTopLatitude, iBottomLatitude, bWrapX, bWrapY);

	// Set init core data
	GC.getInitCore().setWorldSize(eWorldSize);
	GC.getInitCore().setClimate(eClimate);
	GC.getInitCore().setSeaLevel(eSeaLevel);
	GC.getInitCore().setCustomMapOptions(iNumCustomMapOptions, aeCustomMapOptions);

	// Init map
	init(&initData);
}


//////////////////////////////////////////////////////////////////////////
// Protected Functions...
//////////////////////////////////////////////////////////////////////////

void CvMap::calculateAreas()
{
	PROFILE("CvMap::calculateAreas");
	CvPlot* pLoopPlot;
	CvArea* pArea;
	int iArea;
	int iI;

	for (iI = 0; iI < numPlotsINLINE(); iI++)
	{
		pLoopPlot = plotByIndexINLINE(iI);
		gDLL->callUpdater();
		FAssertMsg(pLoopPlot != NULL, "LoopPlot is not assigned a valid value");

		if (pLoopPlot->getArea() == FFreeList::INVALID_INDEX)
		{
			pArea = addArea();
			pArea->init(pArea->getID(), pLoopPlot->isWater());

			iArea = pArea->getID();

			pLoopPlot->setArea(iArea);

			gDLL->getFAStarIFace()->GeneratePath(&GC.getAreaFinder(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), -1, -1, pLoopPlot->isWater(), iArea);
		}
	}
}


// Private Functions...
//
// The team index.
//
// For each plot the map keeps words with a bit per team: the teams with a unit there that
// can attack, the same again by how far such a unit can reach, and the team that owns the
// plot. For each block of 8 x 8 plots it keeps the teams with an attacking unit and the
// teams owning a plot anywhere in the block.
//
// CvPlayerAI::AI_getPlotDanger() counts the enemy units in range that could attack a plot
// and adds the border danger. Nearly always both are nothing, and isTeamAttackerNear() and
// isTeamBorderNear() can say so from a few block words, or failing that from the plot
// words, without walking unit lists. They answer "no" only when the scan would find
// nothing; they may answer "yes" when it would still count nothing, since they do not test
// whether a unit is visible or can enter the plot.
//
// Only units that can attack are indexed. A unit of an always-hostile type also counts
// for the barbarian team: it is an enemy to every team but its own, and every team is at
// war with the barbarians. A unit's reach is its base moves, plus one if its plot has a
// route, which is at least what AI_getPlotDanger() allows it when it tests moves.
//
// CvPlot::addUnit(), removeUnit(), setOwner() and setRouteType() and
// CvUnit::changeExtraMoves() keep the index. Anything else that changes what a unit or
// plot counts as must call setTeamIndexDirty(), and the index is rebuilt at the next
// question: loading a game, a player changing team, a team's extra moves changing, and a
// unit gaining or losing the ability to attack.
//
// Blocks and plots do not depend on each other and a question only reads them, so a clean
// index can be read from several threads at once.
//
//
// The dirty bits for the city-site scores the players keep: see
// CvPlayerAI::AI_refreshFoundValues(), which lists what a score is worked out from and which
// call here covers each input.
//
// A plot has two bits for each player. The first says that something the plot's score reads
// may have changed, so it is scored again. The second says that only something its count of
// deadlocked bonuses reads may have changed, which reaches much further than the rest of the
// score; then the count is taken again and the plot is scored only if it differs.
//
// The bits exist once a player has asked for them, and they start out all set.
//
void CvMap::ensureFoundValueDirtyBits()
{
	FAssert(MAX_PLAYERS <= 64);	// one bit per player

	if ((int)m_aiFoundScoreDirty.size() != numPlotsINLINE())
	{
		m_aiFoundScoreDirty.assign(numPlotsINLINE(), ~0ULL);
		m_aiFoundDeadlockDirty.assign(numPlotsINLINE(), ~0ULL);
	}
}


// Every plot, for every player.
void CvMap::setFoundValuesAllDirty()
{
	m_aiFoundScoreDirty.clear();
	m_aiFoundDeadlockDirty.clear();
}


void CvMap::setFoundValuesDirty(PlayerTypes ePlayer)
{
	const unsigned long long uiPlayer = (1ULL << ePlayer);

	for (size_t i = 0; i < m_aiFoundScoreDirty.size(); i++)
	{
		m_aiFoundScoreDirty[i] |= uiPlayer;
	}
}


void CvMap::setFoundValuesDirtyInArea(PlayerTypes ePlayer, int iArea)
{
	const unsigned long long uiPlayer = (1ULL << ePlayer);

	for (size_t i = 0; i < m_aiFoundScoreDirty.size(); i++)
	{
		if (m_pMapPlots[i].getArea() == iArea)
		{
			m_aiFoundScoreDirty[i] |= uiPlayer;
		}
	}
}


//
// Something about pPlot has changed that the score of a plot within iScoreRange reads, or
// that the deadlocked-bonus count of a plot within iDeadlockRange reads (-1 for neither).
//
void CvMap::foundValuesChanged(const CvPlot* pPlot, int iScoreRange, int iDeadlockRange)
{
	if (m_aiFoundScoreDirty.empty())
	{
		return;	// nobody keeps scores yet
	}

	const int iRange = std::max(iScoreRange, iDeadlockRange);

	if ((iRange < 0) || (iRange > TEAM_INDEX_MAX_RANGE))
	{
		FAssert(iRange < 0);
		return;
	}

	int aiMapX[(2 * TEAM_INDEX_MAX_RANGE) + 1];
	int aiRowStart[(2 * TEAM_INDEX_MAX_RANGE) + 1];
	getWindowCoords(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iRange, aiMapX, aiRowStart);

	const int iWidth = (2 * iRange) + 1;

	for (int iRow = 0; iRow < iWidth; iRow++)
	{
		if (aiRowStart[iRow] < 0)
		{
			continue;
		}

		const int iRowDistance = abs(iRow - iRange);

		for (int iColumn = 0; iColumn < iWidth; iColumn++)
		{
			if (aiMapX[iColumn] < 0)
			{
				continue;
			}

			const int iPlot = aiRowStart[iRow] + aiMapX[iColumn];
			const int iDistance = std::max(iRowDistance, abs(iColumn - iRange));

			if (iDistance <= iScoreRange)
			{
				m_aiFoundScoreDirty[iPlot] = ~0ULL;
			}

			if (iDistance <= iDeadlockRange)
			{
				m_aiFoundDeadlockDirty[iPlot] = ~0ULL;
			}
		}
	}
}


void CvMap::setTeamIndexDirty()
{
	m_bTeamIndexDirty = true;
}


// The teams with a unit on the plot that can attack; puiReachTeams gets TEAM_REACH_WORDS words.
unsigned long long CvMap::calculatePlotUnitTeams(const CvPlot* pPlot, unsigned long long* puiReachTeams) const
{
	unsigned long long uiTeams = 0;
	const int iRouteReach = (pPlot->isRoute() ? 1 : 0);
	CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();

	for (int iI = 0; iI < TEAM_REACH_WORDS; iI++)
	{
		puiReachTeams[iI] = 0;
	}

	while (pUnitNode != NULL)
	{
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = pPlot->nextUnitNode(pUnitNode);

		if ((pLoopUnit != NULL) && pLoopUnit->canAttack())
		{
			unsigned long long uiUnitTeams = (1ULL << pLoopUnit->getTeam());

			if (pLoopUnit->getUnitInfo().isAlwaysHostile())
			{
				uiUnitTeams |= (1ULL << BARBARIAN_TEAM);
			}

			uiTeams |= uiUnitTeams;

			const int iReach = std::min((int)TEAM_REACH_WORDS, pLoopUnit->baseMoves() + iRouteReach);

			for (int iI = 0; iI < iReach; iI++)
			{
				puiReachTeams[iI] |= uiUnitTeams;
			}
		}
	}

	return uiTeams;
}


//
// Every plot eTeam owns, found from the blocks the team index says it owns plots in rather
// than by looking at the whole map. The order is by block, not by plot index.
//
void CvMap::getTeamOwnedPlots(TeamTypes eTeam, std::vector<CvPlot*>& apPlots) const
{
	apPlots.clear();

	if ((eTeam < 0) || (eTeam >= MAX_TEAMS))
	{
		return;
	}

	if (m_bTeamIndexDirty)
	{
		rebuildTeamIndex();
	}

	const unsigned long long uiTeam = (1ULL << eTeam);
	const int iBlockSize = (1 << TEAM_BLOCK_SHIFT);
	const int iGridWidth = getGridWidthINLINE();
	const int iGridHeight = getGridHeightINLINE();
	const int iNumBlocks = (int)m_aiBlockOwnerTeams.size();

	for (int iBlock = 0; iBlock < iNumBlocks; iBlock++)
	{
		if ((m_aiBlockOwnerTeams[iBlock] & uiTeam) == 0)
		{
			continue;
		}

		const int iStartX = (iBlock % m_iTeamBlocksWide) * iBlockSize;
		const int iStartY = (iBlock / m_iTeamBlocksWide) * iBlockSize;
		const int iEndX = std::min(iStartX + iBlockSize, iGridWidth);
		const int iEndY = std::min(iStartY + iBlockSize, iGridHeight);

		for (int iY = iStartY; iY < iEndY; iY++)
		{
			CvPlot* pRow = &m_pMapPlots[iY * iGridWidth];

			for (int iX = iStartX; iX < iEndX; iX++)
			{
				if (pRow[iX].getTeam() == eTeam)
				{
					apPlots.push_back(&pRow[iX]);
				}
			}
		}
	}
}


void CvMap::rebuildTeamIndex() const
{
	FAssert(MAX_TEAMS <= 64);	// one bit per team

	m_iTeamBlocksWide = (getGridWidthINLINE() + (1 << TEAM_BLOCK_SHIFT) - 1) >> TEAM_BLOCK_SHIFT;
	const int iBlocksHigh = (getGridHeightINLINE() + (1 << TEAM_BLOCK_SHIFT) - 1) >> TEAM_BLOCK_SHIFT;
	const int iNumBlocks = m_iTeamBlocksWide * iBlocksHigh;

	m_aiBlockTeamUnits.assign(iNumBlocks * MAX_TEAMS, 0);
	m_aiBlockTeamOwnedPlots.assign(iNumBlocks * MAX_TEAMS, 0);
	m_aiBlockUnitTeams.assign(iNumBlocks, 0);
	m_aiBlockOwnerTeams.assign(iNumBlocks, 0);
	m_aiPlotUnitTeams.assign(numPlotsINLINE(), 0);
	m_aiPlotOwnerTeams.assign(numPlotsINLINE(), 0);
	m_aiPlotReachTeams.assign(numPlotsINLINE() * TEAM_REACH_WORDS, 0);

	for (int iI = 0; iI < numPlotsINLINE(); iI++)
	{
		CvPlot* pLoopPlot = plotByIndexINLINE(iI);
		const int iBlock = ((pLoopPlot->getY_INLINE() >> TEAM_BLOCK_SHIFT) * m_iTeamBlocksWide) + (pLoopPlot->getX_INLINE() >> TEAM_BLOCK_SHIFT);

		if (pLoopPlot->isOwned())
		{
			m_aiPlotOwnerTeams[iI] = (1ULL << pLoopPlot->getTeam());
			m_aiBlockTeamOwnedPlots[(iBlock * MAX_TEAMS) + pLoopPlot->getTeam()]++;
		}

		m_aiPlotUnitTeams[iI] = calculatePlotUnitTeams(pLoopPlot, &m_aiPlotReachTeams[iI * TEAM_REACH_WORDS]);

		CLLNode<IDInfo>* pUnitNode = pLoopPlot->headUnitNode();

		while (pUnitNode != NULL)
		{
			CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
			pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

			if ((pLoopUnit != NULL) && pLoopUnit->canAttack())
			{
				m_aiBlockTeamUnits[(iBlock * MAX_TEAMS) + pLoopUnit->getTeam()]++;

				if (pLoopUnit->getUnitInfo().isAlwaysHostile())
				{
					m_aiBlockTeamUnits[(iBlock * MAX_TEAMS) + BARBARIAN_TEAM]++;
				}
			}
		}
	}

	for (int iBlock = 0; iBlock < iNumBlocks; iBlock++)
	{
		for (int iTeam = 0; iTeam < MAX_TEAMS; iTeam++)
		{
			if (m_aiBlockTeamUnits[(iBlock * MAX_TEAMS) + iTeam] > 0)
			{
				m_aiBlockUnitTeams[iBlock] |= (1ULL << iTeam);
			}

			if (m_aiBlockTeamOwnedPlots[(iBlock * MAX_TEAMS) + iTeam] > 0)
			{
				m_aiBlockOwnerTeams[iBlock] |= (1ULL << iTeam);
			}
		}
	}

	m_bTeamIndexDirty = false;
}


// after the plot's owner, route or unit list has changed, or a unit on it has changed its moves
void CvMap::updatePlotTeams(const CvPlot* pPlot)
{
	if (!m_bTeamIndexDirty)
	{
		const int iPlot = plotNumINLINE(pPlot->getX_INLINE(), pPlot->getY_INLINE());

		m_aiPlotUnitTeams[iPlot] = calculatePlotUnitTeams(pPlot, &m_aiPlotReachTeams[iPlot * TEAM_REACH_WORDS]);
		m_aiPlotOwnerTeams[iPlot] = (pPlot->isOwned() ? (1ULL << pPlot->getTeam()) : 0);
	}
}


void CvMap::changeBlockTeamCount(std::vector<unsigned short>& aiCounts, std::vector<unsigned long long>& aiBlockTeams, int iX, int iY, TeamTypes eTeam, int iChange)
{
	if (m_bTeamIndexDirty || (eTeam == NO_TEAM))
	{
		return;	// nothing to keep up to date: the index will be rebuilt
	}

	const int iBlock = ((iY >> TEAM_BLOCK_SHIFT) * m_iTeamBlocksWide) + (iX >> TEAM_BLOCK_SHIFT);
	const int iIndex = (iBlock * MAX_TEAMS) + eTeam;
	const int iNewCount = aiCounts[iIndex] + iChange;

	if ((iNewCount < 0) || (iNewCount > MAX_UNSIGNED_SHORT))
	{
		// a count that cannot be right means a change went past the hooks; start again
		FAssertMsg(false, "team index count out of range");
		m_bTeamIndexDirty = true;
		return;
	}

	aiCounts[iIndex] = (unsigned short)iNewCount;

	if (iNewCount > 0)
	{
		aiBlockTeams[iBlock] |= (1ULL << eTeam);
	}
	else
	{
		aiBlockTeams[iBlock] &= ~(1ULL << eTeam);
	}
}


void CvMap::changeBlockUnits(int iX, int iY, TeamTypes eTeam, int iChange)
{
	changeBlockTeamCount(m_aiBlockTeamUnits, m_aiBlockUnitTeams, iX, iY, eTeam, iChange);
}


void CvMap::changeBlockOwnedPlots(int iX, int iY, TeamTypes eTeam, int iChange)
{
	changeBlockTeamCount(m_aiBlockTeamOwnedPlots, m_aiBlockOwnerTeams, iX, iY, eTeam, iChange);
}


//
// Whether any block touched by the plots within iRange of (iX, iY), counting range the way
// plotXY() does with wrap included, has a bit of uiTeams set.
//
bool CvMap::isTeamInBlocks(const std::vector<unsigned long long>& aiBlockTeams, int iX, int iY, int iRange, unsigned long long uiTeams) const
{
	const int iBlockSize = (1 << TEAM_BLOCK_SHIFT);
	int aiColumns[8];
	int aiRows[8];
	int iNumColumns = 0;
	int iNumRows = 0;

	if (((2 * iRange) + 1) > (6 * iBlockSize))
	{
		return true;	// a window this wide is not worth ruling out block by block
	}

	// One sample every block width, and the far edge, lands in every block the window touches.
	for (int iD = -iRange; ; iD += iBlockSize)
	{
		iD = std::min(iD, iRange);

		const int iMapX = coordRange(iX + iD, getGridWidthINLINE(), isWrapXINLINE());
		if ((iMapX >= 0) && (iMapX < getGridWidthINLINE()))
		{
			aiColumns[iNumColumns++] = (iMapX >> TEAM_BLOCK_SHIFT);
		}

		const int iMapY = coordRange(iY + iD, getGridHeightINLINE(), isWrapYINLINE());
		if ((iMapY >= 0) && (iMapY < getGridHeightINLINE()))
		{
			aiRows[iNumRows++] = (iMapY >> TEAM_BLOCK_SHIFT);
		}

		if (iD == iRange)
		{
			break;
		}
	}

	for (int iRow = 0; iRow < iNumRows; iRow++)
	{
		for (int iColumn = 0; iColumn < iNumColumns; iColumn++)
		{
			if ((aiBlockTeams[(aiRows[iRow] * m_iTeamBlocksWide) + aiColumns[iColumn]] & uiTeams) != 0)
			{
				return true;
			}
		}
	}

	return false;
}


//
// For a window of plots within iRange of (iX, iY): the map column of each column of the
// window and the index of the first plot of each row, or -1 where the window runs off a
// map edge that does not wrap. Both arrays need (2 * iRange) + 1 entries.
//
void CvMap::getWindowCoords(int iX, int iY, int iRange, int* aiMapX, int* aiRowStart) const
{
	const int iGridWidth = getGridWidthINLINE();
	const int iGridHeight = getGridHeightINLINE();

	for (int iD = -iRange; iD <= iRange; iD++)
	{
		const int iMapX = coordRange(iX + iD, iGridWidth, isWrapXINLINE());
		aiMapX[iD + iRange] = (((iMapX >= 0) && (iMapX < iGridWidth)) ? iMapX : -1);

		const int iMapY = coordRange(iY + iD, iGridHeight, isWrapYINLINE());
		aiRowStart[iD + iRange] = (((iMapY >= 0) && (iMapY < iGridHeight)) ? (iMapY * iGridWidth) : -1);
	}
}


//
// False only if AI_getPlotDanger() would count no unit for this plot and range, taking
// uiTeams (a bit per team) as the enemy teams: no unit of theirs that can attack stands in
// the plot's area within iRange and, when bTestMoves, close enough for its reach.
//
// A unit on the plot itself never counts, since it cannot move into the plot it is on.
//
bool CvMap::isTeamAttackerNear(const CvPlot* pPlot, int iRange, unsigned long long uiTeams, bool bTestMoves) const
{
#ifdef BRIDGED_PROFILE
	static BridgedProfileSample aSamples[3] = {
		{ "team index: no attacker in the blocks" }, { "team index: no attacker that counts on the plots" }, { "team index: attacker in reach" } };
#endif

	if ((uiTeams == 0) || (iRange <= 0))
	{
		return false;
	}

	if (iRange > TEAM_INDEX_MAX_RANGE)
	{
		return true;
	}

	if (m_bTeamIndexDirty)
	{
		rebuildTeamIndex();
	}

	if (!isTeamInBlocks(m_aiBlockUnitTeams, pPlot->getX_INLINE(), pPlot->getY_INLINE(), iRange, uiTeams))
	{
#ifdef BRIDGED_PROFILE
		bridgedProfileAdd(&aSamples[0], 0);
#endif
		return false;
	}

	int aiMapX[(2 * TEAM_INDEX_MAX_RANGE) + 1];
	int aiRowStart[(2 * TEAM_INDEX_MAX_RANGE) + 1];
	getWindowCoords(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iRange, aiMapX, aiRowStart);

	const int iArea = pPlot->getArea();
	const int iWidth = (2 * iRange) + 1;

	for (int iRow = 0; iRow < iWidth; iRow++)
	{
		if (aiRowStart[iRow] < 0)
		{
			continue;
		}

		const int iRowDistance = abs(iRow - iRange);

		for (int iColumn = 0; iColumn < iWidth; iColumn++)
		{
			if (aiMapX[iColumn] < 0)
			{
				continue;
			}

			const int iPlot = aiRowStart[iRow] + aiMapX[iColumn];
			const int iDistance = std::max(iRowDistance, abs(iColumn - iRange));
			unsigned long long uiPlotTeams;

			if (bTestMoves)
			{
				// a unit this many plots away counts only if its reach covers the distance
				uiPlotTeams = ((iDistance > 0) ? m_aiPlotReachTeams[(iPlot * TEAM_REACH_WORDS) + std::min(iDistance, (int)TEAM_REACH_WORDS) - 1] : 0);
			}
			else
			{
				uiPlotTeams = ((iDistance > 0) ? m_aiPlotUnitTeams[iPlot] : 0);
			}

			if (((uiPlotTeams & uiTeams) != 0) && (m_pMapPlots[iPlot].getArea() == iArea))
			{
#ifdef BRIDGED_PROFILE
				bridgedProfileAdd(&aSamples[2], 0);
#endif
				return true;
			}
		}
	}

#ifdef BRIDGED_PROFILE
	bridgedProfileAdd(&aSamples[1], 0);
#endif
	return false;
}


//
// Whether AI_getPlotDanger() would find border danger for this plot within iRange, taking
// uiTeams (a bit per team) as the enemy teams: a plot of theirs in the same area one plot
// away, or two plots away with a route on it.
//
bool CvMap::isTeamBorderNear(const CvPlot* pPlot, int iRange, unsigned long long uiTeams) const
{
	iRange = std::min(iRange, 2);

	if ((uiTeams == 0) || (iRange <= 0))
	{
		return false;
	}

	if (m_bTeamIndexDirty)
	{
		rebuildTeamIndex();
	}

	if (!isTeamInBlocks(m_aiBlockOwnerTeams, pPlot->getX_INLINE(), pPlot->getY_INLINE(), iRange, uiTeams))
	{
		return false;
	}

	int aiMapX[5];
	int aiRowStart[5];
	getWindowCoords(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iRange, aiMapX, aiRowStart);

	const int iArea = pPlot->getArea();
	const int iWidth = (2 * iRange) + 1;

	for (int iRow = 0; iRow < iWidth; iRow++)
	{
		if (aiRowStart[iRow] < 0)
		{
			continue;
		}

		const int iRowDistance = abs(iRow - iRange);

		for (int iColumn = 0; iColumn < iWidth; iColumn++)
		{
			if (aiMapX[iColumn] < 0)
			{
				continue;
			}

			const int iPlot = aiRowStart[iRow] + aiMapX[iColumn];

			if ((m_aiPlotOwnerTeams[iPlot] & uiTeams) != 0)
			{
				const int iDistance = std::max(iRowDistance, abs(iColumn - iRange));
				const CvPlot& kLoopPlot = m_pMapPlots[iPlot];

				if ((iDistance > 0) && (kLoopPlot.getArea() == iArea) && ((iDistance == 1) || kLoopPlot.isRoute()))
				{
					return true;
				}
			}
		}
	}

	return false;
}
