#pragma once

#include "View.h"
#include "assets.gen.h"
#include "AudioPlayer.h"

namespace TotalsGame {

  class NarratorView : public View {

      DECLARE_POOL(NarratorView, 1);

      static const AssetImage *emotes[];

  public:
      enum Emote
      {
            EmoteCoin,
            EmoteDiamond,
            EmoteEmerald,
            EmoteGetReady,
            EmoteMix01,
            EmoteMix02,
            EmoteRuby,
            EmoteSad,
            EmoteWave,
            EmoteYay,

          EmoteNone
      };
private:
    const char *mString;
    Emote mEmote;

    int mOffset;


  public:
    NarratorView(TotalsCube *c);

    void SetMessage(const char *msg, Emote emote=EmoteNone);

    void SetEmote(Emote emote=EmoteNone);

    void SetTransitionAmount(float u);

    void Paint();

    void PaintText();
  };

}

