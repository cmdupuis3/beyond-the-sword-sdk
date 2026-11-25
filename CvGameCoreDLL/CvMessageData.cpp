#include "CvGameCoreDLL.h"
#include "CvMessageData.h"
#include "FDataStreamBase.h"
#include "CvDLLUtilityIFaceBase.h"
#include "CvDLLInterfaceIFaceBase.h"
#include "CvEventReporter.h"
#include "CvPlayerAI.h"
#include "BridgedThreads.h"
#include "LinkedList.h"

CvMessageData* CvMessageData::createMessage(GameMessageTypes eType)
{
	switch (eType)
	{
	case GAMEMESSAGE_EXTENDED_GAME:
		return new CvNetExtendedGame();
	case GAMEMESSAGE_AUTO_MOVES:
		return new CvNetAutoMoves();
	case GAMEMESSAGE_TURN_COMPLETE:
		return new CvNetTurnComplete();
	case GAMEMESSAGE_PUSH_ORDER:
		return new CvNetPushOrder();
	case GAMEMESSAGE_POP_ORDER: 
		return new CvNetPopOrder();
	case GAMEMESSAGE_DO_TASK:
		return new CvNetDoTask();
	case GAMEMESSAGE_UPDATE_CIVICS: 
		return new CvNetUpdateCivics();
	case GAMEMESSAGE_RESEARCH: 
		return new CvNetResearch();
	case GAMEMESSAGE_ESPIONAGE_CHANGE: 
		return new CvNetEspionageChange();
	case GAMEMESSAGE_ADVANCED_START_ACTION:
		return new CvNetAdvancedStartAction();
	case GAMEMESSAGE_MOD_NET_MESSAGE: 
		return new CvNetModNetMessage();
	case GAMEMESSAGE_CONVERT: 
		return new CvNetConvert();
	case GAMEMESSAGE_EMPIRE_SPLIT: 
		return new CvNetEmpireSplit();
	case GAMEMESSAGE_FOUND_RELIGION: 
		return new CvNetFoundReligion();
	case GAMEMESSAGE_LAUNCH_SPACESHIP: 
		return new CvNetLaunchSpaceship();
	case GAMEMESSAGE_EVENT_TRIGGERED: 
		return new CvNetEventTriggered();
	case GAMEMESSAGE_JOIN_GROUP: 
		return new CvNetJoinGroup();
	case GAMEMESSAGE_PUSH_MISSION: 
		return new CvNetPushMission();
	case GAMEMESSAGE_AUTO_MISSION: 
		return new CvNetAutoMission();
	case GAMEMESSAGE_DO_COMMAND: 
		return new CvNetDoCommand();
	case GAMEMESSAGE_PERCENT_CHANGE: 
		return new CvNetPercentChange();
	case GAMEMESSAGE_CHANGE_VASSAL: 
		return new CvNetChangeVassal();
	case GAMEMESSAGE_CHOOSE_ELECTION: 
		return new CvNetChooseElection();
	case GAMEMESSAGE_DIPLO_VOTE: 
		return new CvNetDiploVote();
	case GAMEMESSAGE_CHANGE_WAR: 
		return new CvNetChangeWar();
	case GAMEMESSAGE_PING: 
		return new CvNetPing();
	default:
		FAssertMsg(false, "Unknown message type");
	}
	return NULL;
}

CvMessageData::CvMessageData(GameMessageTypes eType) : m_eType(eType) 
{ 
}

CvMessageData::~CvMessageData() 
{ 
}

GameMessageTypes CvMessageData::getType() const
{
	return m_eType;
}

CvNetExtendedGame::CvNetExtendedGame(PlayerTypes ePlayer) : CvMessageData(GAMEMESSAGE_EXTENDED_GAME), m_ePlayer(ePlayer) 
{ 
}

void CvNetExtendedGame::Debug(char* szAddendum)
{
	sprintf(szAddendum, "Extended Game, %d", m_ePlayer);
}

void CvNetExtendedGame::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		GET_PLAYER(m_ePlayer).makeExtendedGame();
	}
}

void CvNetExtendedGame::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
}

void CvNetExtendedGame::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
}

CvNetAutoMoves::CvNetAutoMoves(PlayerTypes ePlayer) : CvMessageData(GAMEMESSAGE_AUTO_MOVES), m_ePlayer(ePlayer) 
{ 
}

void CvNetAutoMoves::Debug(char* szAddendum)
{
	sprintf(szAddendum, "Auto Moves, %d", m_ePlayer);
}

void CvNetAutoMoves::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		GET_PLAYER(m_ePlayer).setAutoMoves(true);
	}
}

void CvNetAutoMoves::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
}

void CvNetAutoMoves::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
}

CvNetTurnComplete::CvNetTurnComplete(PlayerTypes ePlayer) : CvMessageData(GAMEMESSAGE_TURN_COMPLETE), m_ePlayer(ePlayer) 
{ 
}

void CvNetTurnComplete::Debug(char* szAddendum)
{
	sprintf(szAddendum, "Turn Complete, %d", m_ePlayer);
}

void CvNetTurnComplete::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		GET_PLAYER(m_ePlayer).setEndTurn(true);
	}
}

void CvNetTurnComplete::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
}

void CvNetTurnComplete::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
}

CvNetPushOrder::CvNetPushOrder() : CvMessageData(GAMEMESSAGE_PUSH_ORDER), m_ePlayer(NO_PLAYER), m_iCityID(-1), m_eOrder(NO_ORDER), m_iData(-1), m_bAlt(false), m_bShift(false), m_bCtrl(false)
{
}

CvNetPushOrder::CvNetPushOrder(PlayerTypes ePlayer, int iCityID, OrderTypes eOrder, int iData, bool bAlt, bool bShift, bool bCtrl) : CvMessageData(GAMEMESSAGE_PUSH_ORDER), m_ePlayer(ePlayer), m_iCityID(iCityID), m_eOrder(eOrder), m_iData(iData), m_bAlt(bAlt), m_bShift(bShift), m_bCtrl(bCtrl)
{
}

void CvNetPushOrder::Debug(char* szAddendum)
{
	sprintf(szAddendum, "Pushing an order at the city, order is %d, data is %d", m_eOrder, m_iData);
}

void CvNetPushOrder::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		CvCity* pCity = GET_PLAYER(m_ePlayer).getCity(m_iCityID);
		if (pCity != NULL)
		{
			pCity->pushOrder(m_eOrder, m_iData, -1, m_bAlt, !(m_bShift || m_bCtrl), m_bShift);
		}

		if (GC.getGameINLINE().getActivePlayer() == m_ePlayer)
		{
			gDLL->getInterfaceIFace()->updatePythonScreens();
		}
	}
}

void CvNetPushOrder::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_iCityID);
	pStream->Write(m_eOrder);
	pStream->Write(m_iData);
	pStream->Write(m_bAlt);
	pStream->Write(m_bShift);
	pStream->Write(m_bCtrl);
}

void CvNetPushOrder::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read(&m_iCityID);
	pStream->Read((int*)&m_eOrder);
	pStream->Read(&m_iData);
	pStream->Read(&m_bAlt);
	pStream->Read(&m_bShift);
	pStream->Read(&m_bCtrl);
}

