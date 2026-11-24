// selectionGroupAI.cpp

#include "CvGameCoreDLL.h"
#include "CvSelectionGroupAI.h"
#include "CvPlayerAI.h"
#include "CvUnitAI.h"
#include "CvMap.h"
#include "CvPlot.h"
#include "CvTeamAI.h"
#include "CvDLLEntityIFaceBase.h"
#include "CvGameCoreUtils.h"
#include "FProfiler.h"
#include "CVInfos.h"

// Public Functions...

CvSelectionGroupAI::CvSelectionGroupAI()
{
	AI_reset();
}


CvSelectionGroupAI::~CvSelectionGroupAI()
{
	AI_uninit();
}


void CvSelectionGroupAI::AI_init()
{
	AI_reset();

	//--------------------------------
	// Init other game data
}


void CvSelectionGroupAI::AI_uninit()
{
}


void CvSelectionGroupAI::AI_reset()
{
	AI_uninit();

	m_iMissionAIX = INVALID_PLOT_COORD;
	m_iMissionAIY = INVALID_PLOT_COORD;

	m_bForceSeparate = false;

	m_eMissionAIType = NO_MISSIONAI;

	m_missionAIUnit.reset();

	m_bGroupAttack = false;
	m_iGroupAttackX = -1;
	m_iGroupAttackY = -1;
}


void CvSelectionGroupAI::AI_separate()
{
	CLLNode<IDInfo>* pEntityNode;
	CvUnit* pLoopUnit;

	pEntityNode = headUnitNode();

	while (pEntityNode != NULL)
	{
		pLoopUnit = ::getUnit(pEntityNode->m_data);
		pEntityNode = nextUnitNode(pEntityNode);

		pLoopUnit->joinGroup(NULL);
		if (pLoopUnit->plot()->getTeam() == getTeam())
		{
			pLoopUnit->getGroup()->pushMission(MISSION_SKIP);
		}
	}
}

void CvSelectionGroupAI::AI_seperateNonAI(UnitAITypes eUnitAI)
{
	CLLNode<IDInfo>* pEntityNode;
	CvUnit* pLoopUnit;

	pEntityNode = headUnitNode();

	while (pEntityNode != NULL)
	{
		pLoopUnit = ::getUnit(pEntityNode->m_data);
		pEntityNode = nextUnitNode(pEntityNode);
		if (pLoopUnit->AI_getUnitAIType() != eUnitAI)
		{
			pLoopUnit->joinGroup(NULL);
			if (pLoopUnit->plot()->getTeam() == getTeam())
			{
				pLoopUnit->getGroup()->pushMission(MISSION_SKIP);
			}
		}
	}
}

void CvSelectionGroupAI::AI_seperateAI(UnitAITypes eUnitAI)
{
	CLLNode<IDInfo>* pEntityNode;
	CvUnit* pLoopUnit;

	pEntityNode = headUnitNode();

	while (pEntityNode != NULL)
	{
		pLoopUnit = ::getUnit(pEntityNode->m_data);
		pEntityNode = nextUnitNode(pEntityNode);
		if (pLoopUnit->AI_getUnitAIType() == eUnitAI)
		{
			pLoopUnit->joinGroup(NULL);
			if (plot()->getTeam() == getTeam())
			{
				pLoopUnit->getGroup()->pushMission(MISSION_SKIP);
			}
		}
	}
}
// Returns true if the group has become busy...
#ifdef BRIDGED_PROFILE
#include <map>
#include <stdio.h>

