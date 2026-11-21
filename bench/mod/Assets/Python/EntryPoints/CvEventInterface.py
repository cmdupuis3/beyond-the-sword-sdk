# Benchmark mod override of the stock CvEventInterface.py.
#
# Identical to stock except that every event is first shown to BenchHarness, which only
# reads game state and writes a log.
#
# These functions are App Entry Points from C++
# WARNING: These function names should not be changed
# WARNING: These functions can not be placed into a class
#
import CvUtil
import CvEventManager
import BenchHarness
from CvPythonExtensions import *


class BenchEventManager(CvEventManager.CvEventManager):
	def handleEvent(self, argsList):
		BenchHarness.onEvent(argsList)
		return CvEventManager.CvEventManager.handleEvent(self, argsList)


normalEventManager = BenchEventManager()

def getEventManager():
	return normalEventManager

def onEvent(argsList):
	'Called when a game event happens - return 1 if the event was consumed'
	return getEventManager().handleEvent(argsList)

def applyEvent(argsList):
	context, playerID, netUserData, popupReturn = argsList
	return getEventManager().applyEvent(argsList)

def beginEvent(context, argsList=-1):
	return getEventManager().beginEvent(context, argsList)