CvNetPopOrder::CvNetPopOrder() : CvMessageData(GAMEMESSAGE_POP_ORDER), m_ePlayer(NO_PLAYER), m_iCityID(-1), m_iNum(0)
{
}

CvNetPopOrder::CvNetPopOrder(PlayerTypes ePlayer, int iCityID, int iNum) : CvMessageData(GAMEMESSAGE_POP_ORDER), m_ePlayer(ePlayer), m_iCityID(iCityID), m_iNum(iNum)
{
}

void CvNetPopOrder::Debug(char* szAddendum) 
{	
	sprintf(szAddendum, "Pop Order at City, city ID is %d", m_iCityID);	
}

void CvNetPopOrder::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		CvCity* pCity = GET_PLAYER(m_ePlayer).getCity(m_iCityID);
		if (pCity != NULL)
		{
			pCity->popOrder(m_iNum);
		}

		if (GC.getGameINLINE().getActivePlayer() == m_ePlayer)
		{
			gDLL->getInterfaceIFace()->updatePythonScreens();
		}
	}
}

void CvNetPopOrder::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_iCityID);
	pStream->Write(m_iNum);
}

void CvNetPopOrder::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read(&m_iCityID);
	pStream->Read(&m_iNum);
}

CvNetDoTask::CvNetDoTask() : CvMessageData(GAMEMESSAGE_DO_TASK), m_ePlayer(NO_PLAYER), m_iCityID(-1), m_eTask(NO_TASK), m_iData1(-1), m_iData2(-1), m_bOption(false), m_bAlt(false), m_bShift(false), m_bCtrl(false)
{
}

CvNetDoTask::CvNetDoTask(PlayerTypes ePlayer, int iCityID, TaskTypes eTask, int iData1, int iData2, bool bOption, bool bAlt, bool bShift, bool bCtrl) : CvMessageData(GAMEMESSAGE_DO_TASK), m_ePlayer(ePlayer), m_iCityID(iCityID), m_eTask(eTask), m_iData1(iData1), m_iData2(iData2), m_bOption(bOption), m_bAlt(bAlt), m_bShift(bShift), m_bCtrl(bCtrl)
{
}

void CvNetDoTask::Debug(char* szAddendum)
{
	sprintf(szAddendum, "Do Task at City, city ID is %d", m_iCityID);
}

void CvNetDoTask::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		CvCity* pCity = GET_PLAYER(m_ePlayer).getCity(m_iCityID);
		if (pCity != NULL)
		{
			pCity->doTask(m_eTask, m_iData1, m_iData2, m_bOption, m_bAlt, m_bShift, m_bCtrl);
		}
	}
}

void CvNetDoTask::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_iCityID);
	pStream->Write(m_eTask);
	pStream->Write(m_iData1);
	pStream->Write(m_iData2);
	pStream->Write(m_bOption);
	pStream->Write(m_bAlt);
	pStream->Write(m_bShift);
	pStream->Write(m_bCtrl);
}

void CvNetDoTask::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read(&m_iCityID);
	pStream->Read((int*)&m_eTask);
	pStream->Read(&m_iData1);
	pStream->Read(&m_iData2);
	pStream->Read(&m_bOption);
	pStream->Read(&m_bAlt);
	pStream->Read(&m_bShift);
	pStream->Read(&m_bCtrl);
}

CvNetUpdateCivics::CvNetUpdateCivics() : CvMessageData(GAMEMESSAGE_UPDATE_CIVICS), m_ePlayer(NO_PLAYER), m_aeCivics(GC.getNumCivicOptionInfos(), NO_CIVIC)
{
}

CvNetUpdateCivics::CvNetUpdateCivics(PlayerTypes ePlayer, const std::vector<CivicTypes>& aeCivics) : CvMessageData(GAMEMESSAGE_UPDATE_CIVICS), m_ePlayer(ePlayer), m_aeCivics(aeCivics)
{
}


void CvNetUpdateCivics::Debug(char* szAddendum)
{
	sprintf(szAddendum, "Update Civics, ePlayer is %d", (int)m_ePlayer);
}

void CvNetUpdateCivics::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER && !m_aeCivics.empty())
	{
		GET_PLAYER(m_ePlayer).revolution(&m_aeCivics[0]);
	}
}

void CvNetUpdateCivics::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	for (int i = 0; i < GC.getNumCivicOptionInfos(); i++) 
	{
		pStream->Write(m_aeCivics[i]);
	}
}

void CvNetUpdateCivics::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	for (int i = 0; i < GC.getNumCivicOptionInfos(); i++) 
	{
		pStream->Read((int*)&m_aeCivics[i]);
	}
}

CvNetResearch::CvNetResearch() : CvMessageData(GAMEMESSAGE_RESEARCH), m_ePlayer(NO_PLAYER), m_iDiscover(-1), m_bShift(false), m_eTech(NO_TECH)
{
}

CvNetResearch::CvNetResearch(PlayerTypes ePlayer, TechTypes eTech, int iDiscover, bool bShift) : CvMessageData(GAMEMESSAGE_RESEARCH), m_ePlayer(ePlayer), m_iDiscover(iDiscover), m_bShift(bShift), m_eTech(eTech)
{
}

void CvNetResearch::Debug(char* szAddendum) 
{		
	sprintf(szAddendum, "Research, %d",	m_eTech); 
}

void CvNetResearch::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		CvPlayer& kPlayer = GET_PLAYER(m_ePlayer);
		if (m_iDiscover > 0)
		{
			GET_TEAM(kPlayer.getTeam()).setHasTech(m_eTech, true, m_ePlayer, true, true);

			if (m_iDiscover > 1)
			{
				if (m_ePlayer == GC.getGameINLINE().getActivePlayer())
				{
					kPlayer.chooseTech(m_iDiscover - 1);
				}
			}
		}
		else
		{
			if (m_eTech == NO_TECH)
			{
				kPlayer.clearResearchQueue();
			}
			else if (kPlayer.canEverResearch(m_eTech))
			{
				if ((GET_TEAM(kPlayer.getTeam()).isHasTech(m_eTech) || kPlayer.isResearchingTech(m_eTech)) && !m_bShift)
				{
					kPlayer.clearResearchQueue();
				}
				kPlayer.pushResearch(m_eTech, !m_bShift);
			}
		}

		if (GC.getGameINLINE().getActivePlayer() == m_ePlayer)
		{
			gDLL->getInterfaceIFace()->updatePythonScreens();
		}
	}
}

void CvNetResearch::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_iDiscover);
	pStream->Write(m_bShift);
	pStream->Write(m_eTech);
}

void CvNetResearch::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read(&m_iDiscover);
	pStream->Read(&m_bShift);
	pStream->Read((int*)&m_eTech);
}

CvNetEspionageChange::CvNetEspionageChange() : CvMessageData(GAMEMESSAGE_ESPIONAGE_CHANGE), m_ePlayer(NO_PLAYER), m_eTargetTeam(NO_TEAM), m_iChange(0)
{
}

CvNetEspionageChange::CvNetEspionageChange(PlayerTypes ePlayer, TeamTypes eTargetTeam, int iChange) : CvMessageData(GAMEMESSAGE_ESPIONAGE_CHANGE), m_ePlayer(ePlayer), m_eTargetTeam(eTargetTeam), m_iChange(iChange)
{
}

