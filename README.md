**TRS-8051 BASIC Interpreter Overview**
Version 1.01 (c) 2017 by Dennis Hawkins (dennis@galliform.com)

First and foremost, this is a rewrite of TRS-80 LEVEL 2 BASIC and not an emulation of the actual microsoft binaries.  There is no microsoft code here.

This project started out as a hardware simulation of a TRS-80 Model III running Level 2 BASIC.  The hardware consisted of a single Silicon Labs Laser Bee 8051 CPU and a RAM chip.  The Laser Bee alone literally has about the same power as the original Model I TRS-80.  Using the Laser Bee and a single RAM chip, I was able to generate a stable VGA video signal entirely in software with plenty of CPU cycles left to actually do stuff.

With the hardware working, I turned my attention to developing a BASIC interpreter.  There are numerous interpreters already out there, but they are lacking in functionality.   Many were based on Tiny Basic which is an integer only BASIC with only single letter variables.  So I decided to write my own Level II BASIC complete with full floating point support.  This was easier said than done and after having done it, I have found a great deal of respect for the original author, Bill Gates, who allegedly wrote Level II BASIC in a couple of months.  This project took me about 6 months to complete in my spare time and it still isn't really done.

Then to add insult to injury, after the project was nearly complete, I stumbled across another TRS-80 Level 2 BASIC interpreter and source called AWBASIC written by Anthony Wood.  He had already written his interpreter back in 2002.  Although not designed for the 8051, had I known about it earlier, it would have shortened my design cycle considerably.

When I decided to write this interpreter from scratch, I decided to start out developing it on 
a Windows platform because the development tools are a lot easier to use than they are for the 
8051.  I use a hardware abstraction layer to emulate my TRS-8051 hardware at the source level.  
That is why this program exists in a Windows version.  

Someone may look at the source and say, “Wow, this is a really stupid way to do it”, but I can 
assure everyone that some odd coding was done deliberately to make the code compile better 
binaries for the 8051.  On the Laser Bee, there is only 60K of program space and 2K  internal 
RAM remaining after the Video resources are deducted.  As a result, the code is laced with tons 
of globals and goto statements, and function parameters are deliberately kept to a bare 
minimum.  The hope is that this style of coding will help keep the 8051 code segment size down.

The problem is that the 8051 instruction set has far less code density than the x86 processor 
used by Windows.  Since the Windows version uses 97K, it means that it wont fit inside of an 
8051.  Most of the Windows code that is used for the hardware abstraction layer is done with 
DLL's so its not as big a part of the code size as you would think.  So the final version that 
is actually ported back to the TRS-8051 hardware will have to have stuff hacked out of it.  I 
also try to take advantage of sub-local variables any chance I get.

Meanwhile, I am making this version and its source available to anyone who wants it.  I'm going 
to have to hack stuff out and rewrite a lot of stuff if it is ever going to fit inside a real 
8051.  If you use any of this code for your own purposes, please remember to give credit where 
credit is due.  Also, I'd love to hear from anybody who uses this code.

Dennis Hawkins
dennis@galliform.com