//
// A probe for profile builds: what each call of AI_update() for a group ready to move ended
// in, with the time it took, by where the owner's team stands (CvTeam::getPeaceState()) and
// by whether an enemy unit that can attack is within four plots. It changes nothing.
//
// The outcomes, judged by the group's head unit:
//   gone            the unit no longer exists (it died, founded a city, was used up...)
//   moved           it is on another plot
//   stayed, working it is on the same plot and its group has a mission queued (a build, say)
//   stayed, acted   same plot, no mission, but it spent moves, took damage or attacked
//   stayed idle     same plot, no mission, nothing spent; "as last turn" when the same unit
//                   also stayed idle on this plot on the game turn before
//
static bool peaceProbeUpdate(CvSelectionGroupAI* pGroup)
{
	enum { OUTCOME_GONE, OUTCOME_MOVED, OUTCOME_WORKING, OUTCOME_ACTED, OUTCOME_IDLE_NEW, OUTCOME_IDLE_REPEAT, NUM_OUTCOMES };
	static const char* const aszState[NUM_PEACE_STATES] = { "peace", "preparing", "war", "barbarians and minors" };
	static const char* const aszOutcome[NUM_OUTCOMES] = { "gone", "moved", "stayed, working", "stayed, acted", "stayed idle, first turn", "stayed idle, as last turn" };
	static BridgedProfileSample aSamples[NUM_PEACE_STATES][2][NUM_OUTCOMES];
	static BridgedProfileSample aIdleSamples[NUM_UNITAI_TYPES];
	static BridgedProfileSample kNotReady = { "unit update: group not ready to move" };
	static std::map<long long, std::pair<int, int> > aLastIdle;	// unit -> the game turn and plot of its last idle stay

	CvUnit* pHeadUnit = pGroup->getHeadUnit();

	if (!pGroup->AI_isControlled() || (pHeadUnit == NULL) || !(pGroup->readyToMove() || pGroup->isForceUpdate()))
	{
		const unsigned long long uiStart = bridgedProfileTicks();
		const bool bResult = pGroup->AI_updateGroup();
		bridgedProfileAdd(&kNotReady, bridgedProfileTicks() - uiStart);
		return bResult;
	}

	const PlayerTypes eOwner = pGroup->getOwnerINLINE();
	const CvTeam& kTeam = GET_TEAM(pGroup->getTeam());
	const int iState = kTeam.getPeaceState();
	const CvPlot* pPlot = pGroup->plot();
	const int iUnit = pHeadUnit->getID();
	const UnitAITypes eUnitAI = pHeadUnit->AI_getUnitAIType();
	const int iMoves = pHeadUnit->getMoves();
	const int iDamage = pHeadUnit->getDamage();
	const bool bMadeAttack = pHeadUnit->isMadeAttack();
	const int iHostile = (GC.getMapINLINE().isTeamAttackerNear(pPlot, 4, kTeam.getEnemyTeams(), false) ? 1 : 0);
	const int iGameTurn = GC.getGameINLINE().getGameTurn();
	const int iPlot = GC.getMapINLINE().plotNumINLINE(pPlot->getX_INLINE(), pPlot->getY_INLINE());
	const long long iKey = (((long long)eOwner) << 32) | (unsigned int)iUnit;

	const unsigned long long uiStart = bridgedProfileTicks();
	const bool bResult = pGroup->AI_updateGroup();	// pGroup may not exist after this
	const unsigned long long uiTicks = bridgedProfileTicks() - uiStart;

	pHeadUnit = GET_PLAYER(eOwner).getUnit(iUnit);

	int iOutcome;

	if ((pHeadUnit == NULL) || pHeadUnit->isDelayedDeath())
	{
		iOutcome = OUTCOME_GONE;
	}
	else if (pHeadUnit->plot() != pPlot)
	{
		iOutcome = OUTCOME_MOVED;
	}
	else if (pHeadUnit->getGroup()->getLengthMissionQueue() > 0)
	{
		iOutcome = OUTCOME_WORKING;
	}
	else if ((pHeadUnit->getMoves() != iMoves) || (pHeadUnit->getDamage() != iDamage) || (pHeadUnit->isMadeAttack() != bMadeAttack))
	{
		iOutcome = OUTCOME_ACTED;
	}
	else
	{
		std::map<long long, std::pair<int, int> >::iterator it = aLastIdle.find(iKey);

		if ((it != aLastIdle.end()) && (it->second.first == iGameTurn))
		{
			// a second call for the unit in the same turn: leave it as it was first judged
			iOutcome = OUTCOME_IDLE_NEW;
		}
		else
		{
			iOutcome = (((it != aLastIdle.end()) && (it->second.first == (iGameTurn - 1)) && (it->second.second == iPlot)) ? OUTCOME_IDLE_REPEAT : OUTCOME_IDLE_NEW);
			aLastIdle[iKey] = std::make_pair(iGameTurn, iPlot);
		}
	}

	if ((iOutcome != OUTCOME_IDLE_NEW) && (iOutcome != OUTCOME_IDLE_REPEAT))
	{
		aLastIdle.erase(iKey);
	}

	BridgedProfileSample& kSample = aSamples[iState][iHostile][iOutcome];

	if (!kSample.bRegistered)
	{
		char szName[64];
		_snprintf(szName, sizeof(szName) - 1, "unit %s, %s: %s", aszState[iState], ((iHostile != 0) ? "enemy near" : "quiet"), aszOutcome[iOutcome]);
		szName[sizeof(szName) - 1] = '\0';
		kSample.szName = szName;
		bridgedProfileRegister(&kSample);
	}

	bridgedProfileAdd(&kSample, uiTicks);

	// the repeated idle stays with nothing near, by the kind of unit, for every team but the background ones
	if ((iOutcome == OUTCOME_IDLE_REPEAT) && (iHostile == 0) && (iState != PEACE_STATE_BACKGROUND) && (eUnitAI >= 0) && (eUnitAI < NUM_UNITAI_TYPES))
	{
		BridgedProfileSample& kIdleSample = aIdleSamples[eUnitAI];

		if (!kIdleSample.bRegistered)
		{
			char szName[64];
			_snprintf(szName, sizeof(szName) - 1, "idle as last turn, quiet: %s", GC.getUnitAIInfo(eUnitAI).getType());
			szName[sizeof(szName) - 1] = '\0';
			kIdleSample.szName = szName;
			bridgedProfileRegister(&kIdleSample);
		}

		bridgedProfileAdd(&kIdleSample, uiTicks);
	}

	return bResult;
}
#endif


