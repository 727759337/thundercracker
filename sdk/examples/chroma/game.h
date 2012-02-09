/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo SDK Example.
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#ifndef _GAME_H
#define _GAME_H

#include <sifteo.h>
#include "Level.h"
#include "cubewrapper.h"
#include "TimeKeeper.h"
#include "config.h"

using namespace Sifteo;

//singleton class
class Game
{
public:    
	typedef enum
	{
		STATE_SPLASH,
#if SPLASH_ON
        STARTING_STATE = STATE_SPLASH,
#endif
		STATE_MENU,
        STATE_INTRO,
#if !SPLASH_ON
        STARTING_STATE = STATE_INTRO,
#endif
		STATE_PLAYING,		
        STATE_DYING,
		STATE_POSTGAME,
	} GameState;

	typedef enum
	{
		MODE_SHAKES,
		MODE_TIMED,
		MODE_PUZZLE,
	} GameMode;

	static Game &Inst();
	
	Game();

    //static const int NUM_CUBES = 3;
    static const unsigned int NUM_HIGH_SCORES = 5;
    static const int STARTING_SHAKES = 0;
    static const unsigned int NUM_SFX_CHANNELS = 3;
    static const int NUM_SLOSH_SOUNDS = 2;
    static const unsigned int INT_MAX = 0x7fff;
    static const float SLOSH_THRESHOLD;
    static const int NUM_COLORS_FOR_HYPER = 3;
    //timer constants
    static const float TIME_TO_RESPAWN;
    static const float COMBO_TIME_THRESHOLD;
    static const int MAX_MULTIPLIER = 7;

    //number of dots needed for certain thresholds
    enum
    {
        DOT_THRESHOLD1 = 2,
        DOT_THRESHOLD2 = 4,
        DOT_THRESHOLD_TIMED_RAINBALL = 6,
        DOT_THRESHOLD_TIMED_MULT = 9,
        DOT_THRESHOLD3 = 9,
        DOT_THRESHOLD4 = 14,
        DOT_THRESHOLD5 = 15,
    };

    CubeWrapper m_cubes[NUM_CUBES];
    static Math::Random random;

	void Init();
	void Update();
	void Reset();

	//flag self to test matches
	void setTestMatchFlag() { m_bTestMatches = true; }

	unsigned int getIncrementScore() { m_iDotScoreSum += ++m_iDotScore; return m_iDotScore; }

	inline GameState getState() const { return m_state; }
    inline void setState( GameState state ) { m_state = state; }
	inline GameMode getMode() const { return m_mode; }

	inline unsigned int getScore() const { return m_iScore; }
    inline void addScore( unsigned int score ) { m_iScore += score; }
    inline const Level &getLevel() const { return Level::GetLevel( m_iLevel ); }
    inline void addLevel() { m_iLevel++; }
    inline unsigned int getDisplayedLevel() const { return m_iLevel + 2; }

	TimeKeeper &getTimer() { return m_timer; }
    unsigned int getHighScore( unsigned int index ) const;
    void enterScore();

	void CheckChain( CubeWrapper *pWrapper );
	void checkGameOver();
	bool NoMatches();
	unsigned int numColors() const;
	bool no_match_color_imbalance() const;
	bool no_match_stranded_interior() const;
	bool no_match_stranded_side() const;
	bool no_match_mismatch_side() const;
    unsigned int NumCubesWithColor( unsigned int color ) const;
    bool IsColorUnmatchable( unsigned int color ) const;
    bool AreAllColorsUnmatchable() const;
    bool DoCubesOnlyHaveStrandedDots() const;
    bool OnlyOneOtherCorner( const CubeWrapper *pWrapper ) const;

    void playSound( _SYSAudioModule &sound );
    //play random slosh sound
    void playSlosh();

    inline void forcePaintSync() { m_bForcePaintSync = true; }

    inline unsigned int getShakesLeft() const { return m_ShakesRemaining; }
    inline void useShake() { m_ShakesRemaining--; }

    //destroy all dots of the given color
    void BlowAll( unsigned int color );
    void EndGame();

    inline void Stabilize() { m_bStabilized = true; }

    bool AreNoCubesEmpty() const;
    unsigned int CountEmptyCubes() const;

    inline void SetUsedColor( unsigned int color ) { m_aColorsUsed[color] = true; }
    void UpCombo();
    inline unsigned int GetComboCount() const { return m_comboCount; }
    void UpMultiplier();

private:
	void TestMatches();
    bool DoesHyperDotExist();
    //add one piece to the game
    void RespawnOnePiece();

	bool m_bTestMatches;
	//how much our current dot is worth
	unsigned int m_iDotScore;
	//running total
	unsigned int m_iDotScoreSum;
	unsigned int m_iScore;
	unsigned int m_iDotsCleared;
    //how many colors were involved in this
    bool m_aColorsUsed[ GridSlot::NUM_COLORS ];
	//for progression in shakes mode
	unsigned int m_iLevel;
	GameState m_state;
	GameMode m_mode;
	float m_splashTime;
	TimeKeeper m_timer;
    float m_fLastTime;
    float m_fLastSloshTime;

#if SFX_ON
    AudioChannel m_SFXChannels[NUM_SFX_CHANNELS];
#endif
    //which channel to use
    unsigned m_curChannel;
#if MUSIC_ON
    AudioChannel m_musicChannel;
#endif
    //use to avoid playing the same sound multiple times in one frame
    const _SYSAudioModule *m_pSoundThisFrame;

    static unsigned int s_HighScores[ NUM_HIGH_SCORES ];
    unsigned int m_ShakesRemaining;
    //how long until we respawn one piece in timer mode
    float m_fTimeTillRespawn;
    //which cube to respawn to next
    unsigned int m_cubeToRespawn;
    unsigned int m_comboCount;
    float m_fTimeSinceCombo;
    unsigned int m_Multiplier;

    //force a 1 frame paint sync before/after drawing
    bool m_bForcePaintSync;
    //keeps track of whether a hyperdot was used this chain
    //bool m_bHyperDotMatched;
    //set to true every time the state of the game is stabilized to run checks on
    bool m_bStabilized;
};

#endif
