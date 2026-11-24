#include "CvGameCoreDLL.h"
#include "CvInfoTables.h"
#include "CvGlobals.h"
#include "CvInfos.h"
#include "CvTeamAI.h"
#include "CvGameCoreUtils.h"
#include "FProfiler.h"
#include <algorithm>

// see CvInfoTables.h

namespace
{
	enum
	{
		COUNT_BUILDS,
		COUNT_IMPROVEMENTS,
		COUNT_ROUTES,
		COUNT_UNITS,
		COUNT_UNIT_CLASSES,
		COUNT_CIVILIZATIONS,
		COUNT_BUILDINGS,
		COUNT_BUILDING_CLASSES,
		COUNT_TECHS,
		COUNT_BONUSES,
		NUM_COUNTS
	};

	struct Tables
	{
		int aiCounts[NUM_COUNTS];
		std::vector<CvRouteBuild> akRouteBuilds;
		std::vector<std::vector<BuildTypes> > aaeImprovementBuilds;
		std::vector<ImprovementTypes> aeFinalImprovement;
		std::vector<std::vector<UnitTypes> > aaeUpgradeUnits;	// [civilization * units + unit]
		std::vector<std::vector<BuildingClassTypes> > aaePrereqBuildingClasses;
	};

	struct TeamTechs
	{
		bool bValid;
		std::vector<unsigned char> abBuild;
		std::vector<unsigned char> abUnit;
		std::vector<unsigned char> abBuilding;
		std::vector<int> aiDiscoverableBonuses;	// per improvement
	};

	Tables s_kTables;
	bool s_bTablesBuilt = false;
	TeamTechs s_akTeamTechs[MAX_TEAMS];

	void currentCounts(int* aiCounts)
	{
		aiCounts[COUNT_BUILDS] = GC.getNumBuildInfos();
		aiCounts[COUNT_IMPROVEMENTS] = GC.getNumImprovementInfos();
		aiCounts[COUNT_ROUTES] = GC.getNumRouteInfos();
		aiCounts[COUNT_UNITS] = GC.getNumUnitInfos();
		aiCounts[COUNT_UNIT_CLASSES] = GC.getNumUnitClassInfos();
		aiCounts[COUNT_CIVILIZATIONS] = GC.getNumCivilizationInfos();
		aiCounts[COUNT_BUILDINGS] = GC.getNumBuildingInfos();
		aiCounts[COUNT_BUILDING_CLASSES] = GC.getNumBuildingClassInfos();
		aiCounts[COUNT_TECHS] = GC.getNumTechInfos();
		aiCounts[COUNT_BONUSES] = GC.getNumBonusInfos();
	}

	bool betterRoute(const CvRouteBuild& kFirst, const CvRouteBuild& kSecond)
	{
		return (kFirst.iValue > kSecond.iValue);
	}

