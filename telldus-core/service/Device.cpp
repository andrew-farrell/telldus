#include "Device.h"
#include "Settings.h"

class Device::PrivateData {
public:
	std::wstring model;
	std::wstring name;
	ParameterMap parameterList;
	Protocol *protocol;
	std::wstring protocolName;
	int preferredControllerId;
	int state;
	std::wstring stateValue;
};

Device::Device(int id)
	:Mutex()
{
	d = new PrivateData;
	d->protocol = 0;
	d->preferredControllerId = 0;
	d->state = 0;
	//n�r n�got uppdateras, spara ocks� till registret
	//obs, kunna hantera om alla v�rden inte �r satta
}

Device::~Device(void) {
	delete d->protocol;
	delete d;
}

/**
* Get-/Set-methods
*/

int Device::getLastSentCommand(int methodsSupported){
	
	int lastSentCommand = Device::maskUnsupportedMethods(d->state, methodsSupported);
	
	if (lastSentCommand == TELLSTICK_BELL) {
		//Bell is not a state
		lastSentCommand = TELLSTICK_TURNOFF;
	}
	if (lastSentCommand == 0) {
		lastSentCommand = TELLSTICK_TURNOFF;
	}
	return lastSentCommand;

}

void Device::setLastSentCommand(int command, std::wstring value){
	d->state = command;
	d->stateValue = value;
}

std::wstring Device::getModel(){
	return d->model;
}

void Device::setModel(const std::wstring &model){
	d->model = model;
}

std::wstring Device::getName(){
	return d->name;
}

void Device::setName(const std::wstring &name){
	d->name = name;
}

std::wstring Device::getParameter(const std::wstring &key){
	return d->parameterList[key];
}

void Device::setParameter(const std::wstring &key, const std::wstring &value){
	d->parameterList[key] = value;
}

int Device::getPreferredControllerId(){
	return d->preferredControllerId;
}

void Device::setPreferredControllerId(int controllerId){
	d->preferredControllerId = controllerId;
}

std::wstring Device::getProtocolName(){
	return d->protocolName;
}

void Device::setProtocolName(const std::wstring &protocolName){
	d->protocolName = protocolName;
}

std::wstring Device::getStateValue(){
	return d->stateValue;
}

/**
* End Get-/Set
*/

int Device::doAction(int action, unsigned char data, Controller *controller) {
	//TODO, where to check for supported methods?
	Protocol *p = this->retrieveProtocol();
	if(p){
		std::string code = p->getStringForMethod(action, data, controller);
		return controller->send(code);
	}
	return TELLSTICK_ERROR_UNKNOWN;
}

Protocol* Device::retrieveProtocol() {
	if (d->protocol) {
		return d->protocol;
	}
	
	d->protocol = Protocol::getProtocolInstance(d->protocolName);
	if(d->protocol){
		d->protocol->setModel(d->model);
		d->protocol->setParameters(d->parameterList);
		return d->protocol;
	}

	return 0;
}

int Device::maskUnsupportedMethods(int methods, int supportedMethods) {
	// Bell -> On
	if ((methods & TELLSTICK_BELL) && !(supportedMethods & TELLSTICK_BELL)) {
		methods |= TELLSTICK_TURNON;
	}
	//Cut of the rest of the unsupported methods we don't have a fallback for
	return methods & supportedMethods;
}