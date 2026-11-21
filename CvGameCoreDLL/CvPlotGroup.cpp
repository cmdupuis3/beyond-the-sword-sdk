// plotGroup.cpp

#include "CvGameCoreDLL.h"
#include "CvPlotGroup.h"
#include "CvPlot.h"
#include "CvGlobals.h"
#include "CvPlayerAI.h"
#include "CvMap.h"
#include "CvCity.h"
#include "CvDLLFAStarIFaceBase.h"
#include "FProfiler.h"
#include "CvGameCoreUtils.h"
#include <algorithm>

//
// How many plots of the group can be reached from pStart along the group's own trade
// connections: what recalculatePlots() asked the exe's plot-group finder for (a flood
// with plotGroupValid() as the step test and countPlotGroup() counting closed nodes).
// The same flood, done here: each plot is counted once, the first time a step into it
// from a counted plot passes plotGroupValid()'s tests.
//
// PLOT_GROUP_VERIFY: a global define for profile builds. Set, the exe's flood runs as
// well and the counts that differ are counted.
//
namespace
{
	std::vector<int> s_aiFloodStamp;
	std::vector<CvPlot*> s_apFloodQueue;
	int s_iFloodStamp = 0;

	int countConnectedPlots(CvPlot* pStart, PlayerTypes eOwner)
	{
		CvMap& kMap = GC.getMapINLINE();
		const TeamTypes eTeam = GET_PLAYER(eOwner).getTeam();
		const CvPlotGroup* pGroup = pStart->getPlotGroup(eOwner);

		if ((int)s_aiFloodStamp.size() != kMap.numPlotsINLINE())
		{
			s_aiFloodStamp.assign(kMap.numPlotsINLINE(), 0);
			s_iFloodStamp = 0;
		}

		if (++s_iFloodStamp == MAX_INT)
		{
			std::fill(s_aiFloodStamp.begin(), s_aiFloodStamp.end(), 0);
			s_iFloodStamp = 1;
		}

		s_apFloodQueue.clear();
		s_apFloodQueue.push_back(pStart);
		s_aiFloodStamp[kMap.plotNumINLINE(pStart->getX_INLINE(), pStart->getY_INLINE())] = s_iFloodStamp;

		for (size_t iNext = 0; iNext < s_apFloodQueue.size(); iNext++)
		{
			CvPlot* pPlot = s_apFloodQueue[iNext];

			for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
			{
				CvPlot* pAdjacentPlot = plotDirection(pPlot->getX_INLINE(), pPlot->getY_INLINE(), ((DirectionTypes)iI));

				if (pAdjacentPlot == NULL)
				{
					continue;
				}

				int& iStamp = s_aiFloodStamp[kMap.plotNumINLINE(pAdjacentPlot->getX_INLINE(), pAdjacentPlot->getY_INLINE())];

				if (iStamp == s_iFloodStamp)
				{
					continue;
				}

				// plotGroupValid(pPlot, pAdjacentPlot); pPlot is in the group, so the first
				// test is that pAdjacentPlot is too
				if ((pAdjacentPlot->getPlotGroup(eOwner) == pGroup) && pAdjacentPlot->isTradeNetwork(eTeam) && pAdjacentPlot->isTradeNetworkConnected(pPlot, eTeam))
				{
					iStamp = s_iFloodStamp;
					s_apFloodQueue.push_back(pAdjacentPlot);
				}
			}
		}

		return (int)s_apFloodQueue.size();
	}
}

// Public Functions...

CvPlotGroup::CvPlotGroup()
{
	m_paiNumBonuses = NULL;

	reset(0, NO_PLAYER, true);
}


CvPlotGroup::~CvPlotGroup()
{
	uninit();
}


void CvPlotGroup::init(int iID, PlayerTypes eOwner, CvPlot* pPlot)
{
	//--------------------------------
	// Init saved data
	reset(iID, eOwner);

	//--------------------------------
	// Init non-saved data

	//--------------------------------
	// Init other game data
	addPlot(pPlot);
}


void CvPlotGroup::uninit()
{
	SAFE_DELETE_ARRAY(m_paiNumBonuses);

	m_plots.clear();
}

