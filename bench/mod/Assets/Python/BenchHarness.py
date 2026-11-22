# Benchmark harness for Civ4 BtS game-core DLL comparisons.
#
# Logs, at every game turn, the wall-clock time since the previous turn and a hash of the
# game state. Two DLLs that play the same game from the same seed must produce the same
# hash on every turn; the times are what is being compared.
#
# There are two hashes. "hash" is what the players own and where; it has been the same since
# the first benchmark, so every log can be compared with every other. "wide" takes in much
# more of the game (see _wideHash) and is the stricter test; older logs do not have it.
#
# Runs inside the game's Python 2.4. BenchConfig.py is written by bench/run_bench.py.
# Nothing here may raise into the game: a broken harness must not change the run.

from CvPythonExtensions import *
import time
import BenchConfig

gc = CyGlobalContext()

_now = time.clock	# high-resolution wall clock on Windows in Python 2.4
_fLastTurn = None
_fStart = None
_iTurnsLogged = 0
_bDone = False
_bFailed = False
_bWideFailed = False


def _log(szLine):
	f = open(BenchConfig.LOG_PATH, "a")
	f.write(szLine + "\n")
	f.close()


def _mix(h, v):
	return ((h * 1000003) ^ (v & 0xFFFFFFFF)) & 0xFFFFFFFF


def _stateHash():
	"Hash of what the players own and where. Returns (hash, units, cities)."
	h = 17
	iUnits = 0
	iCities = 0
	for iPlayer in range(gc.getMAX_PLAYERS()):
		pPlayer = gc.getPlayer(iPlayer)
		if not pPlayer.isAlive():
			continue
		h = _mix(h, iPlayer)
		h = _mix(h, pPlayer.getGold())
		h = _mix(h, pPlayer.getTotalPopulation())
		h = _mix(h, pPlayer.getTotalLand())
		(pCity, it) = pPlayer.firstCity(False)
		while pCity:
			if not pCity.isNone():
				iCities += 1
				h = _mix(h, pCity.getX())
				h = _mix(h, pCity.getY())
				h = _mix(h, pCity.getPopulation())
				h = _mix(h, pCity.getFood())
				h = _mix(h, pCity.getProduction())
				h = _mix(h, pCity.getCulture(iPlayer))
			(pCity, it) = pPlayer.nextCity(it, False)
		(pUnit, it) = pPlayer.firstUnit(False)
		while pUnit:
			if not pUnit.isNone():
				iUnits += 1
				h = _mix(h, pUnit.getX())
				h = _mix(h, pUnit.getY())
				h = _mix(h, pUnit.getUnitType())
				h = _mix(h, pUnit.getDamage())
				h = _mix(h, pUnit.getExperience())
			(pUnit, it) = pPlayer.nextUnit(it, False)
	return (h, iUnits, iCities)