void CvNetEspionageChange::Debug(char* szAddendum) 
{ 
	sprintf(szAddendum, "TargetTeam: %d, Espionage Change: %d", m_eTargetTeam, m_iChange); 
}

void CvNetEspionageChange::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		GET_PLAYER(m_ePlayer).changeEspionageSpendingWeightAgainstTeam(m_eTargetTeam, m_iChange);
	}
}
	
void CvNetEspionageChange::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_eTargetTeam);
	pStream->Write(m_iChange);
}

void CvNetEspionageChange::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read((int*)&m_eTargetTeam);
	pStream->Read(&m_iChange);
}

CvNetAdvancedStartAction::CvNetAdvancedStartAction(AdvancedStartActionTypes eAction, PlayerTypes ePlayer, int iX, int iY, int iData, bool bAdd) : CvMessageData(GAMEMESSAGE_ADVANCED_START_ACTION),
	m_eAction(eAction),
	m_ePlayer(ePlayer),
	m_iX(iX),
	m_iY(iY),
	m_iData(iData),
	m_bAdd(bAdd)
{
}

void CvNetAdvancedStartAction::Debug(char* szAddendum) 
{ 	
	sprintf(szAddendum, "Non-simultaneous Advanced Start Action notification");	
}

void CvNetAdvancedStartAction::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		if (!gDLL->getInterfaceIFace()->isExitingToMainMenu())
		{
			GET_PLAYER(m_ePlayer).doAdvancedStartAction(m_eAction, m_iX, m_iY, m_iData, m_bAdd);
		}
	}
}

void CvNetAdvancedStartAction::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_eAction);
	pStream->Write(m_ePlayer);
	pStream->Write(m_iX);
	pStream->Write(m_iY);
	pStream->Write(m_iData);
	pStream->Write(m_bAdd);
}

void CvNetAdvancedStartAction::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_eAction);
	pStream->Read((int*)&m_ePlayer);
	pStream->Read(&m_iX);
	pStream->Read(&m_iY);
	pStream->Read(&m_iData);
	pStream->Read(&m_bAdd);
}

CvNetModNetMessage::CvNetModNetMessage(int iData1, int iData2, int iData3, int iData4, int iData5) : CvMessageData(GAMEMESSAGE_MOD_NET_MESSAGE), m_iData1(iData1), m_iData2(iData2), m_iData3(iData3), m_iData4(iData4), m_iData5(iData5)
{
}

void CvNetModNetMessage::Debug(char* szAddendum) 
{ 	
	sprintf(szAddendum, "Non-simultaneous ModNetMessage notification");	
}

void CvNetModNetMessage::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	CvEventReporter::getInstance().reportModNetMessage(m_iData1, m_iData2, m_iData3, m_iData4, m_iData5);
}

void CvNetModNetMessage::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_iData1);
	pStream->Write(m_iData2);
	pStream->Write(m_iData3);
	pStream->Write(m_iData4);
	pStream->Write(m_iData5);
}

void CvNetModNetMessage::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read(&m_iData1);
	pStream->Read(&m_iData2);
	pStream->Read(&m_iData3);
	pStream->Read(&m_iData4);
	pStream->Read(&m_iData5);
}

CvNetConvert::CvNetConvert(PlayerTypes ePlayer, ReligionTypes eReligion) : CvMessageData(GAMEMESSAGE_CONVERT), m_ePlayer(ePlayer), m_eReligion(eReligion)
{
}

void CvNetConvert::Debug(char* szAddendum) 
{ 
	sprintf(szAddendum, "Religion: %d", m_eReligion); 
}

void CvNetConvert::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		GET_PLAYER(m_ePlayer).convert(m_eReligion);
	}
}

void CvNetConvert::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_eReligion);
}

void CvNetConvert::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read((int*)&m_eReligion);
}

CvNetEmpireSplit::CvNetEmpireSplit(PlayerTypes ePlayer, int iAreaId) : CvMessageData(GAMEMESSAGE_EMPIRE_SPLIT),
	m_ePlayer(ePlayer),
	m_iAreaId(iAreaId)
{
}

void CvNetEmpireSplit::Debug(char* szAddendum) 
{ 	
	sprintf(szAddendum, "Non-simultaneous empire split notification");	
}

void CvNetEmpireSplit::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		GET_PLAYER(m_ePlayer).splitEmpire(m_iAreaId);
	}
}

void CvNetEmpireSplit::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_iAreaId);
}

void CvNetEmpireSplit::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read(&m_iAreaId);
}

CvNetFoundReligion::CvNetFoundReligion(PlayerTypes ePlayer, ReligionTypes eReligion, ReligionTypes eSlotReligion) : CvMessageData(GAMEMESSAGE_FOUND_RELIGION),
	m_ePlayer(ePlayer),
	m_eReligion(eReligion),
	m_eSlotReligion(eSlotReligion)
{
}

void CvNetFoundReligion::Debug(char* szAddendum) 
{ 	
	sprintf(szAddendum, "Non-simultaneous found religion notification");	
}

void CvNetFoundReligion::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		GET_PLAYER(m_ePlayer).foundReligion(m_eReligion, m_eSlotReligion, true);
	}
}

void CvNetFoundReligion::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_eReligion);
	pStream->Write(m_eSlotReligion);
}

void CvNetFoundReligion::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read((int*)&m_eReligion);
	pStream->Read((int*)&m_eSlotReligion);
}

CvNetLaunchSpaceship::CvNetLaunchSpaceship(PlayerTypes ePlayer, VictoryTypes eVictory) : CvMessageData(GAMEMESSAGE_LAUNCH_SPACESHIP),
	m_ePlayer(ePlayer),
	m_eVictory(eVictory)
{
}

void CvNetLaunchSpaceship::Debug(char* szAddendum)
{ 	
	sprintf(szAddendum, "Non-simultaneous spaceship launch notification");	
}

void CvNetLaunchSpaceship::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		GET_PLAYER(m_ePlayer).launch(m_eVictory);
	}
}

void CvNetLaunchSpaceship::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_eVictory);
}

void CvNetLaunchSpaceship::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read((int*)&m_eVictory);
}

CvNetEventTriggered::CvNetEventTriggered(PlayerTypes ePlayer, EventTypes eEvent, int iEventTriggeredId) : CvMessageData(GAMEMESSAGE_EVENT_TRIGGERED),
	m_ePlayer(ePlayer),
	m_eEvent(eEvent),
	m_iEventTriggeredId(iEventTriggeredId)
{
}

void CvNetEventTriggered::Debug(char* szAddendum) 
{ 	
	sprintf(szAddendum, "Non-simultaneous event trigger notification");	
}

void CvNetEventTriggered::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		GET_PLAYER(m_ePlayer).applyEvent(m_eEvent, m_iEventTriggeredId);
	}
}

void CvNetEventTriggered::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_eEvent);
	pStream->Write(m_iEventTriggeredId);
}

void CvNetEventTriggered::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read((int*)&m_eEvent);
	pStream->Read((int*)&m_iEventTriggeredId);
}

CvNetJoinGroup::CvNetJoinGroup() : CvMessageData(GAMEMESSAGE_JOIN_GROUP), m_ePlayer(NO_PLAYER), m_iUnitID(-1), m_iHeadID(-1)
{
}

