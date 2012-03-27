#!/usr/bin/env python
"""
script to convert json puzzles in CubeBuddies format to a .h file that will be built into the game.  
"""

import sys
import json

def BuddyNameToId(name):
    return 'BUDDY_' + name.upper()

def SideNameToId(name):
    return 'SIDE_' + name.upper()

def PartNameToId(name):
    return 'PART_' + name.upper()

def BoolToString(value):
    if value:
        return 'true'
    else:
        return 'false'

def main():  
    print sys.argv[1:]
    
    try:
        src = sys.argv[1]
        dest = sys.argv[2]
    except:
        print("Usage: python %s <json filename> <dest filename>" % __file__)
        print args
        exit(1)
    
    with open(src, 'r') as f:
        j = json.load(f)
        data = j
        
        with open(dest, 'w') as fout:
            fout.write('void InitializePuzzles()\n')
            fout.write('{\n')
            
            # Write out the default Puzzle
            fout.write('\tsPuzzleDefault.Reset();\n')
            fout.write('\tsPuzzleDefault.SetBook(%d);\n' % 0)
            fout.write('\tsPuzzleDefault.SetTitle("%s");\n' % "Default")
            fout.write('\tsPuzzleDefault.SetClue("%s");\n' % "Default")
            fout.write('\tsPuzzleDefault.SetCutsceneEnvironment(%d);\n' % 0)
            fout.write('\tsPuzzleDefault.AddCutsceneTextStart("%s");\n' % "<Default")
            fout.write('\tsPuzzleDefault.AddCutsceneTextStart("%s");\n' % ">Default")
            fout.write('\tsPuzzleDefault.SetNumShuffles(%d);\n' % 0)
            fout.write('\tfor (unsigned int i = 0; i < %d; ++i)\n' % 6)
            fout.write('\t{\n')
            fout.write('\t\tsPuzzleDefault.AddBuddy(BuddyId(i));\n')
            fout.write('\t\tsPuzzleDefault.SetPieceStart(i, SIDE_TOP, Piece(BuddyId(i), Piece::PART_HAIR));\n')
            fout.write('\t\tsPuzzleDefault.SetPieceStart(i, SIDE_LEFT, Piece(BuddyId(i), Piece::PART_EYE_LEFT));\n')
            fout.write('\t\tsPuzzleDefault.SetPieceStart(i, SIDE_BOTTOM, Piece(BuddyId(i), Piece::PART_MOUTH));\n')
            fout.write('\t\tsPuzzleDefault.SetPieceStart(i, SIDE_RIGHT, Piece(BuddyId(i), Piece::PART_EYE_RIGHT));\n')
            fout.write('\t\tsPuzzleDefault.SetPieceEnd(i, SIDE_TOP, Piece(BuddyId(i), Piece::PART_HAIR, true));\n')
            fout.write('\t\tsPuzzleDefault.SetPieceEnd(i, SIDE_LEFT, Piece(BuddyId(i), Piece::PART_EYE_LEFT, true));\n')
            fout.write('\t\tsPuzzleDefault.SetPieceEnd(i, SIDE_BOTTOM, Piece(BuddyId(i), Piece::PART_MOUTH, true));\n')
            fout.write('\t\tsPuzzleDefault.SetPieceEnd(i, SIDE_RIGHT, Piece(BuddyId(i), Piece::PART_EYE_RIGHT, true));\n')
            fout.write('\t}\n')        
            fout.write('\n')
            
            # Write out the actual puzzles from the JSON input
            for i, puzzle in enumerate(data):
                fout.write('\t// Puzzle %d\n' % i)
                fout.write('\tASSERT(%d < arraysize(sPuzzles));\n' % i)
                fout.write('\tsPuzzles[%d].Reset();\n' % i)
                fout.write('\tsPuzzles[%d].SetBook(%d);\n' % (i, puzzle['book']))
                fout.write('\tsPuzzles[%d].SetTitle("%s");\n' % (i, puzzle['title']))
                fout.write('\tsPuzzles[%d].SetClue("%s");\n' % (i, puzzle['clue']))
                fout.write('\tsPuzzles[%d].SetCutsceneEnvironment(%d);\n' % (i, puzzle['cutscene_environment']))
                for line in puzzle['cutscene_start']:
                    fout.write('\tsPuzzles[%d].AddCutsceneTextStart("%s");\n' % (i, line.replace('\n', '\\n')))
                for line in puzzle['cutscene_end']:
                    fout.write('\tsPuzzles[%d].AddCutsceneTextEnd("%s");\n' % (i, line.replace('\n', '\\n')))
                fout.write('\tsPuzzles[%d].SetNumShuffles(%d);\n' % (i, puzzle['shuffles']))
                for buddy in puzzle['buddies']:
                    fout.write('\tsPuzzles[%d].AddBuddy(%s);\n' % (i, BuddyNameToId(buddy)))
                for j, buddy in enumerate(puzzle['pieces_start']):
                    for side in buddy:
                        piece = buddy[side]
                        if not piece.has_key('must_solve'):
                            piece['must_solve'] = 0
                        fout.write('\tsPuzzles[%d].SetPieceStart(%d, %s, Piece(%s, Piece::%s));\n' % (i, j, SideNameToId(side), BuddyNameToId(piece['buddy']), PartNameToId(piece['part'])))
                for j, buddy in enumerate(puzzle['pieces_end']):
                    for side in buddy:
                        piece = buddy[side]
                        if not piece.has_key('must_solve'):
                            piece['must_solve'] = False
                        fout.write('\tsPuzzles[%d].SetPieceEnd(%d, %s, Piece(%s, Piece::%s, %s));\n' % (i, j, SideNameToId(side), BuddyNameToId(piece['buddy']), PartNameToId(piece['part']), BoolToString(piece['must_solve'])))
                fout.write('\n')
            
            fout.write('\tsNumPuzzles = %d;\n' % len(data))
            fout.write( '};\n\n' )
	    
        

if __name__ == "__main__":
    main()
