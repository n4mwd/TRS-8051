AUTO RUN
defint a-z
DIM SHIPANIM$(3, 6), INVADERANIM$(4, 2, 8), BOMBBLAST$(3)
DIM SAUCER$(6), SHELTER$(9)
' Simple variables are allowed to be DIMed
DIM Score, NumShips, ShipX, DrawShipFlag, GameTimer%
DIM InvNum, InvFrame, MoveInvFlag, MoveInvNum
 
; Invader Matrix 
; 1st Subscript = Alien number 0-27
; 2nd subscript = record fields
; Field 0 = Xpos (upper left corner of sprite)
; Field 1 = Ypos
; Field 2 = Invader type 0-4 (0 = exploded, 4 = exploding)
; Field 3 = Invader Animation Frame 0-1

DIM InvMatrix(27,3)

; BOMB ARRAY
; There can be up to 64 simultaneous bomb trails.
; Bomb XPos = Index * 2
; Only the aliens in the lowest row can drop a bomb.
; Bomb YPos is given by the array.  
; If Bomb Ypos=0, then  the bomb has not been dropped.
DIM BombY(63),BombState(63)

_NewGame Come Here for a new game
; Max Bombs is the max number of bombs at one time.
; MaxBombs increases with each level
MaxBombs = 5
GameLevel = 1
NumShips = 5
Score = 0
SauceTimer = 1000 : SauceTimer2 = 2 : SaucerX = 0

_NewSession New Session
GOSUB _LoadCharacters : ' Load characters
NumInvaders% = 28
AlienX = 5 : AlienY = 11 : AlienDir = 1 : InvFrame = 0


_ContSession
rem Initialize some variables
cls
AniBomb = 0
HitShip = 0
HitBomb = 0
ShotBomb = 0
DrawShipFlag = 0
BulletYPos = 0 : BulletXPos = 0
HitAlien = 0 : HitSaucer = 0 : HitBomb = 0 : StageExp = 0
for x = 0 to 63: Bomby(x) = 0 : BombState(x) = 0: next : 'Clear bombs
BombCount = 0

GOsub _DrawScoreLine : ' draw score line
gosub _DrawAllShelters: ' draw 4 shelters

ShipX = 64
shipidx = 0
gosub _DrawShip : ' Draw ship


_PlayLoop Play loop
if NumInvaders% = 0 then goto _WonSession

; Keyboard routine
gosub _CheckKeyboard
if key$ = "" then goto _EndKey
if Key$ = " " then goto _FireBullet
if key$ = "X%" then ShipX = ShipX - 1 : 'Left
if key$ = "X'" then ShipX = ShipX + 1 : 'Right
if (ShipX < 5) then ShipX=5
if (shipX > 122) then ShipX=122
DrawShipFlag = 1
goto _EndKey
_FireBullet
if BulletYPos = 0 then BulletYPos = 69 : BulletXPos = ShipX
_EndKey

_DoDrawBullet   Draw the bullet in flight
if BulletYPos = 0 then goto _EndBullet
; move bullet
BulletYPos = BulletYPos - 3

; Check to see if our bullet hit something
GoSub _CheckHit 

; erase old bullet
for y = 3 to 5
  if BulletYPos + y < 69 then unplot BulletXPos, BulletYPos+y
next y
if BulletYPos < 3 then BulletYpos = 0 : goto _EndBullet

; Check to see if we hit something
if HitSaucer = 0 and HitAlien = 0 and ShotBomb = 0 then _NoBulletHit
; Bullet hit something if here
  BulletYPos = 0
  ShotBomb = 0
  if HitAlien then MoveInvNum = HitAlien
  goto _EndBullet
_NoBulletHit

; Draw new bullet
for y = 0 to 2
if BulletYPos + y > 2 then plot BulletXPos,BulletYPos + y 
next
_EndBullet

_ProcGame  Game Timer
pause 10
GameTimer% = GameTimer% + 1

; Draw the ship if it moved
_DoDrawShip
if DrawShipFlag = 0 then goto _SkipDrawShip
gosub _DrawShip: ' Draw ship
DrawShipFlag = 0 
_SkipDrawShip


;
; SAUCER MOTION SECTION
;

