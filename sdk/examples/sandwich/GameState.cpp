#include "GameState.h"

void GameState::Init() {
	mQuest = 0;
	mQuestMask = 0;
	mUnlockMask = 0;
	mKeyCount = 1;//0;
	mItemSet = 0xff;
}

bool GameState::AdvanceQuest() {
	if (mQuest < gQuestCount) {
		mQuest++;
		Save();
		return true;
	}
	return false;
}

bool GameState::IsActive(const TriggerData& trigger) const {
	return (
		(trigger.questBegin == 0xff || trigger.questBegin <= mQuest) &&
		(trigger.questEnd == 0xff || trigger.questEnd >= mQuest) &&
		(
			trigger.flagId == 0 || 
			(trigger.flagId <= 32 && (mQuestMask & (1<<(trigger.flagId-1))) == 0) || 
			(trigger.flagId > 32 && (mUnlockMask & (1<<(trigger.flagId-33))) == 0)
		)
	);
}

bool GameState::IsActive(uint8_t questId, uint8_t flagId) const {
	return (
		(questId == 0xff || questId == mQuest) &&
		(
			flagId == 0 || 
			(flagId <= 32 && (mQuestMask & (1<<(flagId-1))) == 0) || 
			(flagId > 32 && (mUnlockMask & (1<<(flagId-33))) == 0)
		)
	);
}

bool GameState::FlagTrigger(const TriggerData& trigger) {
	if (IsActive(trigger) && trigger.flagId) {
		if (trigger.flagId <= 32) {
			mQuestMask |= (1 << (trigger.flagId-1));
		} else {
			mUnlockMask |= (1 << (trigger.flagId-33));
		}
		ASSERT(!IsActive(trigger));
		Save();
		return true;
	}
	return false;
}

bool GameState::Flag(uint8_t questId, uint8_t flagId) {
	if (IsActive(questId, flagId) && flagId) {
		if (flagId <= 32) {
			mQuestMask |= (1 << (flagId-1));
		} else {
			mUnlockMask |= (1 << (flagId-33));
		}
		ASSERT(!IsActive(questId, flagId));
		Save();
		return true;
	}
	return false;
}

bool GameState::PickupItem(int itemId) {
  if (itemId == 0) { return false; }
  if (itemId == ITEM_BASIC_KEY || itemId == ITEM_SKELETON_KEY) {
    mKeyCount++;
    //if (mKeyCount == 1) {
    //}
  } else if (!HasItem(itemId)) {
    mItemSet |= (1<<itemId);
    ASSERT(HasItem(itemId));
  } else {
  	return false;
  }
  return true;
}

bool GameState::DecrementBasicKeyCount() { 
  ASSERT(mKeyCount>0); 
  mKeyCount--; 
  return true;
}

unsigned GameState::GetItems(uint8_t* buf) {
	unsigned result = 0;
	const int firstSandwichId = 2;
	const int sandwichTypeCount = 4;
	for(int8_t itemId=firstSandwichId; itemId<firstSandwichId+sandwichTypeCount; ++itemId) {
		if (HasItem(itemId)) { buf[result++] = itemId; }
	}
	if (HasBasicKey()) {
		buf[result++] = ITEM_BASIC_KEY;
	}
	return result;
}

void GameState::Save() {
	// TODO
}

void GameState::Load() {
	// TODO
}

