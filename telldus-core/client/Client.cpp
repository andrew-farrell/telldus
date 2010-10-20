#include "Client.h"
#include "Socket.h"
#include "Mutex.h"

#include <list>

using namespace TelldusCore;

template <typename T> struct CallbackStruct {
	T event;
	int id;
	void *context;
};
typedef CallbackStruct<TDDeviceEvent> DeviceEvent;
typedef CallbackStruct<TDDeviceChangeEvent> DeviceChangeEvent;
typedef CallbackStruct<TDRawDeviceEvent> RawDeviceEvent;
typedef std::list<DeviceEvent> DeviceEventList;
typedef std::list<DeviceChangeEvent> DeviceChangeList;
typedef std::list<RawDeviceEvent> RawDeviceEventList;

class Client::PrivateData {
public:
	int lastCallbackId;
	DeviceEventList deviceEventList;
	DeviceChangeList deviceChangeEventList;
	Socket eventSocket;
	RawDeviceEventList rawDeviceEventList;
	bool running;
	TelldusCore::Mutex mutex;
};

Client *Client::instance = 0;

Client::Client()
	: Thread()
{
	d = new PrivateData;
	d->lastCallbackId = 0;
	d->running = true;
	start();
}

Client::~Client(void) {
	stopThread();
	wait();
	{
		TelldusCore::MutexLocker locker(&d->mutex);
	}
	delete d;
}

void Client::close() {
	if (Client::instance != 0) {
		delete Client::instance;
	}
}

Client *Client::getInstance() {
	if (Client::instance == 0) {
		Client::instance = new Client();
	}
	return Client::instance;
}

void Client::callbackDeviceEvent(int deviceId, int deviceState, const std::wstring &deviceStateValue){
	TelldusCore::MutexLocker locker(&d->mutex);
	for(DeviceEventList::const_iterator callback_it = d->deviceEventList.begin(); callback_it != d->deviceEventList.end(); ++callback_it) {
		(*callback_it).event(deviceId, deviceState, Message::wideToString(deviceStateValue).c_str(), (*callback_it).id, (*callback_it).context);
	}
}

void Client::callbackDeviceChangeEvent(int deviceId, int eventDeviceChanges, int eventChangeType){
	TelldusCore::MutexLocker locker(&d->mutex);
	for(DeviceChangeList::const_iterator callback_it = d->deviceChangeEventList.begin(); callback_it != d->deviceChangeEventList.end(); ++callback_it) {
		(*callback_it).event(deviceId, eventDeviceChanges, eventChangeType, (*callback_it).id, (*callback_it).context);
	}
}

void Client::callbackRawEvent(std::wstring command, int controllerId){
	TelldusCore::MutexLocker locker(&d->mutex);
	for(RawDeviceEventList::const_iterator callback_it = d->rawDeviceEventList.begin(); callback_it != d->rawDeviceEventList.end(); ++callback_it) {
		(*callback_it).event(Message::wideToString(command).c_str(), controllerId, (*callback_it).id, (*callback_it).context);
	}
}

bool Client::getBoolFromService(const Message &msg) {
	return getIntegerFromService(msg) == 1;
}

int Client::getIntegerFromService(const Message &msg) {
	std::wstring response = sendToService(msg);
	return Message::takeInt(&response);
}

std::wstring Client::getWStringFromService(const Message &msg) {
	std::wstring response = sendToService(msg);
	return Message::takeString(&response);
}

int Client::registerDeviceEvent( TDDeviceEvent eventFunction, void *context ) {
	TelldusCore::MutexLocker locker(&d->mutex);
	int id = ++d->lastCallbackId;
	DeviceEvent callback = {eventFunction, id, context};
	d->deviceEventList.push_back(callback);
	return id;
}

int Client::registerDeviceChangeEvent( TDDeviceChangeEvent eventFunction, void *context ) {
	TelldusCore::MutexLocker locker(&d->mutex);
	int id = ++d->lastCallbackId;
	DeviceChangeEvent callback = {eventFunction, id, context};
	d->deviceChangeEventList.push_back(callback);
	return id;
}

int Client::registerRawDeviceEvent( TDRawDeviceEvent eventFunction, void *context ) {
	TelldusCore::MutexLocker locker(&d->mutex);
	int id = ++d->lastCallbackId;
	RawDeviceEvent callback = {eventFunction, id, context};
	d->rawDeviceEventList.push_back(callback);
	return id;
}

void Client::run(){
	//listen here
	d->eventSocket.connect(L"TelldusEvents");

	while(d->running && d->eventSocket.isConnected()){
	//TODO hm, isConnected, never gets the chance of reconnect here... Should maybe retry every something seconds
		std::wstring clientMessage = d->eventSocket.read();
		if(clientMessage != L""){
			//a message arrived
			std::wstring type = Message::takeString(&clientMessage);
			if(type == L"TDDeviceChangeEvent"){
				int deviceId = Message::takeInt(&clientMessage);
				int eventDeviceChanges = Message::takeInt(&clientMessage);
				int eventChangeType = Message::takeInt(&clientMessage);
				callbackDeviceChangeEvent(deviceId, eventDeviceChanges, eventChangeType);
			}
			else if(type == L"TDDeviceEvent"){
				int deviceId = Message::takeInt(&clientMessage);
				int eventState = Message::takeInt(&clientMessage);
				std::wstring eventValue = Message::takeString(&clientMessage);
				callbackDeviceEvent(deviceId, eventState, eventValue);
			}
			else if(type == L"TDRawDeviceEvent"){
				std::wstring command = Message::takeString(&clientMessage);
				int controllerId = Message::takeInt(&clientMessage);
				callbackRawEvent(command, controllerId);
			}
		}
	}
}

std::wstring Client::sendToService(const Message &msg) {
	Socket s;
	s.connect(L"TelldusClient");
	s.write(msg.data());

	return s.read();
}

void Client::stopThread(){
	d->running = false;
	d->eventSocket.stopReadWait();
}

bool Client::unregisterCallback( int callbackId ) {
	TelldusCore::MutexLocker locker(&d->mutex);
	for(DeviceEventList::iterator callback_it = d->deviceEventList.begin(); callback_it != d->deviceEventList.end(); ++callback_it) {
		if ( (*callback_it).id != callbackId ) {
			continue;
		}
		d->deviceEventList.erase(callback_it);
		return true;
	}
	
	for(DeviceChangeList::iterator callback_it = d->deviceChangeEventList.begin(); callback_it != d->deviceChangeEventList.end(); ++callback_it) {
		if ( (*callback_it).id != callbackId ) {
			continue;
		}
		d->deviceChangeEventList.erase(callback_it);
		return true;
	}
	
	for(RawDeviceEventList::iterator callback_it = d->rawDeviceEventList.begin(); callback_it != d->rawDeviceEventList.end(); ++callback_it) {
		if ( (*callback_it).id != callbackId ) {
			continue;
		}
		d->rawDeviceEventList.erase(callback_it);
		return true;
	}
	
	return false;
}