; Starting at 10 seconds and then at random intervals after that
; the saucer is sent across the screen.  
; This section is skipped if the saucer is already in motion
_ActivateSaucer  Activate Saucer
if SaucerX then goto _DoMoveSaucer : 'Saucer already on screen
; SauceTimer is used to randomly set when the saucer appears
SauceTimer = SauceTimer - 1
if SauceTimer <> 0 then goto _DoMoveSaucer
SauceTimer = rnd(1000) + 1
SaucerX = 1 : 'Initiate saucer motion


; The saucer is moved to the right every other game tick
; Once it goes off the screen, it is reset

_DoMoveSaucer  If Saucer is in motion, move it
if SaucerX = 0 then goto _NotSaucer
; Go on every third game tick
if GameTimer% mod 3 = 0 then goto _NotSaucer

; Check to see if the saucer was hit
if HitSaucer then gosub _SaucerHit : goto _NotSaucer
gosub _DrawSaucer 
SaucerX = SaucerX + 1
if SaucerX > 128 then SaucerX = 0
_NotSaucer


; EXPLODE AN INVADER
_ExplodeInvader Explode any aliens that need blowing up
if HitAlien = 0 then goto _NotAlienExplode
gosub _ExplodeAlien 
; Calc MinMax
gosub _CalcMinMax
goto _PlayLoop
_NotAlienExplode

; See if the bombs hit anything else
gosub _CheckBombs
if HitShip = 0 goto _ExplodeBomb
; animate ship explosion
for X = 0 to 4
  ShipIdx = 1
  gosub _DrawShip 
  pause 200
  ShipIdx = 2
  gosub _DrawShip 
  pause 200
next X
  ShipIdx = 3
  gosub _DrawShip 
  goto _EndSession

;
; EXPLODE BOMB
;
_ExplodeBomb
if HitBomb = 0 then goto _NoExplodeBomb
HitBomb = 0
for BombNum = 0 to 63
  if BombState(BombNum) then gosub _BombExplode
Next BombNum
_NoExplodeBomb




; 1st Subscript = Alien number 0-27
; 2nd subscript = record fields
; Field 0 = Xpos (upper left corner of sprite)
; Field 1 = Ypos
; Field 2 = Invader type 0-5 
; (0 = gone, 1-3 = Alien, 4 = exploding, 5=Exploded)
; Field 3 = Invader Animation Frame 0-1

; 
; INVADER MOTION SECTION
;

; START INVADER MOVEMENT
_DoAlienMatrix
;InvMatrix(27,2)=0
;NumInvaders% = 27
if MoveInvNum then goto _DontStartMovement
;if GameTimer% mod NumInvaders% <> 0 then goto _DontStartMovement

; Start invaders in motion for this cycle
MoveInvNum = 1

MinX = 3 - MinAlienX
MaxX = 115 - MaxAlienX
AlienX = AlienX + AlienDir
if AlienX > MaxX then AlienX = MaxX : AlienY = AlienY + 2: AlienDir = -1
if AlienX < MinX then AlienX = MinX : AlienY = AlienY + 2 : AlienDir = 1
;if AlienY = 30 then goto _EndSession
_DontStartMovement


; DO INVADER MOVEMENT
if MoveInvNum = 0 then goto _NotMoving

_SearchLivingInvader Find an invader that is still alive
if InvMatrix(MoveInvNum - 1, 2) <> 0 then goto _FoundLivingInvader
MoveInvNum = MoveInvNum + 1 
if MoveInvNum > 28 then MoveInvNum = 0 : goto _NotMoving 
goto _SearchLivingInvader

; We found an invader that is still alive
_FoundLivingInvader

  Gosub _DrawInvader

; Set invader to next animation frame
  InvMatrix(Z, 3) = (InvMatrix(Z, 3) + 1) mod 2

; Move to next invader
  MoveInvNum = MoveInvNum + 1 : ' Next Invader
  if MoveInvNum > 28 then MoveInvNum = 0 
_NotMoving


; 
; BOMB SECTION
;

; Not sure how the real game did it, but I am
; only allowing 64 bomb trails and only one bomb 
; each at a time.

; INITIATE BOMB DROP
_DropBomb See if we need to drop a bomb on the ship
If BombCount >= MaxBombs then goto _NoDropBomb

; See who wants to drop a bomb.
; only those aliens in the lowest row can drop bombs
for x = 0 to 27 
  If BombCount >= MaxBombs then goto _NoDropBomb
  if InvMatrix(x, 1) <> MaxAlienY then goto _CantDrop
  if InvMatrix(x, 2) = 0 then goto _CantDrop
  if rnd(NumInvaders% * 10) <> 1 then goto _CantDrop