bool CvSelectionGroupAI::AI_update()
{
	// see CvUnitAI::AI_isIdleKept(): a group that stays idle, turn after turn, keeps that
	CvUnit* pHeadUnit = getHeadUnit();
	const bool bJudge = (CvUnitAI::AI_idleUnitRules() && (pHeadUnit != NULL) && AI_isControlled() && !isHuman() && !GET_PLAYER(getOwnerINLINE()).isBarbarian()
		&& !GET_PLAYER(getOwnerINLINE()).isMinorCiv() && (readyToMove() || isForceUpdate()));

	if (bJudge && static_cast<CvUnitAI*>(pHeadUnit)->AI_isIdleKept())
	{
#ifdef BRIDGED_PROFILE
		static BridgedProfileSample kKeptSample = { "unit idle kept: group turns skipped" };
		bridgedProfileAdd(&kKeptSample, 0);
#endif
		static_cast<CvUnitAI*>(pHeadUnit)->AI_noteIdleKept();

		// The group keeps what its last decision left it doing: fortified or asleep, it stays
		// so (a forced update would have woken it for the unit to decide it again); awake, it
		// skips its turn, as an idle decision does.
		setForceUpdate(false);
		if (readyToMove())
		{
			pushMission(MISSION_SKIP);
		}
		return false;
	}

	const PlayerTypes eOwner = getOwnerINLINE();
	const int iHeadID = ((pHeadUnit != NULL) ? pHeadUnit->getID() : -1);
	const CvPlot* pPlot = plot();
	const int iMoves = ((pHeadUnit != NULL) ? pHeadUnit->getMoves() : 0);
	const int iDamage = ((pHeadUnit != NULL) ? pHeadUnit->getDamage() : 0);
	const bool bMadeAttack = ((pHeadUnit != NULL) && pHeadUnit->isMadeAttack());

#ifdef BRIDGED_PROFILE
	const bool bResult = peaceProbeUpdate(this);	// this group may not exist after this
#else
	const bool bResult = AI_updateGroup();
#endif

	if (bJudge)
	{
		CvUnit* pAfter = GET_PLAYER(eOwner).getUnit(iHeadID);

		if ((pAfter != NULL) && !pAfter->isDelayedDeath())
		{
			const bool bIdle = ((pAfter->plot() == pPlot) && (pAfter->getGroup()->getLengthMissionQueue() == 0) && (pAfter->getMoves() == iMoves)
				&& (pAfter->getDamage() == iDamage) && (pAfter->isMadeAttack() == bMadeAttack) && (pAfter->getGroup()->getHeadUnit() == pAfter));
			static_cast<CvUnitAI*>(pAfter)->AI_noteDecision(bIdle);
		}
	}

	return bResult;
}