CvNetJoinGroup::CvNetJoinGroup(PlayerTypes ePlayer, int iUnitID, int iHeadID) : CvMessageData(GAMEMESSAGE_JOIN_GROUP), m_ePlayer(ePlayer), m_iUnitID(iUnitID), m_iHeadID(iHeadID)
{
}

void CvNetJoinGroup::Debug(char* szAddendum) 
{ 
	sprintf(szAddendum, "Join Group unit %d -> head unit %d", m_iUnitID, m_iHeadID); 
}

void CvNetJoinGroup::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		CvUnit* pUnit = GET_PLAYER(m_ePlayer).getUnit(m_iUnitID);
		if (pUnit != NULL)
		{
			CvUnit* pHeadUnit = GET_PLAYER(m_ePlayer).getUnit(m_iHeadID);
			if (pHeadUnit != NULL)
			{
				pUnit->joinGroup(pHeadUnit->getGroup());
			}
			else
			{
				pUnit->joinGroup(NULL);
			}
		}
	}
}

void CvNetJoinGroup::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_iUnitID);
	pStream->Write(m_iHeadID);
}

void CvNetJoinGroup::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read(&m_iUnitID);
	pStream->Read(&m_iHeadID);
}

CvNetPushMission::CvNetPushMission() : CvMessageData(GAMEMESSAGE_PUSH_MISSION), m_ePlayer(NO_PLAYER), m_iUnitID(-1), m_eMission(NO_MISSION), m_iData1(-1), m_iData2(-1), m_iFlags(0), m_bShift(false)
{
}

CvNetPushMission::CvNetPushMission(PlayerTypes ePlayer, int iUnitID, MissionTypes eMission, int iData1, int iData2, int iFlags, bool bShift) : CvMessageData(GAMEMESSAGE_PUSH_MISSION), m_ePlayer(ePlayer), m_iUnitID(iUnitID), m_eMission(eMission), m_iData1(iData1), m_iData2(iData2), m_iFlags(iFlags), m_bShift(bShift)
{
}

void CvNetPushMission::Debug(char* szAddendum) 
{		
	sprintf(szAddendum, "Do Mission, who is %d, unit ID is %d, mission is %S", m_ePlayer, m_iUnitID, GC.getMissionInfo(m_eMission).getDescription());
}

void CvNetPushMission::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		CvUnit* pUnit = GET_PLAYER(m_ePlayer).getUnit(m_iUnitID);
		if (pUnit != NULL)
		{
			CvSelectionGroup* pSelectionGroup = pUnit->getGroup();
			if (pSelectionGroup != NULL)
			{
				pSelectionGroup->pushMission(m_eMission, m_iData1, m_iData2, m_iFlags, m_bShift, true);
			}
		}
	}
}

void CvNetPushMission::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_iUnitID);
	pStream->Write(m_eMission);
	pStream->Write(m_iData1);
	pStream->Write(m_iData2);
	pStream->Write(m_iFlags);
	pStream->Write(m_bShift);
}

void CvNetPushMission::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read(&m_iUnitID);
	pStream->Read((int*)&m_eMission);
	pStream->Read(&m_iData1);
	pStream->Read(&m_iData2);
	pStream->Read(&m_iFlags);
	pStream->Read(&m_bShift);
}

CvNetAutoMission::CvNetAutoMission() : CvMessageData(GAMEMESSAGE_AUTO_MISSION), m_ePlayer(NO_PLAYER), m_iUnitID(-1)
{
}

CvNetAutoMission::CvNetAutoMission(PlayerTypes ePlayer, int iUnitID) : CvMessageData(GAMEMESSAGE_AUTO_MISSION), m_ePlayer(ePlayer), m_iUnitID(iUnitID)
{
}

void CvNetAutoMission::Debug(char* szAddendum) 
{ 
	sprintf(szAddendum, "Auto Mission, unit ID is %d",	m_iUnitID); 
}

void CvNetAutoMission::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		CvUnit* pUnit = GET_PLAYER(m_ePlayer).getUnit(m_iUnitID);
		if (pUnit != NULL)
		{
			CvSelectionGroup* pSelectionGroup = pUnit->getGroup();
			if (pSelectionGroup != NULL)
			{
				pSelectionGroup->autoMission();
			}
		}
	}
}

void CvNetAutoMission::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_iUnitID);
}

void CvNetAutoMission::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read(&m_iUnitID);
}

CvNetDoCommand::CvNetDoCommand() : CvMessageData(GAMEMESSAGE_DO_COMMAND), m_ePlayer(NO_PLAYER), m_iUnitID(-1), m_eCommand(NO_COMMAND), m_iData1(-1), m_iData2(-1), m_bAlt(false) 
{
}

CvNetDoCommand::CvNetDoCommand(PlayerTypes ePlayer, int iUnitID, CommandTypes eCommand, int iData1, int iData2, bool bAlt) : CvMessageData(GAMEMESSAGE_DO_COMMAND), m_ePlayer(ePlayer), m_iUnitID(iUnitID), m_eCommand(eCommand), m_iData1(iData1), m_iData2(iData2), m_bAlt(bAlt) 
{
}

void CvNetDoCommand::Debug(char* szAddendum) 
{
	sprintf(szAddendum, "Do Command, who is %d, unit ID is %d, command is %d", m_ePlayer, m_iUnitID, m_eCommand);
}

void CvNetDoCommand::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		CvUnit* pUnit = GET_PLAYER(m_ePlayer).getUnit(m_iUnitID);
		if (pUnit != NULL)
		{
			if (m_bAlt && GC.getCommandInfo(m_eCommand).getAll())
			{
				int iLoop;
				for (CvUnit* pLoopUnit = GET_PLAYER(m_ePlayer).firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = GET_PLAYER(m_ePlayer).nextUnit(&iLoop))
				{
					if (pLoopUnit->getUnitType() == pUnit->getUnitType())
					{
						pLoopUnit->doCommand(m_eCommand, m_iData1, m_iData2);
					}
				}
			}
			else
			{
				pUnit->doCommand(m_eCommand, m_iData1, m_iData2);
			}
		}
	}
}

void CvNetDoCommand::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_iUnitID);
	pStream->Write(m_eCommand);
	pStream->Write(m_iData1);
	pStream->Write(m_iData2);
	pStream->Write(m_bAlt);
}

void CvNetDoCommand::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read(&m_iUnitID);
	pStream->Read((int*)&m_eCommand);
	pStream->Read(&m_iData1);
	pStream->Read(&m_iData2);
	pStream->Read(&m_bAlt);
}

CvNetPercentChange::CvNetPercentChange() : CvMessageData(GAMEMESSAGE_PERCENT_CHANGE), m_ePlayer(NO_PLAYER), m_iChange(0), m_eCommerce(NO_COMMERCE)
{
}

CvNetPercentChange::CvNetPercentChange(PlayerTypes ePlayer, CommerceTypes eCommerce, int iChange) : CvMessageData(GAMEMESSAGE_PERCENT_CHANGE), m_ePlayer(ePlayer), m_iChange(iChange), m_eCommerce(eCommerce)
{
}

void CvNetPercentChange::Debug(char* szAddendum) 
{ 
	sprintf(szAddendum, "Commerce: %d, Percent Change: %d", m_eCommerce, m_iChange); 
}

void CvNetPercentChange::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		GET_PLAYER(m_ePlayer).changeCommercePercent(m_eCommerce, m_iChange);
	}
}

