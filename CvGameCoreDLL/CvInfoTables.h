#pragma once

#ifndef CV_INFO_TABLES_H
#define CV_INFO_TABLES_H

// Lists worked out once from the XML, and each team's answer to "do we have the
// technologies for this build, unit or building".
//
// The game asks these questions by looping over every build, unit class or building
// class and testing each one. The answers depend on the XML alone (the lists) or on the
// XML and the team's technologies (the masks), so they are worked out once and kept:
// the lists until the XML is loaded again, a team's masks until its technologies change.
//
// Every list keeps the order the loop it replaces would have met its members in, so code
// that takes "the first" or "the last" match gets the same one.
//
// INFO_TABLE_VERIFY: a global define for profile builds. Set, each mask answer is checked
// against the team's technologies, and CvPlayer::getBestRoute() against the stock loop.

struct CvRouteBuild
{
	BuildTypes eBuild;
	RouteTypes eRoute;
	int iValue;		// CvRouteInfo::getValue()
};

namespace CvInfoTables
{
	// the builds that lay a route of value above 0: best route first, equal values in build order
	const std::vector<CvRouteBuild>& routeBuilds();

	// the builds that make this improvement, in build order
	const std::vector<BuildTypes>& improvementBuilds(ImprovementTypes eImprovement);

	// finalImprovementUpgrade(eImprovement, 0)
	ImprovementTypes finalImprovement(ImprovementTypes eImprovement);

	// the civilization's units that this unit upgrades to directly, in unit class order
	const std::vector<UnitTypes>& upgradeUnits(CivilizationTypes eCivilization, UnitTypes eUnit);

	// the building classes this building needs some number of, in building class order
	const std::vector<BuildingClassTypes>& prereqBuildingClasses(BuildingTypes eBuilding);

	// the team has the build's technology (CvBuildInfo::getTechPrereq())
	bool isBuildTech(TeamTypes eTeam, BuildTypes eBuild);

	// the team has every technology the unit needs
	bool isUnitTech(TeamTypes eTeam, UnitTypes eUnit);

	// the team has every technology the building needs, its special building type's included
	bool isBuildingTech(TeamTypes eTeam, BuildingTypes eBuilding);

	// a team gained or lost a technology, or was reset or loaded
	void teamTechsChanged(TeamTypes eTeam);

	// true in a profile build with INFO_TABLE_VERIFY set
	bool isVerify();
}

#endif