bool CvSelectionGroupAI::AI_updateGroup()
{
	CLLNode<IDInfo>* pEntityNode;
	CvUnit* pLoopUnit;
	bool bDead;
	bool bFollow;

	PROFILE("CvSelectionGroupAI::AI_update");

	FAssert(getOwnerINLINE() != NO_PLAYER);

	if (!AI_isControlled())
	{
		return false;
	}

	if (getNumUnits() == 0)
	{
		return false;
	}

	if (isForceUpdate())
	{
		clearMissionQueue(); // XXX ???
		setActivityType(ACTIVITY_AWAKE);
		setForceUpdate(false);

		// if we are in the middle of attacking with a stack, cancel it
		AI_cancelGroupAttack();
	}

	FAssert(!(GET_PLAYER(getOwnerINLINE()).isAutoMoves()));

	int iTempHack = 0; // XXX

	bDead = false;
	
	bool bFailedAlreadyFighting = false;
	while ((m_bGroupAttack && !bFailedAlreadyFighting) || readyToMove())
	{
		iTempHack++;
		if (iTempHack > 100)
		{
			FAssert(false);
			CvUnit* pHeadUnit = getHeadUnit();
			if (NULL != pHeadUnit)
			{
				if (GC.getLogging())
				{
					TCHAR szOut[1024];
					CvWString szTempString;
					getUnitAIString(szTempString, pHeadUnit->AI_getUnitAIType());
					sprintf(szOut, "Unit stuck in loop: %S(%S)[%d, %d] (%S)\n", pHeadUnit->getName().GetCString(), GET_PLAYER(pHeadUnit->getOwnerINLINE()).getName(),
						pHeadUnit->getX_INLINE(), pHeadUnit->getY_INLINE(), szTempString.GetCString());
					gDLL->messageControlLog(szOut);
				}
				
				pHeadUnit->finishMoves();
			}
			break;
		}

		// if we want to force the group to attack, force another attack
		if (m_bGroupAttack)
		{			
			m_bGroupAttack = false;

			groupAttack(m_iGroupAttackX, m_iGroupAttackY, MOVE_DIRECT_ATTACK, bFailedAlreadyFighting);
		}
		// else pick AI action
		else
		{
			CvUnit* pHeadUnit = getHeadUnit();

			if (pHeadUnit == NULL || pHeadUnit->isDelayedDeath())
			{
				break;
			}

			resetPath();

			if (pHeadUnit->AI_update())
			{
				// AI_update returns true when we should abort the loop and wait until next slice
				break;
			}
		}

		if (doDelayedDeath())
		{
			bDead = true;
			break;
		}

		// if no longer group attacking, and force separate is true, then bail, decide what to do after group is split up
		// (UnitAI of head unit may have changed)
		if (!m_bGroupAttack && AI_isForceSeparate())
		{
			AI_separate();	// pointers could become invalid...
			return true;
		}
	}

	if (!bDead)
	{
		if (!isHuman())
		{
			bFollow = false;

			// if we not group attacking, then check for follow action
			if (!m_bGroupAttack)
			{
				pEntityNode = headUnitNode();

				while ((pEntityNode != NULL) && readyToMove(true))
				{
					pLoopUnit = ::getUnit(pEntityNode->m_data);
					pEntityNode = nextUnitNode(pEntityNode);

					if (pLoopUnit->canMove())
					{
						resetPath();

						if (pLoopUnit->AI_follow())
						{
							bFollow = true;
							break;
						}
					}
				}
			}

			if (doDelayedDeath())
			{
				bDead = true;
			}

			if (!bDead)
			{
				if (!bFollow && readyToMove(true))
				{
					pushMission(MISSION_SKIP);
				}
			}
		}
	}

	if (bDead)
	{
		return true;
	}

	return (isBusy() || isCargoBusy());
}


// Returns attack odds out of 100 (the higher, the better...)
int CvSelectionGroupAI::AI_attackOdds(const CvPlot* pPlot, bool bPotentialEnemy) const
{
	CvUnit* pAttacker;

	FAssert(getOwnerINLINE() != NO_PLAYER);

	if (pPlot->getBestDefender(NO_PLAYER, getOwnerINLINE(), NULL, !bPotentialEnemy, bPotentialEnemy) == NULL)
	{
		return 100;
	}
	
	int iOdds = 0;
	pAttacker = AI_getBestGroupAttacker(pPlot, bPotentialEnemy, iOdds);

	if (pAttacker == NULL)
	{
		return 0;
	}

	return iOdds;
}


