#pragma once

// cityAI.h

#ifndef CIV4_CITY_AI_H
#define CIV4_CITY_AI_H

#include "CvCity.h"

typedef std::vector<std::pair<UnitAITypes, int> > UnitTypeWeightArray;

// final: nothing derives from it, so a call through a CvCityAI is bound at compile time
class CvCityAI final : public CvCity
{

public:

	CvCityAI();
	virtual ~CvCityAI();

	void AI_init();
	void AI_uninit();
	void AI_reset();

	void AI_doTurn();

	void AI_assignWorkingPlots();
	void AI_updateAssignWork();

	bool AI_avoidGrowth();
	bool AI_ignoreGrowth();
	int AI_specialistValue(SpecialistTypes eSpecialist, bool bAvoidGrowth, bool bRemove);

	void AI_chooseProduction();

	UnitTypes AI_bestUnit(bool bAsync = false, AdvisorTypes eIgnoreAdvisor = NO_ADVISOR, UnitAITypes* peBestUnitAI = NULL);
	UnitTypes AI_bestUnitAI(UnitAITypes eUnitAI, bool bAsync = false, AdvisorTypes eIgnoreAdvisor = NO_ADVISOR);

	BuildingTypes AI_bestBuilding(int iFocusFlags = 0, int iMaxTurns = 0, bool bAsync = false, AdvisorTypes eIgnoreAdvisor = NO_ADVISOR);
	BuildingTypes AI_bestBuildingThreshold(int iFocusFlags = 0, int iMaxTurns = 0, int iMinThreshold = 0, bool bAsync = false, AdvisorTypes eIgnoreAdvisor = NO_ADVISOR);
	
	int AI_buildingValue(BuildingTypes eBuilding, int iFocusFlags = 0);
	int AI_buildingValueThreshold(BuildingTypes eBuilding, int iFocusFlags = 0, int iThreshold = 0);

	ProjectTypes AI_bestProject();
	int AI_projectValue(ProjectTypes eProject);

	ProcessTypes AI_bestProcess();
	ProcessTypes AI_bestProcess(CommerceTypes eCommerceType);
	int AI_processValue(ProcessTypes eProcess);
	int AI_processValue(ProcessTypes eProcess, CommerceTypes eCommerceType);

	int AI_neededSeaWorkers();

	bool AI_isDefended(int iExtra = 0);
	bool AI_isAirDefended(int iExtra = 0);
	bool AI_isDanger();

	int AI_neededDefenders();
	int AI_neededAirDefenders();
	int AI_minDefenders();
	int AI_neededFloatingDefenders();
	void AI_updateNeededFloatingDefenders();

	int AI_getEmphasizeAvoidGrowthCount();
	bool AI_isEmphasizeAvoidGrowth();

	int AI_getEmphasizeGreatPeopleCount();
	bool AI_isEmphasizeGreatPeople();

	bool AI_isAssignWorkDirty();
	void AI_setAssignWorkDirty(bool bNewValue);

	bool AI_isChooseProductionDirty();
	void AI_setChooseProductionDirty(bool bNewValue);

	CvCity* AI_getRouteToCity() const;
	void AI_updateRouteToCity();

	int AI_getEmphasizeYieldCount(YieldTypes eIndex);
	bool AI_isEmphasizeYield(YieldTypes eIndex);

	int AI_getEmphasizeCommerceCount(CommerceTypes eIndex);
	bool AI_isEmphasizeCommerce(CommerceTypes eIndex);

	bool AI_isEmphasize(EmphasizeTypes eIndex);
	void AI_setEmphasize(EmphasizeTypes eIndex, bool bNewValue);
	void AI_forceEmphasizeCulture(bool bNewValue);

	int AI_getBestBuildValue(int iIndex);
	int AI_totalBestBuildValue(CvArea* pArea);

	int AI_clearFeatureValue(int iIndex);
	BuildTypes AI_getBestBuild(int iIndex);
	int AI_countBestBuilds(CvArea* pArea);
	void AI_updateBestBuild();
	bool AI_isBestBuildKept() const;
	bool AI_isBestBuildWantedAhead() const;

	virtual int AI_cityValue() const;
    
    int AI_calculateWaterWorldPercent();
    
    int AI_getCityImportance(bool bEconomy, bool bMilitary);
    
    int AI_yieldMultiplier(YieldTypes eYield);
    void AI_updateSpecialYieldMultiplier();
    int AI_specialYieldMultiplier(YieldTypes eYield);
    
    int AI_countNumBonuses(BonusTypes eBonus, bool bIncludeOurs, bool bIncludeNeutral, int iOtherCultureThreshold, bool bLand = true, bool bWater = true);

