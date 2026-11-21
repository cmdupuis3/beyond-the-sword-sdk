# Benchmark harness for Civ4 BtS game-core DLL comparisons.
#
# Logs, at every game turn, the wall-clock time since the previous turn and a hash of the
# game state. Two DLLs that play the same game from the same seed must produce the same
# hash on every turn; the times are what is being compared.
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


def _onBeginGameTurn(iGameTurn):
	global _fLastTurn, _fStart, _iTurnsLogged, _bDone
	fNow = _now()
	if _fStart is None:
		_fStart = fNow
	if _fLastTurn is None:
		fTurnMs = -1.0	# first turn seen: nothing to measure against
	else:
		fTurnMs = (fNow - _fLastTurn) * 1000.0
	(h, iUnits, iCities) = _stateHash()
	fHashMs = (_now() - fNow) * 1000.0
	_log("TURN %d ms=%.1f hash=%08x units=%d cities=%d hash_ms=%.1f" % (iGameTurn, fTurnMs, h, iUnits, iCities, fHashMs))
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
