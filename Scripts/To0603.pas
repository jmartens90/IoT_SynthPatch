{ Converts the 0805 resistor/capacitor footprints in the open PcbLib to 0603 (1608 metric).
  Pads 0.80 x 0.95 mm at +/-0.825 mm (IPC nominal). Old silkscreen/courtyard/3D removed, new silk added. }
Procedure ConvertOne(Lib : IPCB_Library; FP : IPCB_LibComponent);
Var
  GI : IPCB_GroupIterator; Prim : IPCB_Primitive; Del : TInterfaceList;
  Pads : TInterfaceList; P1, P2 : IPCB_Pad; Mid, MidY : TCoord; i : Integer;
  T : IPCB_Track;
Begin
  Del := TInterfaceList.Create; Pads := TInterfaceList.Create;
  GI := FP.GroupIterator_Create;
  GI.AddFilter_ObjectSet(AllObjects);
  Prim := GI.FirstPCBObject;
  While Prim <> Nil Do
  Begin
    If Prim.ObjectId = ePadObject Then Pads.Add(Prim) Else Del.Add(Prim);
    Prim := GI.NextPCBObject;
  End;
  FP.GroupIterator_Destroy(GI);
  If Pads.Count <> 2 Then Begin ShowMessage(FP.Name + ': expected 2 pads'); Exit; End;
  P1 := Pads.Items[0]; P2 := Pads.Items[1];
  Mid := (P1.X + P2.X) Div 2; MidY := (P1.Y + P2.Y) Div 2;
  PCBServer.PreProcess;
  For i := 0 To Del.Count - 1 Do
  Begin
    Prim := Del.Items[i];
    FP.RemovePCBObject(Prim);
    PCBServer.DestroyPCBObject(Prim);
  End;
  For i := 0 To 1 Do
  Begin
    P1 := Pads.Items[i];
    PCBServer.SendMessageToRobots(P1.I_ObjectAddress, c_Broadcast, PCBM_BeginModify, c_NoEventData);
    If P1.X < Mid Then P1.X := Mid - MMsToCoord(0.825) Else P1.X := Mid + MMsToCoord(0.825);
    P1.Y := MidY;
    P1.TopXSize := MMsToCoord(0.80); P1.TopYSize := MMsToCoord(0.95);
    P1.MidXSize := MMsToCoord(0.80); P1.MidYSize := MMsToCoord(0.95);
    P1.BotXSize := MMsToCoord(0.80); P1.BotYSize := MMsToCoord(0.95);
    PCBServer.SendMessageToRobots(P1.I_ObjectAddress, c_Broadcast, PCBM_EndModify, c_NoEventData);
  End;
  For i := 0 To 1 Do
  Begin
    T := PCBServer.PCBObjectFactory(eTrackObject, eNoDimension, eCreate_Default);
    T.Layer := eTopOverlay; T.Width := MMsToCoord(0.12);
    T.X1 := Mid - MMsToCoord(0.2); T.X2 := Mid + MMsToCoord(0.2);
    If i = 0 Then Begin T.Y1 := MidY + MMsToCoord(0.62); T.Y2 := T.Y1; End
    Else Begin T.Y1 := MidY - MMsToCoord(0.62); T.Y2 := T.Y1; End;
    FP.AddPCBObject(T);
    PCBServer.SendMessageToRobots(FP.I_ObjectAddress, c_Broadcast, PCBM_BoardRegisteration, T.I_ObjectAddress);
  End;
  FP.Description := '0603 (1608 metric), converted from 0805';
  PCBServer.PostProcess;
  Del.Free; Pads.Free;
End;

Procedure To0603;
Var Lib : IPCB_Library; It : IPCB_LibraryIterator; FP : IPCB_LibComponent; N : Integer; L : TInterfaceList; i : Integer;
Begin
  Lib := PCBServer.GetCurrentPCBLibrary;
  If Lib = Nil Then Begin ShowMessage('Open IoT_SYNTHPATCH.PcbLib first'); Exit; End;
  L := TInterfaceList.Create;
  It := Lib.LibraryIterator_Create;
  It.SetState_FilterAll;
  FP := It.FirstPCBObject;
  While FP <> Nil Do
  Begin
    If (UpperCase(FP.Name) = 'CAPC2013X145X48NL20T23') Or (UpperCase(FP.Name) = 'FP-RN73R2A-IPC_A') Then L.Add(FP);
    FP := It.NextPCBObject;
  End;
  Lib.LibraryIterator_Destroy(It);
  For i := 0 To L.Count - 1 Do ConvertOne(Lib, L.Items[i]);
  Lib.RefreshView;
  ShowMessage('Converted ' + IntToStr(L.Count) + ' footprints to 0603.');
  L.Free;
End;