void CvNetPercentChange::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_iChange);
	pStream->Write(m_eCommerce);
}

void CvNetPercentChange::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read(&m_iChange);
	pStream->Read((int*)&m_eCommerce);
}

CvNetChangeVassal::CvNetChangeVassal() : CvMessageData(GAMEMESSAGE_CHANGE_VASSAL), m_ePlayer(NO_PLAYER), m_eMasterTeam(NO_TEAM), m_bVassal(false), m_bCapitulated(false)
{
}

CvNetChangeVassal::CvNetChangeVassal(PlayerTypes ePlayer, TeamTypes eMasterTeam, bool bVassal, bool bCapitulated) : CvMessageData(GAMEMESSAGE_CHANGE_VASSAL), m_ePlayer(ePlayer), m_eMasterTeam(eMasterTeam), m_bVassal(bVassal), m_bCapitulated(bCapitulated)
{
}

void CvNetChangeVassal::Debug(char* szAddendum) 
{	
	sprintf(szAddendum, "Change Vassal, master team is %d", m_eMasterTeam);	
}

void CvNetChangeVassal::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		FAssert(GET_PLAYER(m_ePlayer).getTeam() != m_eMasterTeam);
		GET_TEAM(GET_PLAYER(m_ePlayer).getTeam()).setVassal(m_eMasterTeam, m_bVassal, m_bCapitulated);
	}
}

void CvNetChangeVassal::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_eMasterTeam);
	pStream->Write(m_bVassal);
	pStream->Write(m_bCapitulated);
}

void CvNetChangeVassal::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read((int*)&m_eMasterTeam);
	pStream->Read(&m_bVassal);
	pStream->Read(&m_bCapitulated);
}

CvNetChooseElection::CvNetChooseElection() : CvMessageData(GAMEMESSAGE_CHOOSE_ELECTION), m_ePlayer(NO_PLAYER), m_iSelection(-1), m_iVoteId(-1)
{
}

CvNetChooseElection::CvNetChooseElection(PlayerTypes ePlayer, int iSelection, int iVoteId) : CvMessageData(GAMEMESSAGE_CHOOSE_ELECTION), m_ePlayer(ePlayer), m_iSelection(iSelection), m_iVoteId(iVoteId)
{
}

void CvNetChooseElection::Debug(char* szAddendum) 
{	
	sprintf(szAddendum, "Choose Election: %d", m_iSelection);	
}

void CvNetChooseElection::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	GC.getGameINLINE().setVoteChosen(m_iSelection, m_iVoteId);
}

void CvNetChooseElection::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_iSelection);
	pStream->Write(m_iVoteId);
}

void CvNetChooseElection::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read(&m_iSelection);
	pStream->Read(&m_iVoteId);
}

CvNetDiploVote::CvNetDiploVote() : CvMessageData(GAMEMESSAGE_DIPLO_VOTE), m_ePlayer(NO_PLAYER), m_iVoteId(-1), m_eChoice(NO_PLAYER_VOTE) 
{
}

CvNetDiploVote::CvNetDiploVote(PlayerTypes ePlayer, int iVoteId, PlayerVoteTypes eChoice) : CvMessageData(GAMEMESSAGE_DIPLO_VOTE), m_ePlayer(ePlayer), m_iVoteId(iVoteId), m_eChoice(eChoice) 
{
}

void CvNetDiploVote::Debug(char* szAddendum) 
{	
	sprintf(szAddendum, "Diplo Vote for %d; Choice is %d", m_iVoteId, m_eChoice);	
}

void CvNetDiploVote::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	GC.getGameINLINE().castVote(m_ePlayer, m_iVoteId, m_eChoice);
}

void CvNetDiploVote::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_iVoteId);
	pStream->Write(m_eChoice);
}

void CvNetDiploVote::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read(&m_iVoteId);
	pStream->Read((int*)&m_eChoice);
}

CvNetChangeWar::CvNetChangeWar() : CvMessageData(GAMEMESSAGE_CHANGE_WAR), m_ePlayer(NO_PLAYER), m_eRivalTeam(NO_TEAM), m_bWar(false)
{
}

CvNetChangeWar::CvNetChangeWar(PlayerTypes ePlayer, TeamTypes eRivalTeam, bool bWar) : CvMessageData(GAMEMESSAGE_CHANGE_WAR), m_ePlayer(ePlayer), m_eRivalTeam(eRivalTeam), m_bWar(bWar)
{
}

void CvNetChangeWar::Debug(char* szAddendum) 
{	
	sprintf(szAddendum, "Change War, rival team is %d", m_eRivalTeam);	
}

void CvNetChangeWar::Execute()
{
	if (CvDecisionQueue::defer(this))
	{
		return;
	}

	if (m_ePlayer != NO_PLAYER)
	{
		FAssert(GET_PLAYER(m_ePlayer).getTeam() != m_eRivalTeam);
		if (m_bWar)
		{
			GET_TEAM(GET_PLAYER(m_ePlayer).getTeam()).declareWar(m_eRivalTeam, false, NO_WARPLAN);

			if (gDLL->isDiplomacy() && GET_PLAYER((PlayerTypes) gDLL->getDiplomacyPlayer()).getTeam() == m_eRivalTeam)
			{
				gDLL->endDiplomacy();
			}
		}
		else
		{
			GET_TEAM(GET_PLAYER(m_ePlayer).getTeam()).makePeace(m_eRivalTeam);
		}
	}
}

void CvNetChangeWar::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_eRivalTeam);
	pStream->Write(m_bWar);
}

void CvNetChangeWar::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read((int*)&m_eRivalTeam);
	pStream->Read(&m_bWar);
}

CvNetPing::CvNetPing(PlayerTypes ePlayer, int iX, int iY) : CvMessageData(GAMEMESSAGE_PING), m_ePlayer(ePlayer), m_iX(iX), m_iY(iY) 
{ 
}

void CvNetPing::Debug(char* szAddendum) 
{ 
	sprintf(szAddendum, "Ping message received");	
}

void CvNetPing::PutInBuffer(FDataStreamBase* pStream)
{
	pStream->Write(m_ePlayer);
	pStream->Write(m_iX);
	pStream->Write(m_iY);
}

void CvNetPing::SetFromBuffer(FDataStreamBase* pStream)
{
	pStream->Read((int*)&m_ePlayer);
	pStream->Read(&m_iX);
	pStream->Read(&m_iY);
}

void CvNetPing::Execute()
{
	if (GC.getGameINLINE().getActiveTeam() != NO_TEAM)
	{
		if (GET_PLAYER(m_ePlayer).getTeam() == GC.getGameINLINE().getActiveTeam() || GET_TEAM(GC.getGameINLINE().getActiveTeam()).isVassal(GET_PLAYER(m_ePlayer).getTeam()) || GET_TEAM(GET_PLAYER(m_ePlayer).getTeam()).isVassal(GC.getGameINLINE().getActiveTeam()))
		{
			gDLL->getInterfaceIFace()->doPing(m_iX, m_iY, m_ePlayer);
		}
	}
}