CvUnit* CvSelectionGroupAI::AI_getBestGroupAttacker(const CvPlot* pPlot, bool bPotentialEnemy, int& iUnitOdds, bool bForce, bool bNoBlitz) const
{
	CLLNode<IDInfo>* pUnitNode;
	CvUnit* pLoopUnit;
	CvUnit* pBestUnit;
	int iPossibleTargets;
	int iValue;
	int iBestValue;
	int iOdds;
	int iBestOdds;

	iBestValue = 0;
	iBestOdds = 0;
	pBestUnit = NULL;

	pUnitNode = headUnitNode();

	bool bIsHuman = (pUnitNode != NULL) ? GET_PLAYER(::getUnit(pUnitNode->m_data)->getOwnerINLINE()).isHuman() : true;
			
	while (pUnitNode != NULL)
	{
		pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = nextUnitNode(pUnitNode);

		if (!pLoopUnit->isDead())
		{
			bool bCanAttack = false;
			if (pLoopUnit->getDomainType() == DOMAIN_AIR)
			{
				bCanAttack = pLoopUnit->canAirAttack();
			}
			else
			{
				bCanAttack = pLoopUnit->canAttack();

				if (bCanAttack && bNoBlitz && pLoopUnit->isBlitz() && pLoopUnit->isMadeAttack())
				{
					bCanAttack = false;
				}
			}

			if (bCanAttack)
			{
				if (bForce || pLoopUnit->canMove())
				{
					if (bForce || pLoopUnit->canMoveInto(pPlot, /*bAttack*/ true, /*bDeclareWar*/ bPotentialEnemy))
					{
						iOdds = pLoopUnit->AI_attackOdds(pPlot, bPotentialEnemy);
						
						iValue = iOdds;
						FAssertMsg(iValue > 0, "iValue is expected to be greater than 0");

						if (pLoopUnit->collateralDamage() > 0)
						{
							iPossibleTargets = std::min((pPlot->getNumVisibleEnemyDefenders(pLoopUnit) - 1), pLoopUnit->collateralDamageMaxUnits());

							if (iPossibleTargets > 0)
							{
								iValue *= (100 + ((pLoopUnit->collateralDamage() * iPossibleTargets) / 5));
								iValue /= 100;
							}
						}

						// if non-human, prefer the last unit that has the best value (so as to avoid splitting the group)
						if (iValue > iBestValue || (!bIsHuman && iValue > 0 && iValue == iBestValue))
						{
							iBestValue = iValue;
							iBestOdds = iOdds;
							pBestUnit = pLoopUnit;
						}
					}
				}
			}
		}
	}
	
	iUnitOdds = iBestOdds;
	return pBestUnit;
}

CvUnit* CvSelectionGroupAI::AI_getBestGroupSacrifice(const CvPlot* pPlot, bool bPotentialEnemy, bool bForce, bool bNoBlitz) const
{
	int iBestValue = 0;
	CvUnit* pBestUnit = NULL;

	CLLNode<IDInfo>* pUnitNode = headUnitNode();
	while (pUnitNode != NULL)
	{
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = nextUnitNode(pUnitNode);

		if (!pLoopUnit->isDead())
		{
			bool bCanAttack = false;
			if (pLoopUnit->getDomainType() == DOMAIN_AIR)
			{
				bCanAttack = pLoopUnit->canAirAttack();
			}
			else
			{
				bCanAttack = pLoopUnit->canAttack();

				if (bCanAttack && bNoBlitz && pLoopUnit->isBlitz() && pLoopUnit->isMadeAttack())
				{
					bCanAttack = false;
				}
			}

			if (bCanAttack)
			{
				if (bForce || pLoopUnit->canMove())
				{
					if (bForce || pLoopUnit->canMoveInto(pPlot, true))
					{
                        int iValue = pLoopUnit->AI_sacrificeValue(pPlot);
						FAssertMsg(iValue > 0, "iValue is expected to be greater than 0");

						// we want to pick the last unit of highest value, so pick the last unit with a good value
						if (iValue >= iBestValue)
						{
							iBestValue = iValue;
							pBestUnit = pLoopUnit;
						}
					}
				}
			}
		}
	}

	return pBestUnit;
}

