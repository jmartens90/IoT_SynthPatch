// Select the board-edge tracks on Mechanical 1 (for Design > Board Shape > Define from selected objects).
Procedure SelectMechOutline;
Var
  Board : IPCB_Board;
  It    : IPCB_BoardIterator;
  T     : IPCB_Track;
  n     : Integer;
Begin
  Board := PCBServer.GetCurrentPCBBoard;
  If Board = Nil Then Exit;
  Client.SendMessage('PCB:DeSelect', 'Scope=All', 255, Client.CurrentView);
  n := 0;
  It := Board.BoardIterator_Create;
  It.AddFilter_ObjectSet(MkSet(eTrackObject));
  It.AddFilter_LayerSet(MkSet(eMechanical1));
  It.AddFilter_Method(eProcessAll);
  T := It.FirstPCBObject;
  While T <> Nil Do
  Begin
    If T.Component = Nil Then Begin T.Selected := True; n := n + 1; End;
    T := It.NextPCBObject;
  End;
  Board.BoardIterator_Destroy(It);
  Board.ViewManager_FullUpdate;
  ShowMessage('Selected Mech1 tracks: ' + IntToStr(n));
End;