//
// The human's decisions while the AI's work ahead runs in the background. By default a decision
// stops the work and is done at once (stopForDecision()); WORK_AHEAD_QUEUE_DECISIONS=1 keeps it,
// as below.
//
// In a tier 3 build the work the AI players can do while a human takes their turn runs on worker
// threads while the game goes on drawing and taking input (CvGame::update(),
// CvPlayerAI::AI_startWorkAheadInBackground()). The workers read the game, so it must not change
// meanwhile: a decision of the human's (every one reaches the game as one of these messages, as
// in a network game, or as a deal through CvGame::implementDeal()) is kept here, the workers are
// asked to stop after the item in hand, and once they have, what was kept is done, before
// anything else.
//
// What was kept is done in lanes: the cities' production and the empire's choices first, then
// the cities' worked tiles and specialists, then the units' moves, commands and promotions, each
// lane in the order it came. A decision goes ahead of an earlier one only if they touch nothing in
// common: no city, unit or plot, and not both gold. A decision that reaches far (war, peace, a
// deal, a vote, an event, the end of the turn, a city given away or razed) is a barrier: what came
// before it is done before it, and what came after, after. DECISIONS_ARRIVAL_ORDER=1 does
// everything in the order it came.
//
// The screen shows the game as it was until what was kept is done, as in a network game while a
// message is on its way, so a decision cannot be made on what an earlier one will bring about (a
// city not founded yet); the lanes keep the order where it may matter all the same.
//
struct CvQueuedDecision
{
	CvMessageData* pMessage;	// a copy, or NULL for a deal:
	PlayerTypes eWho;
	PlayerTypes eOtherWho;
	std::vector<TradeData> aOur;
	std::vector<TradeData> aTheir;
	bool bOurList;
	bool bTheirList;
	bool bForce;

	int iLane;
	int aiPlot[3];				// the plots it touches (indices), -1 for none
	int iCityOwner;
	int iCityID;
	int iUnitOwner;
	int iUnitID;
	bool bGold;
	unsigned long long uiQueued;	// when it was kept (profile builds)
};

namespace
{
	enum DecisionLanes
	{
		LANE_BARRIER = -1,
		LANE_EMPIRE,
		LANE_CITIZENS,
		LANE_UNITS,
	};

	std::vector<CvQueuedDecision>* s_paQueue = NULL;	// made on first use
	bool s_bPlayingBack = false;

	void touchPlot(CvQueuedDecision& kDecision, const CvPlot* pPlot)
	{
		if (pPlot == NULL)
		{
			return;
		}

		const int iIndex = GC.getMapINLINE().plotNumINLINE(pPlot->getX_INLINE(), pPlot->getY_INLINE());

		for (int iI = 0; iI < 3; iI++)
		{
			if (kDecision.aiPlot[iI] == iIndex)
			{
				return;
			}
			if (kDecision.aiPlot[iI] == -1)
			{
				kDecision.aiPlot[iI] = iIndex;
				return;
			}
		}
	}

	const CvCity* touchCity(CvQueuedDecision& kDecision, PlayerTypes ePlayer, int iCityID)
	{
		kDecision.iCityOwner = ePlayer;
		kDecision.iCityID = iCityID;
		const CvCity* pCity = (ePlayer != NO_PLAYER) ? GET_PLAYER(ePlayer).getCity(iCityID) : NULL;

		if (pCity != NULL)
		{
			touchPlot(kDecision, pCity->plot());
		}

		return pCity;
	}

	void touchUnit(CvQueuedDecision& kDecision, PlayerTypes ePlayer, int iUnitID)
	{
		kDecision.iUnitOwner = ePlayer;
		kDecision.iUnitID = iUnitID;
		const CvUnit* pUnit = (ePlayer != NO_PLAYER) ? GET_PLAYER(ePlayer).getUnit(iUnitID) : NULL;

		if (pUnit != NULL)
		{
			touchPlot(kDecision, pUnit->plot());
		}
	}

	bool touchesSame(const CvQueuedDecision& kA, const CvQueuedDecision& kB)
	{
		if (kA.bGold && kB.bGold)
		{
			return true;
		}

		if ((kA.iCityID != -1) && (kA.iCityOwner == kB.iCityOwner) && (kA.iCityID == kB.iCityID))
		{
			return true;
		}

		if ((kA.iUnitID != -1) && (kA.iUnitOwner == kB.iUnitOwner) && (kA.iUnitID == kB.iUnitID))
		{
			return true;
		}

		for (int iI = 0; iI < 3; iI++)
		{
			for (int iJ = 0; (kA.aiPlot[iI] != -1) && (iJ < 3); iJ++)
			{
				if (kA.aiPlot[iI] == kB.aiPlot[iJ])
				{
					return true;
				}
			}
		}

		return false;
	}

	// WORK_AHEAD_QUEUE_DECISIONS=1: a decision made while the work ahead runs is kept and done
	// after it, at the exe's next call of CvGame::update(), in lanes. Unset, the work is brought
	// to its end when the decision comes (CvPlayerAI::AI_settleWorkAhead(): the workers get a few
	// milliseconds to finish, and are stopped only if that is not enough) and the decision is
	// done at once: the exe calls CvGame::update() only about four
	// times a second during a human's turn, so a kept decision waited up to a quarter of a second
	// (122 ms on average in the first playtest).
	bool isQueueDecisions()
	{
		static int iQueue = -1;
		if (iQueue == -1)
		{
			iQueue = ((GC.getDefineINT("WORK_AHEAD_QUEUE_DECISIONS") != 0) ? 1 : 0);
		}
		return (iQueue != 0);
	}

	// Not queueing: the work ahead stopped for a decision, which is then done at once.
	bool stopForDecision()
	{
		if (isQueueDecisions())
		{
			return false;
		}

#ifdef BRIDGED_PROFILE
		static BridgedProfileSample kStopped = { "decision: work ahead stopped for it, time waited" };
		BridgedProfileManual kTimer(&kStopped);
#endif
		CvDecisionQueue::settle();
		return true;
	}

	bool isArrivalOrder()
	{
		static int iArrival = -1;
		if (iArrival == -1)
		{
			iArrival = ((GC.getDefineINT("DECISIONS_ARRIVAL_ORDER") != 0) ? 1 : 0);
		}
		return (iArrival != 0);
	}

	void clearDecision(CvQueuedDecision& kDecision)
	{
		kDecision.pMessage = NULL;
		kDecision.eWho = NO_PLAYER;
		kDecision.eOtherWho = NO_PLAYER;
		kDecision.bOurList = false;
		kDecision.bTheirList = false;
		kDecision.bForce = false;
		kDecision.iLane = LANE_BARRIER;
		kDecision.aiPlot[0] = kDecision.aiPlot[1] = kDecision.aiPlot[2] = -1;
		kDecision.iCityOwner = NO_PLAYER;
		kDecision.iCityID = -1;
		kDecision.iUnitOwner = NO_PLAYER;
		kDecision.iUnitID = -1;
		kDecision.bGold = false;
#ifdef BRIDGED_PROFILE
		kDecision.uiQueued = bridgedProfileTicks();
#else
		kDecision.uiQueued = 0;
#endif
	}

	void keep(CvQueuedDecision& kDecision)
	{
		if (s_paQueue == NULL)
		{
			s_paQueue = new std::vector<CvQueuedDecision>();
		}

		s_paQueue->push_back(kDecision);
		BridgedThreads::requestStop();	// the workers stop after the item in hand (CvGame::update())

#ifdef BRIDGED_PROFILE
		static BridgedProfileSample kKept = { "decision queue: decisions kept while the work ahead ran" };
		bridgedProfileAdd(&kKept, 0);
#endif
	}
}

