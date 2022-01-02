#include <MsgBoxConstants.au3>
#include <WindowsConstants.au3>
#include <FileConstants.au3>

;AutoItSetOption("TrayIconDebug", 1)
;#AutoIt3Wrapper_Run_Debug_Mode=Y

; this script calls  Supercardsd.exe, loads the ROM "g.gba" from supercard dir ,
; then applies a series of different patch settings.
; the patched roms are saved as out/gX.gba where X is the number of the step.
; see mStep array below to see which number corresponds to each patch setting.
; the reason we load "g.gba" rather than allowing to use a custom path is that for
; some reason we cannot send backslash characters to the file open dialog.
; before using this script, change the options of the supercardsd program to have
; everything unchecked by default!
; the program may sporadically fail with exit code 2, so if you use this in an automated
; scenario, just try again.
; IMPORTANT - Supercardsd.exe has some really buggy code populating the Cheats
; treeview. It will crash on about 10% of all roms, so you better rename your
; cheats directory and create an empty cheats dir before using this!

Global $mainwin
Global $pid
Global $romno


Func AddClick()
   ControlClick($mainwin, "", "[CLASS:TsuiButton; INSTANCE:3]")
EndFunc

Func OutClick()
   ControlClick($mainwin, "", "[CLASS:TsuiButton; INSTANCE:1]")
EndFunc

Func OKClick($okwin)
   ControlClick($okwin, "", "[CLASS:TButton; INSTANCE:1]")
EndFunc

Func AddRom($name)
   AddClick()
   CheckError()
   Local $dlg = WinWaitActive("[CLASS:#32770]")
   Local $ctext
   ControlSend($dlg, "", "[CLASS:Edit; INSTANCE:1]", "g.gba")
   ControlSend($dlg, "", "[CLASS:Edit; INSTANCE:1]", "{ENTER}")
   CheckError()
   WinWaitActive($mainwin, "", 1)
   CheckError()

   Sleep(20)
   Local $nag=WinActive("[CLASS:TForm11]")
   If Not $nag Then
	CheckError()
	Return
   EndIf
   ConsoleWrite("nag detected" & @CRLF)
   Local $nagbox = ControlGetHandle($nag,"","[CLASS:TsuiComboBox; INSTANCE:1]")
   ControlClick($nag, "", $nagbox)
   For $i = 0 To 20
	  $ctext=ControlGetText($nag,"",$nagbox)
	  ConsoleWrite("ctext " & $ctext & @CRLF)
	  If $ctext=$romno Then
		 ExitLoop
	  EndIf
	  ControlSend($nag, "", $nagbox, "{DOWN}")
	  if $i > 10 Then
		ConsoleWrite("nagbox: can't find romno" & @CRLF)
		ProcessClose($pid)
		Exit(4)
	  EndIf
   Next
   ControlClick($nag, "", "[CLASS:TsuiButton; INSTANCE:1]")
   WinWaitActive($mainwin, "", 1)
   Sleep(100)
   ConsoleWrite("ret from nagbox" & @CRLF)
   Return
EndFunc

Func OpenRomSettings()
	  Local $listbox = ControlGetHandle($mainwin,"","[CLASS:TsuiListView; INSTANCE:1]")
	  ControlFocus($mainwin,"",$listbox)
	  ControlListView($mainwin, "", $listbox, "Select", 0)

	  ;ControlSend($mainwin,"",$listbox, "{DOWN}")
	  ;Sleep(1000)
	  ;For $y = 3 To 4
	;	 For $x = 3 to 4
	;		ControlClick($mainwin,"",$listbox, "left", 2, $x, $y)
	;	 Next
	 ; Next
	 ControlClick($mainwin,"",$listbox, "left", 2, 4, 4)
	 ;ControlClick($mainwin,"",$listbox, "left", 2, 4, 4)
	  ;ControlSend($mainwin,"",$mainwin, "{APPSKEY}")
	  ;Send("{APPSKEY}")
	  CheckError()
	  Return WinWaitActive("Properties")
EndFunc

; due to not being able to read checked status, we can only negate the previous status
; therefore the supercard sd app needs to be configured to default to everything off -
; start it once by hand, go to options, and turn all checkboxes off, then exit.
Func ToggleCheckBox($win, $id, $enable)
      Sleep(5)
	  Local $chkbox = ControlGetHandle($win,"",$id)
	  ; status is always returned as 0 - probably due to the "nice" Raize components used...
	  ;Local $status = ControlCommand($win, "", $chkbox, "IsChecked", "")
	  ;MsgBox($MB_SYSTEMMODAL, "", "status ; " & $status & ", ena " & $enable)
	  Local $status = 999 ; hack hack
	  If $status <> $enable Then
		 Local $verb = "Check"
		 ;If not $enable then $verb = "Uncheck"
		 ControlCommand($win, "", $chkbox, $verb, "")
	  EndIf
	  Sleep(5)
EndFunc