	void buildTables(Tables& kTables)
	{
		int iI, iJ, iK;

		const int iNumBuilds = kTables.aiCounts[COUNT_BUILDS];
		const int iNumImprovements = kTables.aiCounts[COUNT_IMPROVEMENTS];
		const int iNumUnits = kTables.aiCounts[COUNT_UNITS];
		const int iNumUnitClasses = kTables.aiCounts[COUNT_UNIT_CLASSES];
		const int iNumCivilizations = kTables.aiCounts[COUNT_CIVILIZATIONS];
		const int iNumBuildings = kTables.aiCounts[COUNT_BUILDINGS];
		const int iNumBuildingClasses = kTables.aiCounts[COUNT_BUILDING_CLASSES];

		kTables.akRouteBuilds.clear();
		kTables.aaeImprovementBuilds.clear();
		kTables.aaeImprovementBuilds.resize(iNumImprovements);

		for (iI = 0; iI < iNumBuilds; iI++)
		{
			const CvBuildInfo& kBuild = GC.getBuildInfo((BuildTypes)iI);

			if (kBuild.getRoute() != NO_ROUTE)
			{
				CvRouteBuild kRouteBuild;
				kRouteBuild.eBuild = (BuildTypes)iI;
				kRouteBuild.eRoute = (RouteTypes)kBuild.getRoute();
				kRouteBuild.iValue = GC.getRouteInfo(kRouteBuild.eRoute).getValue();

				if (kRouteBuild.iValue > 0)
				{
					kTables.akRouteBuilds.push_back(kRouteBuild);
				}
			}

			if (kBuild.getImprovement() != NO_IMPROVEMENT)
			{
				kTables.aaeImprovementBuilds[kBuild.getImprovement()].push_back((BuildTypes)iI);
			}
		}

		// stable: of two builds whose routes are worth the same, the earlier build stays first
		std::stable_sort(kTables.akRouteBuilds.begin(), kTables.akRouteBuilds.end(), betterRoute);

		kTables.aeFinalImprovement.resize(iNumImprovements);

		for (iI = 0; iI < iNumImprovements; iI++)
		{
			// finalImprovementUpgrade(iI, 0) as a loop: follow the upgrades to the last one,
			// and give up (an upgrade loop in the XML) after more steps than there are improvements
			ImprovementTypes eImprovement = (ImprovementTypes)iI;
			int iCount = 0;

			while ((iCount <= iNumImprovements) && (GC.getImprovementInfo(eImprovement).getImprovementUpgrade() != NO_IMPROVEMENT))
			{
				eImprovement = (ImprovementTypes)GC.getImprovementInfo(eImprovement).getImprovementUpgrade();
				iCount++;
			}

			kTables.aeFinalImprovement[iI] = (iCount > iNumImprovements) ? NO_IMPROVEMENT : eImprovement;
		}

		kTables.aaeUpgradeUnits.clear();
		kTables.aaeUpgradeUnits.resize(iNumCivilizations * iNumUnits);

		for (iI = 0; iI < iNumUnits; iI++)
		{
			const CvUnitInfo& kUnit = GC.getUnitInfo((UnitTypes)iI);

			for (iJ = 0; iJ < iNumUnitClasses; iJ++)
			{
				if (kUnit.getUpgradeUnitClass(iJ))
				{
					for (iK = 0; iK < iNumCivilizations; iK++)
					{
						const UnitTypes eUpgradeUnit = (UnitTypes)GC.getCivilizationInfo((CivilizationTypes)iK).getCivilizationUnits(iJ);

						if (eUpgradeUnit != NO_UNIT)
						{
							kTables.aaeUpgradeUnits[(iK * iNumUnits) + iI].push_back(eUpgradeUnit);
						}
					}
				}
			}
		}

		kTables.aaePrereqBuildingClasses.clear();
		kTables.aaePrereqBuildingClasses.resize(iNumBuildings);

		for (iI = 0; iI < iNumBuildings; iI++)
		{
			const CvBuildingInfo& kBuilding = GC.getBuildingInfo((BuildingTypes)iI);

			for (iJ = 0; iJ < iNumBuildingClasses; iJ++)
			{
				if (kBuilding.getPrereqNumOfBuildingClass(iJ) >= 1)
				{
					kTables.aaePrereqBuildingClasses[iI].push_back((BuildingClassTypes)iJ);
				}
			}
		}
	}

	// The counts the tables were made for against the counts now. tables() used to make
	// this comparison on every call, tens of millions of times a game; the XML loaders
	// now say when the infos change (CvInfoTables::infosChanged()).
	bool countsCurrent()
	{
		int aiCounts[NUM_COUNTS];
		currentCounts(aiCounts);
		return std::equal(aiCounts, aiCounts + NUM_COUNTS, s_kTables.aiCounts);
	}

#ifdef BRIDGED_PROFILE
	void verifyCounts()
	{
		static BridgedProfileSample kChecked = { "info table verify: table counts checked" };
		static BridgedProfileSample kWrong = { "info table verify: TABLES OUT OF DATE" };

		bridgedProfileAdd(&kChecked, 0);
		if (!countsCurrent())
		{
			bridgedProfileAdd(&kWrong, 0);
		}
	}
#endif

	const Tables& tables()
	{
		if (!s_bTablesBuilt)
		{
			// first use, or the XML was loaded again
			currentCounts(s_kTables.aiCounts);
			buildTables(s_kTables);
			s_bTablesBuilt = true;

			for (int iI = 0; iI < MAX_TEAMS; iI++)
			{
				s_akTeamTechs[iI].bValid = false;
			}
		}
#ifdef BRIDGED_PROFILE
		else if (CvInfoTables::isVerify())
		{
			verifyCounts();
		}
#endif

		return s_kTables;
	}

	bool calculateBuildTech(const CvTeam& kTeam, BuildTypes eBuild)
	{
		return kTeam.isHasTech((TechTypes)GC.getBuildInfo(eBuild).getTechPrereq());
	}

	bool calculateUnitTech(const CvTeam& kTeam, UnitTypes eUnit)
	{
		const CvUnitInfo& kUnit = GC.getUnitInfo(eUnit);

		if (!kTeam.isHasTech((TechTypes)kUnit.getPrereqAndTech()))
		{
			return false;
		}

		for (int iI = 0; iI < GC.getNUM_UNIT_AND_TECH_PREREQS(); iI++)
		{
			if (!kTeam.isHasTech((TechTypes)kUnit.getPrereqAndTechs(iI)))
			{
				return false;
			}
		}

		return true;
	}

