import time
from factorytest_common import rangeHelper

################################################################
# NOTE: All functions in this module with 'Test' in the name
#       will be discovered and run by factorytest.py
################################################################

# master command IDs
NrfCommsID                  = 0
ExternalFlashCommsID        = 1
ExternalFlashReadWriteID    = 2
LedID                       = 3
UniqueIdID                  = 4
VolumeCalibrationID         = 5
BatteryCalibrationID        = 6
HomeButtonID                = 7
ShutdownID                  = 8
AudioTestID                 = 9
BootloadRequestID           = 10
RFPacketTestID              = 11
RebootRequestID             = 12

# testjig command IDs
SetUsbEnabledID             = 0
SimulatedBatteryVoltageID   = 1
GetBattSupplyCurrentID      = 2
GetusbCurrentID             = 3
BeginNeighborRxID           = 4
StopNeighborRxID            = 5

# testjig event IDs
NeighborRxID                = 6

def _setUsbEnabled(jig, enabled):
    pkt = [SetUsbEnabledID, enabled]
    jig.txPacket(pkt)
    resp = jig.rxPacket()
    return resp.opcode == SetUsbEnabledID

def NrfCommsTest(devMgr):
    uart = devMgr.masterUART()
    txpower = 0
    uart.writeMsg([NrfCommsID, txpower])
    resp = uart.getResponse()

    if resp.opcode != NrfCommsID:
        return False

    success = (resp.payload[0] != 0)
    return success

def ExternalFlashCommsTest(devMgr):
    uart = devMgr.masterUART()
    uart.writeMsg([ExternalFlashCommsID])
    resp = uart.getResponse()

    success = ((resp.opcode == ExternalFlashCommsID) and (resp.payload[0] != 0))
    return success

def ExternalFlashReadWriteTest(devMgr):
    uart = devMgr.masterUART()
    sectorAddr = 0
    offset = 0
    uart.writeMsg([ExternalFlashReadWriteID, sectorAddr, offset])
    resp = uart.getResponse()

    success = ((resp.opcode == ExternalFlashReadWriteID) and (resp.payload[0] != 0))
    return success

def _ledHelper(uart, color, code):
    uart.writeMsg([LedID, code])
    resp = uart.getResponse()

    s = raw_input("Is LED %s? (y/n) " % color)
    if resp.opcode != LedID or not s.startswith("y"):
        return False
    return True

def LedTest(devMgr):
    uart = devMgr.masterUART()

    combos = { "green" : 1,
                "red" : 2,
                "both on" : 3 }

    for key, val in combos.iteritems():
        if not _ledHelper(uart, key, val):
            return False

    # make sure off is last
    if not _ledHelper(uart, "off", 0):
        return False

    return True

def UniqueIdTest(devMgr):
    uart = devMgr.masterUART()
    uart.writeMsg([UniqueIdID])
    resp = uart.getResponse()

    if resp.opcode == UniqueIdID and len(resp.payload) == 12:
        # TODO: capture this, do something with it?
        return True
    else:
        return False

def VBusCurrentDrawTest(devMgr):
    jig = devMgr.testjig();

    if _setUsbEnabled(jig, True):
        # read current & verify against window
        jig.txPacket([GetusbCurrentID])
        resp = jig.rxPacket()
        if resp.opcode != GetusbCurrentID or len(resp.payload) < 2:
            print "Opcode incorrect or payload is borked!"
            return False
        
        current = resp.payload[0] | resp.payload[1] << 8
        
        calc_current = (3.3/(2**12)) * 200 * current
        
        print "current draw at VBUS: %fmA" % (calc_current)
        
        _setUsbEnabled(jig, False)   
    else:
        print "Invalid jig response!"
        
    return True

    # # Master Power Iteration - cycles from 3.2V down to 2.0V and then back up
    # # TODO: tune windows for success criteria
    # measurements = ( (3.2, 0, 0xfff),
    #                  (2.9, 0, 0xfff),
    #                  (2.6, 0, 0xfff),
    #                  (2.3, 0, 0xfff),
    #                  (2.0, 0, 0xfff) )
    # for m in measurements:
    #     voltage = m[0]
    #     minCurrent = m[1]
    #     maxCurrent = m[2]
    # 
    #     # set voltage
    #     # scale voltages to the 12-bit DAC output, and send them LSB first
    #     bit12 = int(rangeHelper(voltage, 0.0, 3.3, 0, 0xfff))
    #     pkt = [SimulatedBatteryVoltageID, bit12 & 0xff, (bit12 >> 8) & 0xff]
    #     jig.txPacket(pkt)
    #     resp = jig.rxPacket()
    #     if resp.opcode != SimulatedBatteryVoltageID:
    #         return False
    # 
    #     time.sleep(1)
    # 
    #     # read current & verify against window
    #     jig.txPacket([GetBattSupplyCurrentID])
    #     resp = jig.rxPacket()
    #     if resp.opcode != GetBattSupplyCurrentID or len(resp.payload) < 2:
    #         return False
    #     current = resp.payload[0] | resp.payload[1] << 8
    #     print "current draw @ %.2fV: 0x%x" % (voltage, current)
    #     if current < minCurrent or current > maxCurrent:
    #         return False
    # 
    # return True

def ListenForNeighborsTest(devMgr):
    jig = devMgr.testjig()

    jig.txPacket([BeginNeighborRxID])
    resp = jig.rxPacket()
    if resp.opcode != BeginNeighborRxID:
        print "error while starting to listen for neighbors"
        return False

    # TODO: determine how we actually want to test this
    count = 0
    while count < 1000:
        resp = jig.rxPacket(timeout = 5000)
        if resp.opcode == NeighborRxID:
            # NB! neighbor data is big endian
            neighbordata = resp.payload[1] << 8 | resp.payload[2]
            nbrId = resp.payload[1] & 0xf
            print "neighbor rx, side %d  id %d (data 0x%x)" % (resp.payload[0], nbrId, neighbordata)
            count = count + 1

    jig.txPacket([StopNeighborRxID])
    resp = jig.rxPacket()
    if resp.opcode != StopNeighborRxID:
        print "error while stopping neighbor rx"
        return False

    return True