CvMessageData* CvDecisionQueue::copy(const CvMessageData* pMessage)
{
	switch (pMessage->getType())
	{
	case GAMEMESSAGE_EXTENDED_GAME: return new CvNetExtendedGame(*(const CvNetExtendedGame*)pMessage);
	case GAMEMESSAGE_AUTO_MOVES: return new CvNetAutoMoves(*(const CvNetAutoMoves*)pMessage);
	case GAMEMESSAGE_TURN_COMPLETE: return new CvNetTurnComplete(*(const CvNetTurnComplete*)pMessage);
	case GAMEMESSAGE_PUSH_ORDER: return new CvNetPushOrder(*(const CvNetPushOrder*)pMessage);
	case GAMEMESSAGE_POP_ORDER: return new CvNetPopOrder(*(const CvNetPopOrder*)pMessage);
	case GAMEMESSAGE_DO_TASK: return new CvNetDoTask(*(const CvNetDoTask*)pMessage);
	case GAMEMESSAGE_UPDATE_CIVICS: return new CvNetUpdateCivics(*(const CvNetUpdateCivics*)pMessage);
	case GAMEMESSAGE_RESEARCH: return new CvNetResearch(*(const CvNetResearch*)pMessage);
	case GAMEMESSAGE_ESPIONAGE_CHANGE: return new CvNetEspionageChange(*(const CvNetEspionageChange*)pMessage);
	case GAMEMESSAGE_ADVANCED_START_ACTION: return new CvNetAdvancedStartAction(*(const CvNetAdvancedStartAction*)pMessage);
	case GAMEMESSAGE_MOD_NET_MESSAGE: return new CvNetModNetMessage(*(const CvNetModNetMessage*)pMessage);
	case GAMEMESSAGE_CONVERT: return new CvNetConvert(*(const CvNetConvert*)pMessage);
	case GAMEMESSAGE_EMPIRE_SPLIT: return new CvNetEmpireSplit(*(const CvNetEmpireSplit*)pMessage);
	case GAMEMESSAGE_FOUND_RELIGION: return new CvNetFoundReligion(*(const CvNetFoundReligion*)pMessage);
	case GAMEMESSAGE_LAUNCH_SPACESHIP: return new CvNetLaunchSpaceship(*(const CvNetLaunchSpaceship*)pMessage);
	case GAMEMESSAGE_EVENT_TRIGGERED: return new CvNetEventTriggered(*(const CvNetEventTriggered*)pMessage);
	case GAMEMESSAGE_JOIN_GROUP: return new CvNetJoinGroup(*(const CvNetJoinGroup*)pMessage);
	case GAMEMESSAGE_PUSH_MISSION: return new CvNetPushMission(*(const CvNetPushMission*)pMessage);
	case GAMEMESSAGE_AUTO_MISSION: return new CvNetAutoMission(*(const CvNetAutoMission*)pMessage);
	case GAMEMESSAGE_DO_COMMAND: return new CvNetDoCommand(*(const CvNetDoCommand*)pMessage);
	case GAMEMESSAGE_PERCENT_CHANGE: return new CvNetPercentChange(*(const CvNetPercentChange*)pMessage);
	case GAMEMESSAGE_CHANGE_VASSAL: return new CvNetChangeVassal(*(const CvNetChangeVassal*)pMessage);
	case GAMEMESSAGE_CHOOSE_ELECTION: return new CvNetChooseElection(*(const CvNetChooseElection*)pMessage);
	case GAMEMESSAGE_DIPLO_VOTE: return new CvNetDiploVote(*(const CvNetDiploVote*)pMessage);
	case GAMEMESSAGE_CHANGE_WAR: return new CvNetChangeWar(*(const CvNetChangeWar*)pMessage);
	default: return NULL;
	}
}

// The lane of a decision and what it touches, as the game stands when it is made.
void CvDecisionQueue::describe(CvQueuedDecision& kDecision)
{
	const CvMessageData* pMessage = kDecision.pMessage;

	if (pMessage == NULL)
	{
		kDecision.iLane = LANE_BARRIER;		// a deal
		return;
	}

	switch (pMessage->getType())
	{
	case GAMEMESSAGE_PUSH_ORDER:
		{
			const CvNetPushOrder& kMessage = *(const CvNetPushOrder*)pMessage;
			kDecision.iLane = LANE_EMPIRE;
			touchCity(kDecision, kMessage.m_ePlayer, kMessage.m_iCityID);
		}
		break;

	case GAMEMESSAGE_POP_ORDER:
		{
			const CvNetPopOrder& kMessage = *(const CvNetPopOrder*)pMessage;
			kDecision.iLane = LANE_EMPIRE;
			touchCity(kDecision, kMessage.m_ePlayer, kMessage.m_iCityID);
		}
		break;

	case GAMEMESSAGE_DO_TASK:
		{
			const CvNetDoTask& kMessage = *(const CvNetDoTask*)pMessage;
			const CvCity* pCity = touchCity(kDecision, kMessage.m_ePlayer, kMessage.m_iCityID);

			switch (kMessage.m_eTask)
			{
			case TASK_RAZE:
			case TASK_DISBAND:
			case TASK_GIFT:
			case TASK_LIBERATE:
				kDecision.iLane = LANE_BARRIER;
				break;

			case TASK_CHANGE_WORKING_PLOT:
				kDecision.iLane = LANE_CITIZENS;
				if (pCity != NULL)
				{
					touchPlot(kDecision, pCity->getCityIndexPlot(kMessage.m_iData1));
				}
				break;

			case TASK_SET_AUTOMATED_CITIZENS:
			case TASK_SET_EMPHASIZE:
			case TASK_CHANGE_SPECIALIST:
			case TASK_CLEAR_WORKING_OVERRIDE:
				kDecision.iLane = LANE_CITIZENS;
				break;

			case TASK_HURRY:
				kDecision.iLane = LANE_EMPIRE;
				kDecision.bGold = true;
				break;

			default:
				kDecision.iLane = LANE_EMPIRE;
				break;
			}
		}
		break;

	case GAMEMESSAGE_UPDATE_CIVICS:
	case GAMEMESSAGE_RESEARCH:
	case GAMEMESSAGE_ESPIONAGE_CHANGE:
	case GAMEMESSAGE_PERCENT_CHANGE:
		kDecision.iLane = LANE_EMPIRE;
		break;

	case GAMEMESSAGE_JOIN_GROUP:
		{
			const CvNetJoinGroup& kMessage = *(const CvNetJoinGroup*)pMessage;
			kDecision.iLane = LANE_UNITS;
			touchUnit(kDecision, kMessage.m_ePlayer, kMessage.m_iUnitID);
		}
		break;

	case GAMEMESSAGE_PUSH_MISSION:
		{
			const CvNetPushMission& kMessage = *(const CvNetPushMission*)pMessage;
			kDecision.iLane = LANE_UNITS;
			touchUnit(kDecision, kMessage.m_ePlayer, kMessage.m_iUnitID);

			switch (kMessage.m_eMission)
			{
			case MISSION_MOVE_TO:
			case MISSION_ROUTE_TO:
			case MISSION_AIRLIFT:
			case MISSION_NUKE:
			case MISSION_RECON:
			case MISSION_PARADROP:
			case MISSION_AIRBOMB:
			case MISSION_RANGE_ATTACK:
				touchPlot(kDecision, GC.getMapINLINE().plot(kMessage.m_iData1, kMessage.m_iData2));
				break;

			case MISSION_SPREAD_CORPORATION:
			case MISSION_TRADE:
			case MISSION_ESPIONAGE:
				kDecision.bGold = true;
				break;

			default:
				break;
			}
		}
		break;

	case GAMEMESSAGE_AUTO_MISSION:
		{
			const CvNetAutoMission& kMessage = *(const CvNetAutoMission*)pMessage;
			kDecision.iLane = LANE_UNITS;
			touchUnit(kDecision, kMessage.m_ePlayer, kMessage.m_iUnitID);
		}
		break;

	case GAMEMESSAGE_DO_COMMAND:
		{
			const CvNetDoCommand& kMessage = *(const CvNetDoCommand*)pMessage;
			kDecision.iLane = LANE_UNITS;
			touchUnit(kDecision, kMessage.m_ePlayer, kMessage.m_iUnitID);
			kDecision.bGold = (kMessage.m_eCommand == COMMAND_UPGRADE);
		}
		break;

	default:
		kDecision.iLane = LANE_BARRIER;		// the end of the turn, war, votes, events, Python's messages
		break;
	}
}