	bool calculateBuildingTech(const CvTeam& kTeam, BuildingTypes eBuilding)
	{
		const CvBuildingInfo& kBuilding = GC.getBuildingInfo(eBuilding);

		if (!kTeam.isHasTech((TechTypes)kBuilding.getPrereqAndTech()))
		{
			return false;
		}

		for (int iI = 0; iI < GC.getNUM_BUILDING_AND_TECH_PREREQS(); iI++)
		{
			if (!kTeam.isHasTech((TechTypes)kBuilding.getPrereqAndTechs(iI)))
			{
				return false;
			}
		}

		if (kBuilding.getSpecialBuildingType() != NO_SPECIALBUILDING)
		{
			if (!kTeam.isHasTech((TechTypes)GC.getSpecialBuildingInfo((SpecialBuildingTypes)kBuilding.getSpecialBuildingType()).getTechPrereq()))
			{
				return false;
			}
		}

		return true;
	}

	int calculateDiscoverableBonusCount(const CvTeam& kTeam, ImprovementTypes eImprovement)
	{
		int iCount = 0;

		for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
		{
			if (kTeam.isHasTech((TechTypes)GC.getBonusInfo((BonusTypes)iI).getTechReveal()))
			{
				if (GC.getImprovementInfo(eImprovement).getImprovementBonusDiscoverRand(iI) > 0)
				{
					iCount++;
				}
			}
		}

		return iCount;
	}

	const TeamTechs& teamTechs(TeamTypes eTeam)
	{
		const Tables& kTables = tables();	// first: a fresh set of tables makes every team's masks stale
		TeamTechs& kTechs = s_akTeamTechs[eTeam];

		if (!kTechs.bValid)
		{
			const CvTeam& kTeam = GET_TEAM(eTeam);
			int iI;

			kTechs.abBuild.resize(kTables.aiCounts[COUNT_BUILDS]);
			for (iI = 0; iI < kTables.aiCounts[COUNT_BUILDS]; iI++)
			{
				kTechs.abBuild[iI] = calculateBuildTech(kTeam, (BuildTypes)iI) ? 1 : 0;
			}

			kTechs.abUnit.resize(kTables.aiCounts[COUNT_UNITS]);
			for (iI = 0; iI < kTables.aiCounts[COUNT_UNITS]; iI++)
			{
				kTechs.abUnit[iI] = calculateUnitTech(kTeam, (UnitTypes)iI) ? 1 : 0;
			}

			kTechs.abBuilding.resize(kTables.aiCounts[COUNT_BUILDINGS]);
			for (iI = 0; iI < kTables.aiCounts[COUNT_BUILDINGS]; iI++)
			{
				kTechs.abBuilding[iI] = calculateBuildingTech(kTeam, (BuildingTypes)iI) ? 1 : 0;
			}

			kTechs.aiDiscoverableBonuses.resize(kTables.aiCounts[COUNT_IMPROVEMENTS]);
			for (iI = 0; iI < kTables.aiCounts[COUNT_IMPROVEMENTS]; iI++)
			{
				kTechs.aiDiscoverableBonuses[iI] = calculateDiscoverableBonusCount(kTeam, (ImprovementTypes)iI);
			}

			kTechs.bValid = true;
		}

		return kTechs;
	}

#ifdef BRIDGED_PROFILE
	void verifyMask(bool bKept, bool bFresh)
	{
		static BridgedProfileSample kChecked = { "info table verify: mask answers checked" };
		static BridgedProfileSample kWrong = { "info table verify: MASK ANSWER DIFFERS" };

		bridgedProfileAdd(&kChecked, 0);
		if (bKept != bFresh)
		{
			bridgedProfileAdd(&kWrong, 0);
		}
	}
#endif
}


const std::vector<CvRouteBuild>& CvInfoTables::routeBuilds()
{
	return tables().akRouteBuilds;
}


const std::vector<BuildTypes>& CvInfoTables::improvementBuilds(ImprovementTypes eImprovement)
{
	FAssert(eImprovement >= 0 && eImprovement < GC.getNumImprovementInfos());
	return tables().aaeImprovementBuilds[eImprovement];
}


ImprovementTypes CvInfoTables::finalImprovement(ImprovementTypes eImprovement)
{
	FAssert(eImprovement >= 0 && eImprovement < GC.getNumImprovementInfos());
	return tables().aeFinalImprovement[eImprovement];
}