; Drop a bomb if here
; Find closest bomb trail column
  XPos = AlienX + InvMatrix(x, 0)
  Z = (XPos + 6) / 2
test=Z
  if BombY(Z) then goto _CantDrop : ' Already has bomb
;plot Z*2,0
  BombY(Z) = AlienY + InvMatrix(x, 1) + 9 
  BombCount = BombCount + 1
;  goto _NoDropBomb : ' One bomb per pass
_CantDrop
Next X
_NoDropBomb

; BOMB DROP MOVEMENT
_AnimateBombs Bombs are animated here
;if GameTimer% mod 5 <> 0 then goto _NoAnimateBombs
if BombCount = 0 then goto _NoAnimateBombs
; search for column with bomb
_BombLoop
  ; Advance index
  if AniBomb = 63 then AniBomb = 0 else AniBomb = AniBomb + 1
  if BombY(AniBomb) = 0 then goto _BombLoop
  ; If this bomb is exploding, then we skip animation here
  if BombState(AniBomb) then goto _NoAnimateBombs
  ; Found Bomb if here
  xpos = AniBomb * 2
  yPos = BombY(AniBomb)
  ; unplot old bomb
  unplot xpos, ypos
  BombY(AniBomb) = BombY(AniBomb) + 1
  plot xpos, BombY(AniBomb) 
_NoAnimateBombs


goto _PlayLoop




_EndSession Come here when the Aliens get to the bottom
' Or the ship is destroyed
NumShips = NumShips - 1
if NumShips goto _ContSession
? at(23, 12); "G A M E   O V E R"
pause 5000
goto _NewGame



_WonSession: There are no more Aliens
Score = Score + 1000
NumShips = NumShips + 1
GameLevel = GameLevel + 1
MaxBombs = MaxBombs + 5
print at(23, 12); "BONUS 1000 POINTS"
Gosub _DrawScoreLine
pause 5000
print at(23, 12); "BONUS 1000 POINTS"
goto _NewSession


; 1st Subscript = Alien number 0-27
; 2nd subscript = record fields
; Field 0 = Xpos (upper left corner of sprite)
; Field 1 = Ypos
; Field 2 = Invader type 0-5 
; (0 = exploded, 1-3 = Alien, 4 = exploding, 5= exploded)
; Field 3 = Invader Animation Frame 0-1


_CheckHit Check to see if bullet hit something
z = 0
for y = 0 to 2 : z = z + point(BulletXPos, BulletYPos + y) : next
if z = 0 then goto _DoneCheckHit
if BulletYPos < 10 then HitSaucer = 1 : goto _DoneCheckHit
; Search to see if it hit an alien
; Aliens are 12 pixels wide by 8 pixels high
for x = 0 to 27
  if InvMatrix(x, 2) = 0 then goto _SkipDead : ' dont check dead aliens
  Xpos = AlienX + InvMatrix(x,0)
  Ypos = AlienY + InvMatrix(x,1)
  if BulletXPos < Xpos or BulletXPos > Xpos + 12 then goto _SkipDead
  if BulletYpos < Ypos or BulletYPos > YPos + 8 then goto _SkipDead
  HitAlien = x + 1 : Goto _DoneCheckHit
_SkipDead
next

; Did we shoot a bomb
Xpos = int(BulletXPos / 2)
YPos = BombY(XPos) 
;if YPos = 0 then goto _DoneCheckHit
if YPos < BulletYPos then goto _DoneCheckHit
if YPos >= BulletYPos + 3 then goto _DoneCheckHit
  HitBomb = 1
  ShotBomb = 1 
  BombState(Xpos) = 10
_DoneCheckHit
return


; CHECK BOMBS
;
; Check all the bombs to see if they have hit something
;
_CheckBombs
for X = 0 to 63
  Xpos = X * 2
  YPos = BombY(X)
  if YPos = 0 OR BombState(X) then goto _SkipCheckBombs
  
  ; Check if in ship zone
  if YPos < 68 then goto _BombNotInShipZone
    ; The bomb is in the ship zone
    If Xpos < ShipX - 6 OR Xpos > ShipX + 6 then goto _CheckGround
      ; The ship was hit if here
      HitShip = 1 
      goto _EndCheckBombs : ' Ship Hit will end session
