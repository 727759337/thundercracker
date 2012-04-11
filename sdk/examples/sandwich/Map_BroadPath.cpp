#include "Map.h"
#include "Game.h"

bool Map::CanTraverse(BroadLocation bloc, Side side) {
  // validate that the exit portal is reachable given the subdivision type, or else
  // check the portal buffer for the general case
  switch(bloc.view->GetRoom()->SubdivType()) {
    case SUBDIV_DIAG_POS:
      if (side == TOP || side == LEFT) { return bloc.subdivision == 0; }
      return bloc.subdivision == 1;
    case SUBDIV_DIAG_NEG:
      if (side == TOP || side == RIGHT) { return bloc.subdivision == 0; }
      return bloc.subdivision == 1;
    case SUBDIV_BRDG_HOR:
    case SUBDIV_BRDG_VER:
      if (side == TOP || side == BOTTOM) { return bloc.subdivision == 0; }
      return bloc.subdivision == 1;
    default:
      Int2 loc = bloc.view->Location();
      switch(side) {
        default: ASSERT(false);
        case TOP: return loc.y > 0 && GetPortalY(loc.x, loc.y-1);
        case LEFT: return loc.x > 0 && GetPortalX(loc.x-1, loc.y);
        case BOTTOM: return loc.y < mData->height-1 && GetPortalY(loc.x, loc.y);
        case RIGHT: return loc.x < mData->width-1 && GetPortalX(loc.x, loc.y);
      }
  }
  return false;
}

bool Map::GetBroadLocationNeighbor(BroadLocation loc, Side side, BroadLocation* outNeighbor) {
  if (!CanTraverse(loc, side)) { return false; }
  Viewport* gv = loc.view->Parent()->VirtualNeighborAt(side);
  if (!gv || !gv->ShowingRoom()) { return false; }
  outNeighbor->view = gv->GetRoomView();
  const Room* room = outNeighbor->view->GetRoom();
  switch(room->SubdivType()) {
    case SUBDIV_DIAG_POS:
      outNeighbor->subdivision = (side == BOTTOM || side == RIGHT) ? 0 : 1;
      break;
    case SUBDIV_DIAG_NEG:
      outNeighbor->subdivision = (side == BOTTOM || side == LEFT) ? 0 : 1;
      break;
    case SUBDIV_BRDG_HOR:
    case SUBDIV_BRDG_VER:
      outNeighbor->subdivision = (side == TOP || side == BOTTOM) ? 0 : 1;
      break;
    default:
      outNeighbor->subdivision = 0;
      break;
  }
  return true;
}

// TODO: stack alloc?
static uint8_t sVisitMask[NUM_CUBES];

BroadPath::BroadPath() {
  for(int i=0; i<2*NUM_CUBES; ++i) {
    steps[i] = -1;
  }
}

bool BroadPath::DequeueStep(BroadLocation newRoot, BroadLocation* outNext) {
  if (steps[0] == -1 || steps[1] == -1) {
    steps[0] = -1;
    return false;
  }
  for(int i=0; i<2*NUM_CUBES; ++i) { steps[i] = steps[i+1]; }
  steps[NUM_CUBES-2] = -1;
  if (*steps >= 0 && gGame.GetMap()->GetBroadLocationNeighbor(newRoot, (Side)*steps, outNext)) {
    return true;
  }
  steps[0] = -1;
  return false;
}

void BroadPath::Cancel() {
  steps[0] = -1;
}


static bool Visit(BroadPath* outPath, BroadLocation loc, Side side, int depth, unsigned* outViewId) {
  BroadLocation next;
  if (!gGame.GetMap()->GetBroadLocationNeighbor(loc, side, &next) || sVisitMask[next.view->Parent()->GetCube()] & (1<<next.subdivision)) {
    if (depth > 1) {
      Viewport *nextView = loc.view->Parent()->VirtualNeighborAt(side);
      if (nextView && nextView->ShowingGatewayEdge() && nextView->Touched()) {
        outPath->steps[depth-1] = -1;
        *outViewId = nextView->GetCube();
        return true;
      }
    }
    return false;
  }
  sVisitMask[next.view->Parent()->GetCube()] |= (1<<next.subdivision);
  if (next.view->Parent()->Touched()) {
    outPath->steps[depth] = -1;
    *outViewId = next.view->Parent()->GetCube();
    return true;
  } else {
    for(int side=0; side<NUM_SIDES; ++side) {
      outPath->steps[depth] = side;
      if (Visit(outPath, next, (Side)side, depth+1, outViewId)) {
        return true;
      } else {
        outPath->steps[depth] = -1;
      }
    }
  }
  outPath->steps[depth] = -1;
  return false;
}

bool Map::FindBroadPath(BroadPath* outPath, unsigned* outViewId) {
  bool anyTouches = false;
  for(unsigned i=0; i<NUM_CUBES; ++i) { 
    sVisitMask[i] = 0; 
    anyTouches |= gGame.ViewAt(i)->Touched();
  }
  if (!anyTouches) { return false; }
  const BroadLocation* pRoot = gGame.GetPlayer()->Current();
  sVisitMask[pRoot->view->Parent()->GetCube()] = (1 << pRoot->subdivision);
  for(int side=0; side<NUM_SIDES; ++side) {
    outPath->steps[0] = side;
    if (Visit(outPath, *pRoot, (Side)side, 1, outViewId)) {
      return true;
    }
  }
  outPath->steps[0] = -1;
  return false;
}