const std::vector<UnitTypes>& CvInfoTables::upgradeUnits(CivilizationTypes eCivilization, UnitTypes eUnit)
{
	const Tables& kTables = tables();
	FAssert(eCivilization >= 0 && eCivilization < kTables.aiCounts[COUNT_CIVILIZATIONS]);
	FAssert(eUnit >= 0 && eUnit < kTables.aiCounts[COUNT_UNITS]);
	return kTables.aaeUpgradeUnits[(eCivilization * kTables.aiCounts[COUNT_UNITS]) + eUnit];
}


const std::vector<BuildingClassTypes>& CvInfoTables::prereqBuildingClasses(BuildingTypes eBuilding)
{
	FAssert(eBuilding >= 0 && eBuilding < GC.getNumBuildingInfos());
	return tables().aaePrereqBuildingClasses[eBuilding];
}


bool CvInfoTables::isBuildTech(TeamTypes eTeam, BuildTypes eBuild)
{
	FAssert(eTeam >= 0 && eTeam < MAX_TEAMS);
	FAssert(eBuild >= 0 && eBuild < GC.getNumBuildInfos());

	const bool bTech = (teamTechs(eTeam).abBuild[eBuild] != 0);

#ifdef BRIDGED_PROFILE
	if (isVerify())
	{
		verifyMask(bTech, calculateBuildTech(GET_TEAM(eTeam), eBuild));
	}
#endif

	return bTech;
}


bool CvInfoTables::isUnitTech(TeamTypes eTeam, UnitTypes eUnit)
{
	FAssert(eTeam >= 0 && eTeam < MAX_TEAMS);
	FAssert(eUnit >= 0 && eUnit < GC.getNumUnitInfos());

	const bool bTech = (teamTechs(eTeam).abUnit[eUnit] != 0);

#ifdef BRIDGED_PROFILE
	if (isVerify())
	{
		verifyMask(bTech, calculateUnitTech(GET_TEAM(eTeam), eUnit));
	}
#endif

	return bTech;
}


bool CvInfoTables::isBuildingTech(TeamTypes eTeam, BuildingTypes eBuilding)
{
	FAssert(eTeam >= 0 && eTeam < MAX_TEAMS);
	FAssert(eBuilding >= 0 && eBuilding < GC.getNumBuildingInfos());

	const bool bTech = (teamTechs(eTeam).abBuilding[eBuilding] != 0);

#ifdef BRIDGED_PROFILE
	if (isVerify())
	{
		verifyMask(bTech, calculateBuildingTech(GET_TEAM(eTeam), eBuilding));
	}
#endif

	return bTech;
}


int CvInfoTables::discoverableBonusCount(TeamTypes eTeam, ImprovementTypes eImprovement)
{
	FAssert(eTeam >= 0 && eTeam < MAX_TEAMS);
	FAssert(eImprovement >= 0 && eImprovement < GC.getNumImprovementInfos());

	const int iCount = teamTechs(eTeam).aiDiscoverableBonuses[eImprovement];

#ifdef BRIDGED_PROFILE
	if (isVerify())
	{
		static BridgedProfileSample kChecked = { "info table verify: discoverable bonus counts checked" };
		static BridgedProfileSample kWrong = { "info table verify: DISCOVERABLE BONUS COUNT DIFFERS" };
		bridgedProfileAdd(&kChecked, 0);
		if (iCount != calculateDiscoverableBonusCount(GET_TEAM(eTeam), eImprovement))
		{
			bridgedProfileAdd(&kWrong, 0);
		}
	}
#endif

	return iCount;
}


void CvInfoTables::infosChanged()
{
	s_bTablesBuilt = false;
}


void CvInfoTables::prepare()
{
	tables();
	for (int iTeam = 0; iTeam < MAX_TEAMS; iTeam++)
	{
		if (GET_TEAM((TeamTypes)iTeam).isAlive())
		{
			teamTechs((TeamTypes)iTeam);
		}
	}
}

void CvInfoTables::teamTechsChanged(TeamTypes eTeam)
{
	// Rare next to the lookups, and every game start and load comes through here: the
	// place to notice info lists that changed without infosChanged() being told.
	if (s_bTablesBuilt && !countsCurrent())
	{
		s_bTablesBuilt = false;
	}

	if (eTeam >= 0 && eTeam < MAX_TEAMS)
	{
		s_akTeamTechs[eTeam].bValid = false;
	}
	else
	{
		for (int iI = 0; iI < MAX_TEAMS; iI++)
		{
			s_akTeamTechs[iI].bValid = false;
		}
	}
}


bool CvInfoTables::isVerify()
{
#ifdef BRIDGED_PROFILE
	static int iVerify = -1;
	if (iVerify == -1)
	{
		iVerify = (GC.getDefineINT("INFO_TABLE_VERIFY") != 0) ? 1 : 0;
	}
	return (iVerify != 0);
#else
	return false;
#endif
}