	int AI_playerCloseness(PlayerTypes eIndex, int iMaxDistance);
	int AI_cityThreat(bool bDangerPercent = false);
	
	int AI_getWorkersHave();
	int AI_getWorkersNeeded();
	void AI_changeWorkersHave(int iChange);
	BuildingTypes AI_bestAdvancedStartBuilding(int iPass);
	
	void read(FDataStreamBase* pStream);
	void write(FDataStreamBase* pStream);

protected:

	int m_iEmphasizeAvoidGrowthCount;
	int m_iEmphasizeGreatPeopleCount;

	bool m_bAssignWorkDirty;
	// City-wide values AI_yieldValue() needs, kept while AI_assignWorkingPlots() runs: see AI_assignCached()
	enum AssignCacheTypes
	{
		ASSIGN_CACHE_FOOD_DIFFERENCE,
		ASSIGN_CACHE_HEALTH_LEVEL,
		ASSIGN_CACHE_HAPPINESS_LEVEL,
		ASSIGN_CACHE_BEST_FOOD_YIELD,
		ASSIGN_CACHE_GOOD_TILES,
		ASSIGN_CACHE_GOOD_TILES_HEALTHY,
		ASSIGN_CACHE_GOOD_SPECIALISTS,
		ASSIGN_CACHE_GOOD_SPECIALISTS_HEALTHY,
		NUM_ASSIGN_CACHE_TYPES
	};
	bool m_bAssignCacheActive;
	int m_iAssignCacheChangeCount;
	bool m_abAssignCacheValid[NUM_ASSIGN_CACHE_TYPES];
	int m_aiAssignCacheValue[NUM_ASSIGN_CACHE_TYPES];

	int AI_assignCached(AssignCacheTypes eType);

	// What AI_yieldValue() reads about the city and its owner that does not change when
	// a plot is worked or a specialist assigned: see AI_yieldValueContext()
	struct YieldValueContext
	{
		int aiBaseYieldRateModifier[NUM_YIELD_TYPES];
		int iProductionModifier;
		int aiTotalCommerceRateModifier[NUM_COMMERCE_TYPES];
		int aiCommercePercent[NUM_COMMERCE_TYPES];
		int aiProcessCommerceModifier[NUM_COMMERCE_TYPES];	// 0 unless bProductionProcess
		int aiCommerceWeight[NUM_COMMERCE_TYPES];			// with the city's commerce emphasis
		int aiAverageCommerceExchange[NUM_COMMERCE_TYPES];
		int aiAverageYieldMultiplier[NUM_YIELD_TYPES];
		int aiSpecialYieldMultiplier[NUM_YIELD_TYPES];
		int iFood;
		int iGrowthThreshold;
		int iPopulation;
		int iMilitaryHappinessUnits;
		int iNumCities;
		int iNoMilitaryPercentAnger;
		int iHurryAngerTimer;
		int iFlatHurryAngerLength;
		int iConscriptAngerTimer;
		int iFlatConscriptAngerLength;
		int iDefyResolutionAngerTimer;
		int iFlatDefyResolutionAngerLength;
		int iMaxFoodKeptPercent;
		int iHurryCostModifier;
		int abEmphasizeYield[NUM_YIELD_TYPES];
		int bEmphasizeGreatPeople;
		int bFoodIsProduction;
		int bCanPopRush;
		int bProductionProcess;
		int bProduction;
		int bNoUnhappiness;
		int bHuman;
		int bLowCultureLevel;
	};
	int m_iYieldValueContextDepth;
	bool m_bYieldValueContextValid;
	YieldValueContext m_kYieldValueContext;

	void AI_calculateYieldValueContext(YieldValueContext& kContext);
	const YieldValueContext& AI_yieldValueContext(YieldValueContext& kLocal);
	void AI_beginYieldValueContext();

	// The values AI_buildingValueThreshold() works from that do not depend on the building:
	// see AI_buildingValueContext()
	struct BuildingValueContext
	{
		bool bAreaAlone;
		int iHasMetCount;
		int iFoodDifference;
		int iHappinessLevel;
		int iHealthLevel;
		int iTotalPopulation;
		int iNumCities;
		int iNumCitiesInArea;
		int iProductionRank;
		int iCultureRank;
		int iCulturalVictoryNumCultureCities;
		bool bFinancialTrouble;
		bool bCulturalVictory1;
		bool bCulturalVictory2;
		bool bCulturalVictory3;
		bool bCanPopRush;
		bool bForeignTrade;

		bool isSame(const BuildingValueContext& kOther) const;
	};
	bool m_bBuildingValueContextActive;
	bool m_bBuildingValueContextValid;
	BuildingValueContext m_kBuildingValueContext;