// Returns ratio of strengths of stacks times 100
// (so 100 is even ratio, numbers over 100 mean this group is more powerful than the stack on a plot)
int CvSelectionGroupAI::AI_compareStacks(const CvPlot* pPlot, bool bPotentialEnemy, bool bCheckCanAttack, bool bCheckCanMove) const
{
	FAssert(pPlot != NULL);

	int	compareRatio;
	DomainTypes eDomainType = getDomainType();

	// if not aircraft, then choose based on the plot, not the head unit (mainly for transport carried units)
	if (eDomainType != DOMAIN_AIR)
	{
		if (pPlot->isWater())
			eDomainType = DOMAIN_SEA;
		else
			eDomainType = DOMAIN_LAND;

	}

	compareRatio = AI_sumStrength(pPlot, eDomainType, bCheckCanAttack, bCheckCanMove);
	compareRatio *= 100;

	PlayerTypes eOwner = getOwnerINLINE();
	if (eOwner == NO_PLAYER)
	{
		eOwner = getHeadOwner();
	}
	FAssert(eOwner != NO_PLAYER);
	
	int defenderSum = pPlot->AI_sumStrength(NO_PLAYER, getOwnerINLINE(), eDomainType, true, !bPotentialEnemy, bPotentialEnemy);
	compareRatio /= std::max(1, defenderSum);

	return compareRatio;
}

int CvSelectionGroupAI::AI_sumStrength(const CvPlot* pAttackedPlot, DomainTypes eDomainType, bool bCheckCanAttack, bool bCheckCanMove) const
{
	CLLNode<IDInfo>* pUnitNode;
	CvUnit* pLoopUnit;
	int	strSum = 0;

	pUnitNode = headUnitNode();

	while (pUnitNode != NULL)
	{
		pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = nextUnitNode(pUnitNode);

		if (!pLoopUnit->isDead())
		{
			bool bCanAttack = false;
			if (pLoopUnit->getDomainType() == DOMAIN_AIR)
				bCanAttack = pLoopUnit->canAirAttack();
			else
				bCanAttack = pLoopUnit->canAttack();

			if (!bCheckCanAttack || bCanAttack)
			{
				if (!bCheckCanMove || pLoopUnit->canMove())
					if (!bCheckCanMove || pAttackedPlot == NULL || pLoopUnit->canMoveInto(pAttackedPlot, /*bAttack*/ true, /*bDeclareWar*/ true))
						if (eDomainType == NO_DOMAIN || pLoopUnit->getDomainType() == eDomainType)
							strSum += pLoopUnit->currEffectiveStr(pAttackedPlot, pLoopUnit);
			}
		}
	}

	return strSum;
}

void CvSelectionGroupAI::AI_queueGroupAttack(int iX, int iY)
{
	m_bGroupAttack = true;

	m_iGroupAttackX = iX;
	m_iGroupAttackY = iY;
}

inline void CvSelectionGroupAI::AI_cancelGroupAttack()
{
	m_bGroupAttack = false;
}

inline bool CvSelectionGroupAI::AI_isGroupAttack()
{
	return m_bGroupAttack;
}

bool CvSelectionGroupAI::AI_isControlled()
{
	return (!isHuman() || isAutomated());
}


