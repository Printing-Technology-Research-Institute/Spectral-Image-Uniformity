from __future__ import print_function
import sys
from mvIMPACT import acquire

def supportsValue(prop, value):
    if prop.hasDict:
        validValues = []
        prop.getTranslationDictValues(validValues)
        return value in validValues

    if prop.hasMinValue and prop.getMinValue() > value:
        return False

    if prop.hasMaxValue and prop.getMaxValue() < value:
        return False

    return true

def conditionalSetProperty(prop, value, boSilent=False):
    if prop.isValid and prop.isWriteable and supportsValue(prop, value):
        prop.write(value)
        if boSilent == False:
            print("Property '" + prop.name() + "' set to '" + prop.readS() + "'.")

def getNumberFromUser():
    # Since Python 3 'raw_input' became 'input'...
    if sys.version_info[0] < 3:
        return int(raw_input())
    return int(input())

def getDeviceFromUserInput(devMgr, boSilent=False, boAutomaticallyUseGenICamInterface=True):
    for i in range(devMgr.deviceCount()):
        pDev = devMgr.getDevice(i)
        msg = "[" + str(i) + "]: " + pDev.serial.read() + "(" + pDev.product.read() + ", " + pDev.family.read()
        if pDev.interfaceLayout.isValid:
            if boAutomaticallyUseGenICamInterface == True:
                conditionalSetProperty(pDev.interfaceLayout, acquire.dilGenICam, boSilent)
            msg += ", interface layout: " + pDev.interfaceLayout.readS()
        if pDev.acquisitionStartStopBehaviour.isValid:
            conditionalSetProperty(pDev.acquisitionStartStopBehaviour, acquire.assbUser, boSilent)
            msg += ", acquisition start/stop behaviour: " + pDev.acquisitionStartStopBehaviour.readS()
        if pDev.isInUse:
            msg += ", !!!ALREADY IN USE!!!"
        print(msg + ")")

    print("Please enter the number in front of the listed device followed by [ENTER] to open it: ", end='')
    devNr = getNumberFromUser()
    if (devNr < 0) or (devNr >= devMgr.deviceCount()):
        print("Invalid selection!")
        return None
    return devMgr.getDevice(devNr)

def requestENTERFromUser():
    msg = "Press Enter to continue..."
    # Since Python 3 'raw_input' became 'input'...
    if sys.version_info[0] < 3:
        raw_input(msg)
    else:
        input(msg)

# Start the acquisition manually if this was requested(this is to prepare the driver for data capture and tell the device to start streaming data)
def manuallyStartAcquisitionIfNeeded(pDev, fi):
    if pDev.acquisitionStartStopBehaviour.read() == acquire.assbUser:
        result = fi.acquisitionStart()
        if result != acquire.DMR_NO_ERROR:
            print("'FunctionInterface.acquisitionStart' returned with an unexpected result: " + str(result) + "(" + ImpactAcquireException.getErrorCodeAsString(result) + ")")

# Stop the acquisition manually if this was requested
def manuallyStopAcquisitionIfNeeded(pDev, fi):
    if pDev.acquisitionStartStopBehaviour.read() == acquire.assbUser:
        result = fi.acquisitionStop()
        if result != acquire.DMR_NO_ERROR:
            print("'FunctionInterface.acquisitionStop' returned with an unexpected result: " + str(result) + "(" + ImpactAcquireException.getErrorCodeAsString(result) + ")")