	void AI_calculateBuildingValueContext(BuildingValueContext& kContext);
	const BuildingValueContext& AI_buildingValueContext(BuildingValueContext& kLocal);
	int AI_calculateAssignCached(AssignCacheTypes eType);
	bool m_bChooseProductionDirty;

	IDInfo m_routeToCity;

	int* m_aiEmphasizeYieldCount;
	int* m_aiEmphasizeCommerceCount;
	bool m_bForceEmphasizeCulture;

	int m_aiBestBuildValue[NUM_CITY_PLOTS];

	BuildTypes m_aeBestBuild[NUM_CITY_PLOTS];
	int m_iBestBuildTurn;		// the game turn AI_updateBestBuild() last ran; not saved

	bool* m_pbEmphasize;
	
	int* m_aiSpecialYieldMultiplier;
	
	int m_iCachePlayerClosenessTurn;
	int m_iCachePlayerClosenessDistance;
	int* m_aiPlayerCloseness;
	
	int m_iNeededFloatingDefenders;
	int m_iNeededFloatingDefendersCacheTurn;
	
	int m_iWorkersNeeded;
	int m_iWorkersHave;
	

	void AI_doDraft(bool bForce = false);
	void AI_doHurry(bool bForce = false);
	void AI_doEmphasize();
	int AI_getHappyFromHurry(HurryTypes eHurry);
	int AI_getHappyFromHurry(HurryTypes eHurry, UnitTypes eUnit, bool bIgnoreNew);
	int AI_getHappyFromHurry(HurryTypes eHurry, BuildingTypes eBuilding, bool bIgnoreNew);
	int AI_getHappyFromHurry(int iHurryPopulation);
	bool AI_doPanic();
	int AI_calculateCulturePressure(bool bGreatWork = false);

	
	bool AI_chooseUnit(UnitAITypes eUnitAI = NO_UNITAI);
	bool AI_chooseUnit(UnitTypes eUnit, UnitAITypes eUnitAI);
	
	bool AI_chooseDefender();
	bool AI_chooseLeastRepresentedUnit(UnitTypeWeightArray &allowedTypes);
	bool AI_chooseBuilding(int iFocusFlags = 0, int iMaxTurns = MAX_INT, int iMinThreshold = 0);
	bool AI_chooseProject();
	bool AI_chooseProcess(CommerceTypes eCommerceType = NO_COMMERCE);

	bool AI_bestSpreadUnit(bool bMissionary, bool bExecutive, int iBaseChance, UnitTypes* eBestSpreadUnit, int* iBestSpreadUnitValue);
	bool AI_addBestCitizen(bool bWorkers, bool bSpecialists, int* piBestPlot = NULL, SpecialistTypes* peBestSpecialist = NULL);
	bool AI_removeWorstCitizen(SpecialistTypes eIgnoreSpecialist = NO_SPECIALIST);
	void AI_juggleCitizens();

	bool AI_potentialPlot(short* piYields);
	bool AI_foodAvailable(int iExtra = 0);
	int AI_yieldValue(short* piYields, short* piCommerceYields, bool bAvoidGrowth, bool bRemove, bool bIgnoreFood = false, bool bIgnoreGrowth = false, bool bIgnoreStarvation = false, bool bWorkerOptimization = false);
	int AI_plotValue(CvPlot* pPlot, bool bAvoidGrowth, bool bRemove, bool bIgnoreFood = false, bool bIgnoreGrowth = false, bool bIgnoreStarvation = false);

	int AI_experienceWeight();
	int AI_buildUnitProb();

	void AI_bestPlotBuild(CvPlot* pPlot, int* piBestValue, BuildTypes* peBestBuild, int iFoodPriority, int iProductionPriority, int iCommercePriority, bool bChop, int iHappyAdjust, int iHealthAdjust, int iFoodChange);
	
	void AI_buildGovernorChooseProduction();
	
	int AI_getYieldMagicValue(const int* piYieldsTimes100, bool bHealthy);
	int AI_getPlotMagicValue(CvPlot* pPlot, bool bHealthy, bool bWorkerOptimization = false);
	int AI_countGoodTiles(bool bHealthy, bool bUnworkedOnly, int iThreshold = 50, bool bWorkerOptimization = false);
	int AI_countGoodSpecialists(bool bHealthy);
	int AI_calculateTargetCulturePerTurn();
	
	void AI_stealPlots();
	
	int AI_buildingSpecialYieldChangeValue(BuildingTypes kBuilding, YieldTypes eYield);
	
	void AI_cachePlayerCloseness(int iMaxDistance);
	void AI_updateWorkersNeededHere();

	// added so under cheat mode we can call protected functions for testing
	friend class CvGameTextMgr;
};

#endif