bool CvSelectionGroupAI::AI_isDeclareWar(const CvPlot* pPlot)
{
	FAssert(getHeadUnit() != NULL);

	if (isHuman())
	{
		return false;
	}
	else
	{
		bool bLimitedWar = false;
		if (pPlot != NULL)
		{
			TeamTypes ePlotTeam = pPlot->getTeam();
			if (ePlotTeam != NO_TEAM)
			{
				WarPlanTypes eWarplan = GET_TEAM(getTeam()).AI_getWarPlan(ePlotTeam);
				if (eWarplan == WARPLAN_LIMITED)
				{
					bLimitedWar = true;
				}
			}
		}

		CvUnit* pHeadUnit = getHeadUnit();

		if (pHeadUnit != NULL)
		{
			switch (pHeadUnit->AI_getUnitAIType())
			{
			case UNITAI_UNKNOWN:
			case UNITAI_ANIMAL:
			case UNITAI_SETTLE:
			case UNITAI_WORKER:
				break;
			case UNITAI_ATTACK_CITY:
			case UNITAI_ATTACK_CITY_LEMMING:
				return true;
				break;

			case UNITAI_ATTACK:
			case UNITAI_COLLATERAL:
			case UNITAI_PILLAGE:
				if (bLimitedWar)
				{
					return true;
				}
				break;
				
			case UNITAI_PARADROP:
			case UNITAI_RESERVE:
			case UNITAI_COUNTER:
			case UNITAI_CITY_DEFENSE:
			case UNITAI_CITY_COUNTER:
			case UNITAI_CITY_SPECIAL:
			case UNITAI_EXPLORE:
			case UNITAI_MISSIONARY:
			case UNITAI_PROPHET:
			case UNITAI_ARTIST:
			case UNITAI_SCIENTIST:
			case UNITAI_GENERAL:
			case UNITAI_MERCHANT:
			case UNITAI_ENGINEER:
			case UNITAI_SPY:
			case UNITAI_ICBM:
			case UNITAI_WORKER_SEA:
				break;

			case UNITAI_ATTACK_SEA:
			case UNITAI_RESERVE_SEA:
			case UNITAI_ESCORT_SEA:
				if (bLimitedWar)
				{
					return true;
				}
				break;
			case UNITAI_EXPLORE_SEA:
				break;

			case UNITAI_ASSAULT_SEA:
				if (hasCargo())
				{
					return true;
				}
				break;

			case UNITAI_SETTLER_SEA:
			case UNITAI_MISSIONARY_SEA:
			case UNITAI_SPY_SEA:
			case UNITAI_CARRIER_SEA:
			case UNITAI_MISSILE_CARRIER_SEA:
			case UNITAI_PIRATE_SEA:
			case UNITAI_ATTACK_AIR:
			case UNITAI_DEFENSE_AIR:
			case UNITAI_CARRIER_AIR:
			case UNITAI_MISSILE_AIR:
				break;

			default:
				FAssert(false);
				break;
			}
		}
	}

	return false;
}


CvPlot* CvSelectionGroupAI::AI_getMissionAIPlot()
{
	return GC.getMapINLINE().plotSorenINLINE(m_iMissionAIX, m_iMissionAIY);
}


bool CvSelectionGroupAI::AI_isForceSeparate()
{
	return m_bForceSeparate;
}


void CvSelectionGroupAI::AI_makeForceSeparate()
{
	m_bForceSeparate = true;
}


MissionAITypes CvSelectionGroupAI::AI_getMissionAIType()
{
	return m_eMissionAIType;
}


void CvSelectionGroupAI::AI_setMissionAI(MissionAITypes eNewMissionAI, CvPlot* pNewPlot, CvUnit* pNewUnit)
{
	// the owner keeps count of the groups heading for each plot: see CvPlayerAI::AI_plotTargetMissionAIs()
	if (getOwnerINLINE() != NO_PLAYER)
	{
		CvPlot* pOldPlot = AI_getMissionAIPlot();

		if (pOldPlot != pNewPlot)
		{
			if (pOldPlot != NULL)
			{
				GET_PLAYER(getOwnerINLINE()).AI_changeMissionTargetGroups(pOldPlot, -1);
			}

			if (pNewPlot != NULL)
			{
				GET_PLAYER(getOwnerINLINE()).AI_changeMissionTargetGroups(pNewPlot, 1);
			}
		}
	}

	m_eMissionAIType = eNewMissionAI;

	if (pNewPlot != NULL)
	{
		m_iMissionAIX = pNewPlot->getX_INLINE();
		m_iMissionAIY = pNewPlot->getY_INLINE();
	}
	else
	{
		m_iMissionAIX = INVALID_PLOT_COORD;
		m_iMissionAIY = INVALID_PLOT_COORD;
	}

	if (pNewUnit != NULL)
	{
		m_missionAIUnit = pNewUnit->getIDInfo();
	}
	else
	{
		m_missionAIUnit.reset();
	}
}


CvUnit* CvSelectionGroupAI::AI_getMissionAIUnit()
{
	return getUnit(m_missionAIUnit);
}