; surprise surprise: map feature is only implemented in beta, even though documentation of stable advertises it...
Func mChkIds($name)
   switch($name)
   case "EnableSave"
	  return 8 ; according to manual, this is prerequisite to be able to save battery. ; it creates a 64KB .sav file, "Enable More Saver "a 256 KB one.
   case "EnableSaverPatch"
	  return 11 ; according to manual, needs to be used together with restart and enable save ; this allows to save the battery to file using L+R+SEL+A
   case "EnableRealTimeSave"
	  return 5 ; according to manual, needs to be used together with restart and enable save; creates a SCI file and allows to save state with L+R+SEL+B
   case "EnableMoreRealTimeSave"
	  return 3 ; needs EnableRealTimeSave first; creates larger SCI file
   case "EnableRestart"
	  return 10 ; L+R+SEL+START gets you back to game menu; L+R+A+B+X+Y gets you back to supercard menu; might cause issues with some games
   case "EnableCoerciveRestart"
	  return 1 ; needs EnableRestart Checked first ; this seems to be same as restart but for "GBA Apps", whatever that is.
   case "EnableAddTextFile"
	  return 2  ; this one needs the edit box TRzButtonEdit instance 1 to be filled with the path to text file
   EndSwitch
EndFunc

Func PropCheckBoxToggle($prop, $name)
   Return ToggleCheckBox($prop, "[CLASS:TsuiCheckBox; INSTANCE:" & mChkIds($name) & "]", 1)
EndFunc

Func PropDlgOK($prop)
   Local $ctrl = ControlGetHandle($prop,"","[CLASS:TsuiButton; INSTANCE:1]")
	;ControlFocus($prop,"",$ctrl)
	ControlClick($prop, "", $ctrl)
	;ControlCommand($prop, "", $ctrl, "SendCommandID", $WM_LBUTTONDOWN)
	;Delay(10)
	;ControlCommand($prop, "", $ctrl, "SendCommandID", $WM_LBUTTONUP)
 EndFunc

Func CheckError()
	Local $h1 = 0
	Local $h2 = 0
	Local $h3 = 0
	$h1 = WinActive("Supercardsd", "Cannot make a visible window modal.")
	$h2 = WinActive("Supercardsd", "Access violation")
	$h3 = WinActive("Error", "Open file Error")
	If $h1 Then WinClose($h1)
	If $h2 Then WinClose($h2)
	If $h3 Then WinClose($h3)
	If $h1 Or $h2 Or $h3 Then
;   If WinExists("Supercardsd", "Cannot make a visible window modal.") Or  Then
	  ConsoleWriteError("hitting timing/segv bug, exiting" & @CRLF)
	  WinClose($mainwin)
	  ProcessClose($pid)
	  Exit(2)
	EndIf
EndFunc

Global $supercardpath = "Z:\root\supercard_sd"
Local $outpath = $supercardpath & "\out"
Local $outfile = $outpath & "\g.gba"
If $CmdLine[0] >= 1 Then
   $romno= $CmdLine[1];
Else
   $romno= "0111";
EndIf

FileChangeDir($supercardpath)
$pid = Run("Supercardsd.exe", $supercardpath)
If $pid = 0 Then
   ConsoleWriteError("failed to run executable" & @CRLF)
   Exit(1)
EndIf
$mainwin = WinWaitActive("Super Card V2.71 for SD Version")
AddRom("0002 - Super Mario Advance - Super Mario USA + Mario Brothers (Japan).gba")
;Exit(0)
Local $propwin = OpenRomSettings()

Local $mSteps[4] = ["EnableSave", "EnableSaverPatch", "EnableRestart", "EnableRealTimeSave"]
Local $cmdlinestep = -1
if $CmdLine[0] >= 2 Then
   $cmdlinestep = $CmdLine[2]
EndIf

For $i = 0 to UBound($mSteps)-1
   if $cmdlinestep <> -1 And $cmdlinestep <> $i then ContinueLoop
   Local $step = $mSteps[$i]
   Local $backupfile = $outpath & "\g" & $i & ".gba"
   ConsoleWrite("saving rom with " & $step  & " enabled to " & $backupfile & @CRLF)
   PropCheckBoxToggle($propwin, $mSteps[$i])
   PropDlgOK($propwin)
   OutClick()
   CheckError()
   OKClick(WinWaitActive("OK",  ""))
   Sleep(10)
   FileCopy($outfile, $backupfile, $FC_OVERWRITE)
   if $cmdlinestep = -1 And $i <>  UBound($mSteps)-1 Then
	  $propwin = OpenRomSettings()
	  CheckError()
	  ; cleanup - uncheck all checkboxes
	  PropCheckBoxToggle($propwin, $mSteps[$i])
	  if $step == "EnableRealTimeSave" Then ; this step activates 2 other items - uncheck them
		 PropCheckBoxToggle($propwin, "EnableRestart")
		 PropCheckBoxToggle($propwin, "EnableSave")
	  EndIf
	  Sleep(2)
   EndIf
Next

; done, clean up
; PropDlgOK($propwin)  ; only needed when propdlg was opened on last step
WinClose($mainwin)
Exit(0)
