#include <gtest/gtest.h>
#include "radio.h"

#ifndef DISABLE_RADIO_TEST

#ifdef UNIT_TEST_RADIO
  #include "mockcube.h"
  
  #define CubeSlot MockCubeSlot
#endif


class RadioTest : public ::testing::Test {
  protected:
    virtual void SetUp() {
        CubeSlots::disableCubes(0xFFFFFFFF);
    }

    virtual void TearDown() {
        for (int i = 0; i < _SYS_NUM_CUBE_SLOTS; i++) {
            CubeSlot &cube = CubeSlot::getInstance(i);
            cube.radioAcknowledgeCallCount = 0;
        }
    }
};


TEST_F(RadioTest, ackWithPacketShouldCallRadioAcknowledgeOnEnabledCube) {
    CubeSlot &cube = CubeSlot::getInstance(1);
    CubeSlots::enableCubes(cube.bit());
    
    RadioManager::fifoPush(1);
    
    PacketBuffer buf;
    RadioManager::ackWithPacket(buf);
    
    EXPECT_EQ(1, cube.radioAcknowledgeCallCount);
}

TEST_F(RadioTest, ackWithPacketShouldNotCallRadioAcknowledgeOnDisabledCube) {
    CubeSlot &cube = CubeSlot::getInstance(1);
    
    RadioManager::fifoPush(1);
    
    PacketBuffer buf;
    RadioManager::ackWithPacket(buf);
    
    EXPECT_EQ(0, cube.radioAcknowledgeCallCount);
}

#endif // DISABLE_RADIO_TEST