;
_CheckGround
      if YPos <> 74 then goto _SkipCheckBombs
        ; bomb hit the ground
        BombState(X) = 10
        HitBomb = 1
        goto _SkipCheckBombs
;
_BombNotInShipZone
  if YPos < 60 OR point(xpos,ypos + 1) = 0 then goto _SkipCheckBombs
    ; The Bomb hit a shelter if here
    BombState(x) = 10
    HitBomb = 1
;
_SkipCheckBombs
Next X
_EndCheckBombs
return



END 


; BOMB EXPLODE
;
; Explode a bomb in stages
; BombNum = the Bomb (0 - 63) to explode
; BombState: 0=do nothing, 1=erase bomb, 2-9 = do nothing, 10=draw explosion
; HitBomb: Set to 1 if bombstate = 2-10
;
_BombExplode
xpos = BombNum
yPos = BombY(BombNum)
bs = BombState(BombNum)

if BS = 0 then goto _EndBombExplode
if BS = 1 then goto _EraseBombExplode
if BS = 10 then goto _NewExplosion
  ; BS = 2-9 here
  HitBomb = 1
  BombState(BombNum) = BS - 1
  goto _EndBombExplode
;
_NewExplosion
  ; BS=10, draw NEW explosion
  BombState(BombNum) = 9
  BombMode = 1
  Gosub _DrawBombBlast
  HitBomb = 1
  goto _EndBombExplode
;
_EraseBombExplode
  BombMode = 0
  Gosub _DrawBombBlast
  ; Remove Bomb
  BombY(BombNum) = 0 
  BombState(BombNum) = 0
  BombCount = BombCount - 1
;
_EndBombExplode
return




; Get min-max for alien matrix
; This gets recalculated every time somebody gets shot
_CalcMinMax
MinAlienX = 1000 : MaxAlienX = 0 : MaxAlienY = 0
for z = 0 to 27
; Ignore dead aliens
  if InvMatrix(z, 2) = 0 then goto _SkipMinMax
    x = InvMatrix(z,0)
    if x > MaxAlienX then MaxAlienX = x
    if x < MinAlienX then MinAlienX = x
    y = InvMatrix(z,1)
    if y > MaxAlienY then MaxAlienY = y
_SkipMinMax
next z
return



; Explode the alien referenced by HitAlien and MoveInvNum
; If here, the HitAlien needs to explode or is in the process of exploding
; MoveInvNum does not get incremented during an explosion
;
_ExplodeAlien  Blow up an alien
; The following line sets the explosion timing
if GameTimer% mod 5 <> 0 then return

; Initialize the explosion stage if its zero
if StageExp = 0 then StageExp = 1 

; Jump to the proper phase of the explosion
on StageExp goto _Explode1, _Explode2, _Explode3, _Explode4
; shouldn't be here
return

_Explode1 Initial explosion
InvMatrix(MoveInvNum - 1, 2) = 4 : ' Exploding
InvMatrix(MoveInvNum - 1, 3) = 0 : ' Frame 0
StageExp = 2
goto _FinishExplode

_Explode2 Secondary Explosion
InvMatrix(MoveInvNum - 1, 3) = 1 : ' Frame 1
StageExp = 3
goto _FinishExplode

_Explode3 Final Explosion
InvMatrix(MoveInvNum - 1, 2) = 5 : ' Exploded
StageExp = 4
goto _FinishExplode

_Explode4 Explosion cleanup
InvMatrix(MoveInvNum - 1, 2) = 0 : ' Dead
HitAlien = 0
StageExp = 0
NumInvaders% = NumInvaders% - 1 : ' One less invader
; update score - compute level
Level = (3 - ((MoveInvNum - 1) mod 4))
if Level = 0 then Level = 1
Score = Score + Level * 10
GoSub _DrawScoreLine
MoveInvNum = 1

_FinishExplode
Gosub _DrawInvader
return


; The saucer was hit.  
_SaucerHit
Bonus = 50 * (Rnd(9) + 1)
if HitSaucer <> 1 then goto _NotHit1
; HitSaucer = 1 if here
  Print AT(SaucerX / 2, 1);"BONUS  "
  print AT(SaucerX / 2, 2);Bonus;" Points";
  score = Score + Bonus
  gosub _DrawScoreLine
_NotHit1
;
; Erase Bonus text in 50 cycles
if HitSaucer <> 50 then goto _NotHit50
; HitSaucer = 50 if here
  Print AT(SaucerX / 2, 1);"       "
  print AT(SaucerX / 2, 2);"          ";AT(0, 26)
  HitSaucer = 0
  SaucerX = 0
  return
