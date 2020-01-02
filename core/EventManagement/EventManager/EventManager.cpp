#include "./EventManager.h"

int EventManager::currentTick = 0;

bool EventManager::addEvent(Event _event) {
	eventQueue.addEvent(AsyncEvent(_event, currentTick + _event.getDurationInTicks()));
	return true;
}

bool EventManager::executeNextEvent() {
	if(eventQueue.isEmpty()) return false;
	
	AsyncEvent asyncEvent = eventQueue.getNextEvent();
	eventQueue.removeNextEvent();

	currentTick = asyncEvent.getTickToExecOn();

	return true;
}