bool CvSelectionGroupAI::AI_isFull()
{
	CLLNode<IDInfo>* pUnitNode;
	CvUnit* pLoopUnit;

	if (getNumUnits() > 0)
	{
		UnitAITypes eUnitAI = getHeadUnitAI();
		// do two passes, the first pass, we ignore units with speical cargo
		int iSpecialCargoCount = 0;
		int iCargoCount = 0;
		
		// first pass, count but ignore special cargo units
		pUnitNode = headUnitNode();

		while (pUnitNode != NULL)
		{
			pLoopUnit = ::getUnit(pUnitNode->m_data);
			pUnitNode = nextUnitNode(pUnitNode);
			if (pLoopUnit->AI_getUnitAIType() == eUnitAI)
			{
				if (pLoopUnit->cargoSpace() > 0)
				{
					iCargoCount++;
				}

				if (pLoopUnit->specialCargo() != NO_SPECIALUNIT)
				{
					iSpecialCargoCount++;
				}
				else if (!(pLoopUnit->isFull()))
				{
					return false;
				}
			}
		}
		
		// if every unit in the group has special cargo, then check those, otherwise, consider ourselves full
		if (iSpecialCargoCount >= iCargoCount)
		{
			pUnitNode = headUnitNode();
			while (pUnitNode != NULL)
			{
				pLoopUnit = ::getUnit(pUnitNode->m_data);
				pUnitNode = nextUnitNode(pUnitNode);
				
				if (pLoopUnit->AI_getUnitAIType() == eUnitAI)
				{
					if (!(pLoopUnit->isFull()))
					{
						return false;
					}
				}
			}
		}

		return true;
	}

	return false;	
}


CvUnit* CvSelectionGroupAI::AI_ejectBestDefender(CvPlot* pDefendPlot)
{
	CLLNode<IDInfo>* pEntityNode;
	CvUnit* pLoopUnit;

	pEntityNode = headUnitNode();
	
	CvUnit* pBestUnit = NULL;
	int iBestUnitValue = 0;

	while (pEntityNode != NULL)
	{
		pLoopUnit = ::getUnit(pEntityNode->m_data);
		pEntityNode = nextUnitNode(pEntityNode);
		
		if (!pLoopUnit->noDefensiveBonus())
		{
			int iValue = pLoopUnit->currEffectiveStr(pDefendPlot, NULL) * 100;
			
			if (pDefendPlot->isCity(true, getTeam()))
			{
				iValue *= 100 + pLoopUnit->cityDefenseModifier();
				iValue /= 100;
			}

			iValue *= 100;
			iValue /= (100 + pLoopUnit->cityAttackModifier() + pLoopUnit->getExtraCityAttackPercent());
			
			iValue /= 2 + pLoopUnit->getLevel();
			
			if (iValue > iBestUnitValue)
			{
				iBestUnitValue = iValue;
				pBestUnit = pLoopUnit;
			}
		}
	}
	
	if (NULL != pBestUnit && getNumUnits() > 1)
	{
		pBestUnit->joinGroup(NULL);
	}
	
	return pBestUnit;
}


// Protected Functions...

void CvSelectionGroupAI::read(FDataStreamBase* pStream)
{
	CvSelectionGroup::read(pStream);

	uint uiFlag=0;
	pStream->Read(&uiFlag);	// flags for expansion

	pStream->Read(&m_iMissionAIX);
	pStream->Read(&m_iMissionAIY);

	pStream->Read(&m_bForceSeparate);

	pStream->Read((int*)&m_eMissionAIType);

	pStream->Read((int*)&m_missionAIUnit.eOwner);
	pStream->Read(&m_missionAIUnit.iID);

	pStream->Read(&m_bGroupAttack);
	pStream->Read(&m_iGroupAttackX);
	pStream->Read(&m_iGroupAttackY);
}


void CvSelectionGroupAI::write(FDataStreamBase* pStream)
{
	CvSelectionGroup::write(pStream);

	uint uiFlag=0;
	pStream->Write(uiFlag);		// flag for expansion

	pStream->Write(m_iMissionAIX);
	pStream->Write(m_iMissionAIY);

	pStream->Write(m_bForceSeparate);

	pStream->Write(m_eMissionAIType);

	pStream->Write(m_missionAIUnit.eOwner);
	pStream->Write(m_missionAIUnit.iID);

	pStream->Write(m_bGroupAttack);
	pStream->Write(m_iGroupAttackX);
	pStream->Write(m_iGroupAttackY);
}

// Private Functions...