// FUNCTION: reset()
// Initializes data members that are serialized.
void CvPlotGroup::reset(int iID, PlayerTypes eOwner, bool bConstructorCall)
{
	int iI;

	//--------------------------------
	// Uninit class
	uninit();

	m_iID = iID;
	m_eOwner = eOwner;

	if (!bConstructorCall)
	{
		FAssertMsg((0 < GC.getNumBonusInfos()), "GC.getNumBonusInfos() is not greater than zero but an array is being allocated in CvPlotGroup::reset");
		m_paiNumBonuses = new int [GC.getNumBonusInfos()];
		for (iI = 0; iI < GC.getNumBonusInfos(); iI++)
		{
			m_paiNumBonuses[iI] = 0;
		}
	}
}


void CvPlotGroup::addPlot(CvPlot* pPlot)
{
	XYCoords xy;

	xy.iX = pPlot->getX_INLINE();
	xy.iY = pPlot->getY_INLINE();

	insertAtEndPlots(xy);

	pPlot->setPlotGroup(getOwnerINLINE(), this);
}


void CvPlotGroup::removePlot(CvPlot* pPlot)
{
	CLLNode<XYCoords>* pPlotNode;

	pPlotNode = headPlotsNode();

	while (pPlotNode != NULL)
	{
		if (GC.getMapINLINE().plotSorenINLINE(pPlotNode->m_data.iX, pPlotNode->m_data.iY) == pPlot)
		{
			pPlot->setPlotGroup(getOwnerINLINE(), NULL);

			pPlotNode = deletePlotsNode(pPlotNode); // can delete this PlotGroup...
			break;
		}
		else
		{
			pPlotNode = nextPlotsNode(pPlotNode);
		}
	}
}


void CvPlotGroup::recalculatePlots()
{
	PROFILE_FUNC();

	CLLNode<XYCoords>* pPlotNode;
	CvPlot* pPlot;
	CLinkList<XYCoords> oldPlotGroup;
	XYCoords xy;
	PlayerTypes eOwner;
	int iCount;

	eOwner = getOwnerINLINE();

	pPlotNode = headPlotsNode();

	if (pPlotNode != NULL)
	{
		pPlot = GC.getMapINLINE().plotSorenINLINE(pPlotNode->m_data.iX, pPlotNode->m_data.iY);

		iCount = countConnectedPlots(pPlot, eOwner);	// see above

#ifdef BRIDGED_PROFILE
		static BridgedProfileSample aSamples[3] = {
			{ "plot group: still connected" }, { "plot group: split, built again" }, { "plot group verify: FLOOD COUNT DIFFERS" } };
		static int iVerify = -1;
		if (iVerify == -1)
		{
			iVerify = (GC.getDefineINT("PLOT_GROUP_VERIFY") != 0) ? 1 : 0;
		}
		if (iVerify != 0)
		{
			int iExeCount = 0;
			gDLL->getFAStarIFace()->SetData(&GC.getPlotGroupFinder(), &iExeCount);
			gDLL->getFAStarIFace()->GeneratePath(&GC.getPlotGroupFinder(), pPlot->getX_INLINE(), pPlot->getY_INLINE(), -1, -1, false, eOwner);
			if (iExeCount != iCount)
			{
				bridgedProfileAdd(&aSamples[2], 0);
			}
		}
		bridgedProfileAdd(&aSamples[(iCount == getLengthPlots()) ? 0 : 1], 0);
#endif

		if (iCount == getLengthPlots())
		{
			return;
		}
	}

	oldPlotGroup.clear();

	pPlotNode = headPlotsNode();

	while (pPlotNode != NULL)
	{
		pPlot = GC.getMapINLINE().plotSorenINLINE(pPlotNode->m_data.iX, pPlotNode->m_data.iY);

		FAssertMsg(pPlot != NULL, "Plot is not assigned a valid value");

		xy.iX = pPlot->getX_INLINE();
		xy.iY = pPlot->getY_INLINE();

		oldPlotGroup.insertAtEnd(xy);

		pPlot->setPlotGroup(eOwner, NULL);

		pPlotNode = deletePlotsNode(pPlotNode); // will delete this PlotGroup...
	}

	pPlotNode = oldPlotGroup.head();

	while (pPlotNode != NULL)
	{
		pPlot = GC.getMapINLINE().plotSorenINLINE(pPlotNode->m_data.iX, pPlotNode->m_data.iY);

		FAssertMsg(pPlot != NULL, "Plot is not assigned a valid value");

		pPlot->updatePlotGroup(eOwner, true);

		pPlotNode = oldPlotGroup.deleteNode(pPlotNode);
	}
}