_NotHit50
;
HitSaucer = HitSaucer + 1
return




_CheckKeyboard  Check for keystrokes
Key$=inkey$
if Key$ = chr$(27) then end
_KeyLoop Make sure keybuffer is empty
rem if inkey$ <> "" goto _KeyLoop
return


_DrawScoreLine Draw Score Line and hide cursor
? @0,"SCORE: ";score;
? @28,"LEVEL: ";GameLevel;
? @54,"SHIPS: "; NumShips; AT(0,26)
return



_LoadCharacters 
Restore

' Load ship and ship explosions
FOR Y = 0 TO 2 : FOR X = 0 TO 5
  READ SHIPANIM$(Y, X)
NEXT X, Y

' Load Invaders and explosions
FOR Z = 1 TO 4 : FOR Y = 0 TO 1 : FOR X = 0 TO 7
  READ INVADERANIM$(Z, Y, X)
NEXT X, Y, Z

' Load InvMatrix(27,3)
for x = 0 to 6 : for y = 0 to 3
  Z = x * 4 + y
  InvMatrix(Z, 0) = x * 15 : ' XPos
  InvMatrix(Z, 1) = y * 10 : ' YPos
  InvMatrix(Z, 2) = y + 1 : ' Alien type
  if InvMatrix(Z, 2) > 3 then InvMatrix(Z, 2) = 3
  InvMatrix(X, 3) = rnd(1) : ' Animation Frame
next y,x
; hard code initial min-max
MinAlienX = 0
MaxAlienX = 90
MaxAlienY = 30

' Load Saucer
FOR X = 1 TO 6
  READ SAUCER$(X)
NEXT X

' Load Shelter
FOR X = 1 TO 9
  READ SHELTER$(X)
NEXT X

' Load Bomb Blast
FOR X = 0 to 3
  Read BombBlast$(X)
Next X

RETURN 



_DrawShip Subroutine to draw ship
' ShipIdx is the index into the array to draw
FOR I = 0 TO 5 : FOR J = 1 TO 13
  IF MID$(SHIPANIM$(SHIPIDX, I), J, 1) = "#" THEN PLOT ShipX + J - 7, 69 + I ELSE UNPLOT ShipX + J - 7, 69 + I
NEXT J, I
RETURN 


; This is used for all Bomb explosions except for ship
; XPOS and YPOS indicate location of bomb center
; BombMode = 1 to draw bomb blast, 0 to erase it
_DrawBombBlast
for Y = 0 to 3 : For X = 1 to 3
  Xc = xpos * 2 + X - 2
  Yc = ypos + y - 2
  if MID$(BombBlast$(Y), x, 1) <> "#" then goto _SkipBlast
  IF BombMode THEN PLOT xc, yc else UNPLOT xc,yc
_SkipBlast
next x,y
return


; 1st Subscript = Alien number 0-27
; 2nd subscript = record fields
; Field 0 = Xpos (upper left corner of sprite)
; Field 1 = Ypos
; Field 2 = Invader type 0-5 
; (0 = exploded, 1-3 = Alien, 4 = exploding, 5= exploded)
; Field 3 = Invader Animation Frame 0-1

_DrawInvader  Subroutine to draw Invader
' MoveInvNum specifies which invader cell + 1
' Invader type 0-4 (0 = gone, 1-3 = Alien, 4 = exploding, 5=exploded)
Z = MoveInvNum - 1
InvType = InvMatrix(Z, 2)
if InvType = 0 then return
if InvType = 5 then InvType = 0
Frame = InvMatrix(Z, 3)
XPos =  AlienX + InvMatrix(Z, 0) - 1
YPos = AlienY + InvMatrix(Z, 1) 
for j = 0 to 11 : UNPLOT Xpos + J, YPos-1 :UNPLOT Xpos + J, YPos-2 : next
FOR I = 0 TO 7 : FOR J = 1 TO 12
  IF MID$(INVADERANIM$(InvType, Frame, I), J, 1) = "#" THEN PLOT Xpos + J, YPos + I ELSE UNPLOT XPos + J, YPos + I
NEXT J, I
RETURN 







_DrawSaucer Subroutine to draw saucer
FOR I = 0 TO 5 : FOR J = 1 TO 13
  IF MID$(SAUCER$(I), J, 1) = "#" THEN PLOT SaucerX + J, 3 + I ELSE UNPLOT SaucerX + J, 3 + I
