#include "StateMachine.h"
#include "sifteo.h"

namespace TotalsGame
{
int strcmp(const char *a, const char *b)
{
    int dif = 0;
    while(!dif && *a && *b)
        dif = *a++ - *b++;
    return dif;
}

	StateMachine::StateMachine()
	{
		numStateNodes = 0;
		numStateTransitions = 0;
		pendingTransition = NULL;
		currentState = NULL;
	}

	StateMachine &StateMachine::State(const char *name, IStateController *controller)
	{
        ASSERT(numStateNodes < MAX_STATE_NODES);

		stateNodes[numStateNodes].controller = controller;
		stateNodes[numStateNodes].isController = true;
		stateNodes[numStateNodes].name = name;

		numStateNodes++;

		return *this;
	}

	StateMachine &StateMachine::State(const char *name, StateTransitionCallback transition)
	{
        ASSERT(numStateNodes < MAX_STATE_NODES);

		stateNodes[numStateNodes].callback = transition;
		stateNodes[numStateNodes].isController = false;
		stateNodes[numStateNodes].name = name;

		numStateNodes++;

		return *this;
	}

	StateMachine &StateMachine::Transition(const char *from, const char *name, const char *to)
	{
        ASSERT(numStateNodes < MAX_STATE_TRANSITIONS);

		stateTransitions[numStateTransitions].from = from;
		stateTransitions[numStateTransitions].name = name;
		stateTransitions[numStateTransitions].to = to;

		numStateTransitions++;

		return *this;
	}

	void StateMachine::Tick()
	{
        ASSERT(currentState);

		if(currentState->isController)
		{		
           currentState->controller->OnTick();
		}
		else
		{
			pendingTransition = currentState->callback();
		}

		if(pendingTransition)
		{
			for(int i = 0; i < numStateTransitions; i++)
			{
				if(!strcmp(stateTransitions[i].from, currentState->name)
					&& !strcmp(stateTransitions[i].name, pendingTransition))
				{
					SetState(stateTransitions[i].to);
					break;
				}
			}

			pendingTransition = NULL;
		}
	}

	void StateMachine::Paint(bool dirty)
	{
		if(currentState->isController)
		{
			currentState->controller->OnPaint(dirty);
		}
	}

	void StateMachine::QueueTransition(const char *name)
	{
		pendingTransition = name;
	}

	void StateMachine::SetState(const char *name)
	{
		if(currentState && currentState->isController)
		{
			currentState->controller->OnDispose();
		}

		for(int i = 0; i < numStateNodes; i++)
		{
			if(!strcmp(stateNodes[i].name, name))
			{
				currentState = stateNodes + i;
				if(currentState->isController)
				{
					currentState->controller->OnSetup();
				}
				return;
			}
		}
		currentState = NULL;
	}

}