int CvPlotGroup::getID() const
{
	return m_iID;
}


void CvPlotGroup::setID(int iID)
{
	m_iID = iID;
}


PlayerTypes CvPlotGroup::getOwner() const
{
	return getOwnerINLINE();
}


int CvPlotGroup::getNumBonuses(BonusTypes eBonus) const
{
	FAssertMsg(eBonus >= 0, "eBonus is expected to be non-negative (invalid Index)");
	FAssertMsg(eBonus < GC.getNumBonusInfos(), "eBonus is expected to be within maximum bounds (invalid Index)");
	return m_paiNumBonuses[eBonus];
}


bool CvPlotGroup::hasBonus(BonusTypes eBonus)
{
	return(getNumBonuses(eBonus) > 0);
}


void CvPlotGroup::changeNumBonuses(BonusTypes eBonus, int iChange)
{
	CLLNode<XYCoords>* pPlotNode;
	CvCity* pCity;
	int iOldNumBonuses;

	FAssertMsg(eBonus >= 0, "eBonus is expected to be non-negative (invalid Index)");
	FAssertMsg(eBonus < GC.getNumBonusInfos(), "eBonus is expected to be within maximum bounds (invalid Index)");

	if (iChange != 0)
	{
		iOldNumBonuses = getNumBonuses(eBonus);

		m_paiNumBonuses[eBonus] = (m_paiNumBonuses[eBonus] + iChange);

		//FAssertMsg(m_paiNumBonuses[eBonus] >= 0, "m_paiNumBonuses[eBonus] is expected to be non-negative (invalid Index)"); XXX

		pPlotNode = headPlotsNode();

		while (pPlotNode != NULL)
		{
			pCity = GC.getMapINLINE().plotSorenINLINE(pPlotNode->m_data.iX, pPlotNode->m_data.iY)->getPlotCity();

			if (pCity != NULL)
			{
				if (pCity->getOwnerINLINE() == getOwnerINLINE())
				{
					pCity->changeNumBonuses(eBonus, iChange);
				}
			}

			pPlotNode = nextPlotsNode(pPlotNode);
		}
	}
}


void CvPlotGroup::insertAtEndPlots(XYCoords xy)
{
	m_plots.insertAtEnd(xy);
}


CLLNode<XYCoords>* CvPlotGroup::deletePlotsNode(CLLNode<XYCoords>* pNode)
{
	CLLNode<XYCoords>* pPlotNode;

	pPlotNode = m_plots.deleteNode(pNode);

	if (getLengthPlots() == 0)
	{
		GET_PLAYER(getOwnerINLINE()).deletePlotGroup(getID());
	}

  return pPlotNode;
}


CLLNode<XYCoords>* CvPlotGroup::nextPlotsNode(CLLNode<XYCoords>* pNode)
{
	return m_plots.next(pNode);
}


int CvPlotGroup::getLengthPlots()
{
	return m_plots.getLength();
}


CLLNode<XYCoords>* CvPlotGroup::headPlotsNode()
{
	return m_plots.head();
}


void CvPlotGroup::read(FDataStreamBase* pStream)
{
	// Init saved data
	reset();

	uint uiFlag=0;
	pStream->Read(&uiFlag);	// flags for expansion

	pStream->Read(&m_iID);

	pStream->Read((int*)&m_eOwner);

	FAssertMsg((0 < GC.getNumBonusInfos()), "GC.getNumBonusInfos() is not greater than zero but an array is being allocated in CvPlotGroup::read");
	pStream->Read(GC.getNumBonusInfos(), m_paiNumBonuses);

	m_plots.Read(pStream);
}


void CvPlotGroup::write(FDataStreamBase* pStream)
{
	uint uiFlag=0;
	pStream->Write(uiFlag);		// flag for expansion

	pStream->Write(m_iID);

	pStream->Write(m_eOwner);

	FAssertMsg((0 < GC.getNumBonusInfos()), "GC.getNumBonusInfos() is not greater than zero but an array is being allocated in CvPlotGroup::write");
	pStream->Write(GC.getNumBonusInfos(), m_paiNumBonuses);

	m_plots.Write(pStream);
}