NEXT J, I
RETURN 

_DrawShelter Subroutine to draw shelter
FOR I = 1 TO 9 : FOR J = 1 TO 20
  IF MID$(SHELTER$(I), J, 1) = "#" THEN PLOT XOFF + J, YOFF + I ELSE UNPLOT XOFF + J, YOFF + I
NEXT J, I
RETURN 

_DrawAllShelters Draw 4 shelters
for k = 0 to 3
yoff = 59
xoff = 9 + 30 * k
gosub _DrawShelter: ' draw one shelter
next
return



10000 ' define ship (13x6)
DATA "      #      "
DATA "     ###     "
DATA "     ###     "
DATA "  #########  "
DATA " ########### "
DATA " ########### "

' define ship explosion 1 (13x6)
DATA "   #         "
DATA "  #   #   #  "
DATA "   #  #  #   "
DATA "  #   ## # # "
DATA "   # #### #  "
DATA " #  ######   "

' define ship explosion 2 (13x6)
DATA "  #   #  #   "
DATA "    #        "
DATA "  #   #   #  "
DATA "     #  #    "
DATA "  # ## #  #  "
DATA "   ######  # "

' define Invader 1-A (12x8)
DATA "     #      "
DATA "    ###     "
DATA "   #####    "
DATA "  # ### #   "
DATA " #########  "
DATA "    # #     "
DATA "   #   #    "
DATA " ##     ##  "

' define Invader 1-B (12x8)
DATA "     #      "
DATA "    ###     "
DATA "   #####    "
DATA "  # ### #   "
DATA " #########  "
DATA "   # # #    "
DATA "  #     #   "
DATA "   #   #    "

' define Invader 2-A (12x8)
DATA "   #   #    "
DATA " #  # #  #  "
DATA " # ##### #  "
DATA " ### # ###  "
DATA " #########  "
DATA "  #######   "
DATA "   #   #    "
DATA "  #     #   "

' define Invader 2-B (12x8)
DATA "   #   #    "
DATA "    # #     "
DATA "   #####    "
DATA "  ## # ##   "
DATA " #########  "
DATA " # ##### #  "
DATA " # #   # #  "
DATA "    # #     "

' define Invader 3-A (12x8)
DATA "    ####    "
DATA "  ########  "
DATA " ########## "
DATA " ##  ##  ## "
DATA " ########## "
DATA "   ##  ##   "
DATA "   # ## #   "
DATA " ##      ## "

' define Invader 3-B (12x8)
DATA "    ####    "
DATA "  ########  "
DATA " ########## "
DATA " ##  ##  ## "
DATA " ########## "
DATA "   ##  ##   "
DATA "  #  ##  #  "
DATA "   #    #   "

' define invader explosion 1 (12x8)
DATA "    #  #    "
DATA " #   ##   # "
DATA "  #      #  "
DATA "   #    #   "
DATA " ##      ## "
DATA "   #    #   "
DATA "  #  ##  #  "
DATA " #  #  #  # "

' define invader explosion 2 (12x8)
DATA "    #       "
DATA "  #      #  "
DATA "      #     "
DATA "    #       "
DATA "  #    #    "
DATA "     #      "
DATA "  #      #  "
DATA "       #    "


' define saucer (13x6)
DATA "    ######   "
DATA "  ########## "
DATA " ## ##  ## ##"
DATA " ############"
DATA "   ###  ###  "
DATA "    #    #   "

' define shelter (20x9)
DATA "    ############    "
DATA "   ##############   "
DATA "  ################  "
DATA " ################## "
DATA "####################"
DATA "####################"
DATA "#######      #######"
DATA "######        ######"
DATA "#####          #####"

; Bomb Blast (3x4)
DATA " # "
DATA "###"
DATA "###"
DATA " # "


; This is an example of a hidden comment.
; Hidden comments exist in the source file,
; but are ignored by the LOAD command.
; Hidden comments must have a semicolon
; in the first column.
; Labels are converted by the LOAD command 
; into actual Line numbers.  Labels begin
; with an underscore.
goto _MyLabel : ' Forward reference
_MyLabel This part of a label becomes a comment
; Use the LIST command to see how this converted.  
; Be sure not to SAVE over top of the source or 
; else all the Labels and hidden comments are
; destroyed.
  