bool CvDecisionQueue::defer(CvMessageData* pMessage)
{
	if (s_bPlayingBack || !CvPlayerAI::AI_isWorkAheadUnderWay() || stopForDecision())
	{
		return false;
	}

	CvQueuedDecision kDecision;
	clearDecision(kDecision);
	kDecision.pMessage = copy(pMessage);

	if (kDecision.pMessage == NULL)
	{
		// not known here: the work ahead is stopped and what was kept done, then this one
		settle();
		return false;
	}

	describe(kDecision);
	keep(kDecision);
	return true;
}

bool CvDecisionQueue::deferDeal(PlayerTypes eWho, PlayerTypes eOtherWho, CLinkList<TradeData>* pOurList, CLinkList<TradeData>* pTheirList, bool bForce)
{
	if (s_bPlayingBack || !CvPlayerAI::AI_isWorkAheadUnderWay() || stopForDecision())
	{
		return false;
	}

	CvQueuedDecision kDecision;
	clearDecision(kDecision);
	kDecision.eWho = eWho;
	kDecision.eOtherWho = eOtherWho;
	kDecision.bForce = bForce;
	kDecision.bOurList = (pOurList != NULL);
	kDecision.bTheirList = (pTheirList != NULL);

	if (pOurList != NULL)
	{
		for (CLLNode<TradeData>* pNode = pOurList->head(); pNode != NULL; pNode = pOurList->next(pNode))
		{
			kDecision.aOur.push_back(pNode->m_data);
		}
	}

	if (pTheirList != NULL)
	{
		for (CLLNode<TradeData>* pNode = pTheirList->head(); pNode != NULL; pNode = pTheirList->next(pNode))
		{
			kDecision.aTheir.push_back(pNode->m_data);
		}
	}

	describe(kDecision);
	keep(kDecision);
	return true;
}

bool CvDecisionQueue::hasQueued()
{
	return (s_paQueue != NULL) && !s_paQueue->empty();
}

void CvDecisionQueue::play(CvQueuedDecision& kDecision)
{
#ifdef BRIDGED_PROFILE
	static BridgedProfileSample kWaited = { "decision queue: decision done, time held" };
	bridgedProfileAdd(&kWaited, bridgedProfileTicks() - kDecision.uiQueued);
#endif

	if (kDecision.pMessage != NULL)
	{
		kDecision.pMessage->Execute();
		SAFE_DELETE(kDecision.pMessage);
	}
	else
	{
		CLinkList<TradeData> kOur;
		CLinkList<TradeData> kTheir;

		for (int iI = 0; iI < (int)kDecision.aOur.size(); iI++)
		{
			kOur.insertAtEnd(kDecision.aOur[iI]);
		}
		for (int iI = 0; iI < (int)kDecision.aTheir.size(); iI++)
		{
			kTheir.insertAtEnd(kDecision.aTheir[iI]);
		}

		GC.getGameINLINE().implementDeal(kDecision.eWho, kDecision.eOtherWho, kDecision.bOurList ? &kOur : NULL, kDecision.bTheirList ? &kTheir : NULL, kDecision.bForce);
	}
}

void CvDecisionQueue::playBack()
{
	if (!hasQueued() || s_bPlayingBack)
	{
		return;
	}

	FAssert(!CvPlayerAI::AI_isWorkAheadUnderWay());
	PROFILE_FUNC();

	std::vector<CvQueuedDecision> aQueue;
	aQueue.swap(*s_paQueue);
	s_bPlayingBack = true;

	const int iCount = (int)aQueue.size();
	const bool bArrival = isArrivalOrder();
	std::vector<bool> abDone(iCount, false);
	int iStart = 0;

	while (iStart < iCount)
	{
		int iEnd = iStart;
		while ((iEnd < iCount) && (aQueue[iEnd].iLane != LANE_BARRIER))
		{
			iEnd++;
		}

		// iStart to iEnd: one decision at a time, the first that may go in the lowest lane
		for (int iLeft = iEnd - iStart; iLeft > 0; iLeft--)
		{
			int iBest = -1;

			for (int iJ = iStart; iJ < iEnd; iJ++)
			{
				if (abDone[iJ])
				{
					continue;
				}

				if (bArrival)
				{
					iBest = iJ;
					break;
				}

				bool bReady = true;
				for (int iK = iStart; bReady && (iK < iJ); iK++)
				{
					if (!abDone[iK] && ((aQueue[iK].iLane <= aQueue[iJ].iLane) || touchesSame(aQueue[iK], aQueue[iJ])))
					{
						bReady = false;
					}
				}

				if (bReady && ((iBest == -1) || (aQueue[iJ].iLane < aQueue[iBest].iLane)))
				{
					iBest = iJ;
				}
			}

			FAssert(iBest != -1);
#ifdef BRIDGED_PROFILE
			if (iBest != iStart)
			{
				static BridgedProfileSample kAhead = { "decision queue: decision done ahead of an earlier one" };
				bridgedProfileAdd(&kAhead, 0);
			}
#endif
			play(aQueue[iBest]);
			abDone[iBest] = true;

			while ((iStart < iEnd) && abDone[iStart])
			{
				iStart++;
			}
		}

		if (iEnd < iCount)
		{
			play(aQueue[iEnd]);		// a barrier
			abDone[iEnd] = true;
		}

		iStart = iEnd + 1;
	}

	s_bPlayingBack = false;
}

void CvDecisionQueue::settle()
{
	CvPlayerAI::AI_settleWorkAhead();	// finished if the workers are nearly done, stopped if not
	playBack();
}

void CvDecisionQueue::discardAll()
{
	CvPlayerAI::AI_discardWorkAhead();

	if (s_paQueue != NULL)
	{
		for (int iI = 0; iI < (int)s_paQueue->size(); iI++)
		{
			SAFE_DELETE((*s_paQueue)[iI].pMessage);
		}
		s_paQueue->clear();
	}

	s_bPlayingBack = false;
}