def _wideHash():
	"""Hash of the rest of the game state that the stock Python interface can read.

	The random number generators cannot be read directly. The game's own sync checksum (what
	multiplayer uses to notice two computers drifting apart) adds in both of their seeds, so
	it stands in for them; which other things it covers depends on the turn slice.
	Only plain getters are called here: nothing that works a value out and keeps it.
	"""
	pGame = gc.getGame()
	h = 23
	h = _mix(h, pGame.calculateSyncChecksum())
	h = _mix(h, pGame.getTurnSlice())
	iNumTechs = gc.getNumTechInfos()
	iNumTeams = gc.getMAX_TEAMS()
	for iTeam in range(iNumTeams):
		pTeam = gc.getTeam(iTeam)
		if not pTeam.isAlive():
			continue
		h = _mix(h, iTeam)
		for iTech in range(iNumTechs):
			if pTeam.isHasTech(iTech):
				h = _mix(h, iTech)
			else:
				iProgress = pTeam.getResearchProgress(iTech)
				if iProgress != 0:
					h = _mix(h, (iTech << 20) ^ iProgress)
		for iOther in range(iNumTeams):
			if iOther != iTeam and gc.getTeam(iOther).isAlive():
				iBits = 0
				if pTeam.isAtWar(iOther):
					iBits |= 1
				if pTeam.isOpenBorders(iOther):
					iBits |= 2
				if pTeam.isDefensivePact(iOther):
					iBits |= 4
				if pTeam.isVassal(iOther):
					iBits |= 8
				if iBits:
					h = _mix(h, (iOther << 4) | iBits)
	iNumClasses = gc.getNumBuildingClassInfos()
	iNumReligions = gc.getNumReligionInfos()
	iNumCivicOptions = gc.getNumCivicOptionInfos()
	for iPlayer in range(gc.getMAX_PLAYERS()):
		pPlayer = gc.getPlayer(iPlayer)
		if not pPlayer.isAlive():
			continue
		h = _mix(h, iPlayer)
		h = _mix(h, pPlayer.getCurrentResearch())
		h = _mix(h, pPlayer.getStateReligion())
		h = _mix(h, pPlayer.getGoldenAgeTurns())
		h = _mix(h, pPlayer.getAnarchyTurns())
		h = _mix(h, pPlayer.getAssets())
		h = _mix(h, pPlayer.getPower())
		for iI in range(CommerceTypes.NUM_COMMERCE_TYPES):
			h = _mix(h, pPlayer.getCommercePercent(iI))
		for iI in range(iNumCivicOptions):
			h = _mix(h, pPlayer.getCivics(iI))
		for iI in range(iNumClasses):
			iCount = pPlayer.getBuildingClassCount(iI)
			if iCount != 0:
				h = _mix(h, (iI << 12) ^ iCount)
		for iI in range(iNumReligions):
			h = _mix(h, pPlayer.getHasReligionCount(iI))
		(pCity, it) = pPlayer.firstCity(False)
		while pCity:
			if not pCity.isNone():
				h = _mix(h, pCity.getID())
				h = _mix(h, pCity.getNumBuildings())
				h = _mix(h, pCity.getProductionUnit())
				h = _mix(h, pCity.getProductionBuilding())
				h = _mix(h, pCity.getProductionProject())
				h = _mix(h, pCity.getGreatPeopleProgress())
				h = _mix(h, pCity.getCultureLevel())
			(pCity, it) = pPlayer.nextCity(it, False)
		(pUnit, it) = pPlayer.firstUnit(False)
		while pUnit:
			if not pUnit.isNone():
				h = _mix(h, pUnit.getID())
				h = _mix(h, pUnit.getGroupID())
				h = _mix(h, pUnit.getLevel())
				h = _mix(h, pUnit.getMoves())
			(pUnit, it) = pPlayer.nextUnit(it, False)
	pMap = gc.getMap()
	for iPlot in range(pMap.numPlots()):
		pPlot = pMap.plotByIndex(iPlot)
		iOwner = pPlot.getOwner()
		iImprovement = pPlot.getImprovementType()
		iRoute = pPlot.getRouteType()
		# most plots are unowned with nothing built on them: only what is there goes in
		if iOwner != -1 or iImprovement != -1 or iRoute != -1:
			h = _mix(h, iPlot)
			h = _mix(h, ((iOwner & 0xFF) << 16) | ((iImprovement & 0xFF) << 8) | (iRoute & 0xFF))
		h = _mix(h, ((pPlot.getFeatureType() & 0xFF) << 8) | (pPlot.getBonusType(-1) & 0xFF))
	return h


def _onBeginGameTurn(iGameTurn):
	global _fLastTurn, _fStart, _iTurnsLogged, _bDone, _bWideFailed
	fNow = _now()
	if _fStart is None:
		_fStart = fNow
	if _fLastTurn is None:
		fTurnMs = -1.0	# first turn seen: nothing to measure against
	else:
		fTurnMs = (fNow - _fLastTurn) * 1000.0
	(h, iUnits, iCities) = _stateHash()
	fHashMs = (_now() - fNow) * 1000.0
	# The wide hash is newer and asks for much more. If it fails, the run and the first hash
	# go on without it.
	fWide = _now()
	try:
		szWide = "%08x" % (_wideHash(),)
	except Exception, e:
		szWide = "ERR"
		if not _bWideFailed:
			_bWideFailed = True
			_log("WIDE_HASH_ERROR %s" % (str(e),))
	fWideMs = (_now() - fWide) * 1000.0
	_log("TURN %d ms=%.1f hash=%08x units=%d cities=%d hash_ms=%.1f wide=%s wide_ms=%.1f" % (iGameTurn, fTurnMs, h, iUnits, iCities, fHashMs, szWide, fWideMs))
	_iTurnsLogged += 1
	# restart the clock after hashing so the hash cost is not charged to the next turn
	_fLastTurn = _now()
	if (not _bDone) and BenchConfig.MAX_TURNS > 0 and _iTurnsLogged > BenchConfig.MAX_TURNS:
		_bDone = True
		_log("DONE turns=%d total_s=%.1f" % (_iTurnsLogged - 1, _now() - _fStart))


def onEvent(argsList):
	"Called for every game event before the normal event manager sees it."
	global _bFailed
	if _bFailed:
		return
	try:
		szTag = argsList[0]
		if szTag == 'BeginGameTurn':
			_onBeginGameTurn(argsList[1])
		elif szTag in ('GameStart', 'OnLoad', 'OnPreSave', 'victory'):
			_log("EVENT %s turn=%d t=%.1f" % (szTag, gc.getGame().getGameTurn(), _now()))
	except Exception, e:
		_bFailed = True
		try:
			_log("HARNESS_ERROR %s" % (str(e),))
		except:
			pass


try:
	_log("INIT label=%s max_turns=%d" % (BenchConfig.LABEL, BenchConfig.MAX_TURNS))
except:
	_bFailed = True
