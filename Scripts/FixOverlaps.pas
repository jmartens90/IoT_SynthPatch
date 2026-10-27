Procedure FixOverlaps;
Var
  Board : IPCB_Board;
  Iter  : IPCB_BoardIterator;
  Comp  : IPCB_Component;
  N     : Integer;
Begin
  Board := PCBServer.GetCurrentPCBBoard;
  If Board = Nil Then Exit;
  N := 0;
  PCBServer.PreProcess;
  Iter := Board.BoardIterator_Create;
  Iter.AddFilter_ObjectSet(MkSet(eComponentObject));
  Iter.AddFilter_LayerSet(AllLayers);
  Iter.AddFilter_Method(eProcessAll);
  Comp := Iter.FirstPCBObject;
  While Comp <> Nil Do
  Begin
    If Comp.Name.Text = 'C25' Then
    Begin
      PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_BeginModify, c_NoEventData);
      Comp.MoveToXY(Board.XOrigin + MMsToCoord(94.653), Board.YOrigin + MMsToCoord(23.278));
      PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_EndModify, c_NoEventData);
      N := N + 1;
    End;
    If Comp.Name.Text = 'J25' Then
    Begin
      PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_BeginModify, c_NoEventData);
      Comp.MoveToXY(Board.XOrigin + MMsToCoord(2.300), Board.YOrigin + MMsToCoord(65.300));
      PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_EndModify, c_NoEventData);
      N := N + 1;
    End;
    If Comp.Name.Text = 'C82' Then
    Begin
      PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_BeginModify, c_NoEventData);
      Comp.MoveToXY(Board.XOrigin + MMsToCoord(40.653), Board.YOrigin + MMsToCoord(11.028));
      PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_EndModify, c_NoEventData);
      N := N + 1;
    End;
    If Comp.Name.Text = 'R41' Then
    Begin
      PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_BeginModify, c_NoEventData);
      Comp.MoveToXY(Board.XOrigin + MMsToCoord(54.403), Board.YOrigin + MMsToCoord(14.778));
      PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_EndModify, c_NoEventData);
      N := N + 1;
    End;
    If Comp.Name.Text = 'C89' Then
    Begin
      PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_BeginModify, c_NoEventData);
      Comp.MoveToXY(Board.XOrigin + MMsToCoord(64.403), Board.YOrigin + MMsToCoord(14.778));
      PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_EndModify, c_NoEventData);
      N := N + 1;
    End;
    Comp := Iter.NextPCBObject;
  End;
  Board.BoardIterator_Destroy(Iter);
  PCBServer.PostProcess;
  Board.ViewManager_FullUpdate;
  ShowMessage('Moved ' + IntToStr(N) + ' parts.');
End;
