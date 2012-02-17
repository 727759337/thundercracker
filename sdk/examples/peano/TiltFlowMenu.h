#pragma once

#include "sifteo.h"

namespace TotalsGame
{
class TiltFlowView;
class TiltFlowItem;
class TiltFlowDetailView;

  class TiltFlowMenu /* TODO : IDisposable*/ {
public:
    static const float kPickDelay = 0.25f;
    static const float kRestDelay = 0.1f;
private:
    TiltFlowItem *items;
    int numItems;
  public:
    TiltFlowItem *GetItem(int i);
    int GetNumItems();
  private:
    float mSimTime;
    float mPickTime;
    TiltFlowView *view;
  public:
    TiltFlowView *GetView();
  private:
    TiltFlowDetailView *details;
  public:
    TiltFlowDetailView *GetDetails();

    TiltFlowMenu(TiltFlowItem *_items, int _numItems, TiltFlowDetailView *_details);
/* TODO
    public void Dispose() {
      view.Cube = null;
    }
*/
    float GetSimTime();
    TiltFlowItem *GetResultItem();
    TiltFlowItem *GetToggledItem();
    void SetToggledItem(TiltFlowItem *item);
  private:
    TiltFlowItem *toggledItem;
  public:
    void ClearToggledItem();
    bool IsPicked();
    bool IsDone();

    void Tick(float dt);

    void Pick();

  public:
    //for placement new
    void* operator new (size_t size, void* ptr) throw() {return ptr;}

  };
}

