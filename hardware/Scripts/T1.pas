Procedure T1A;
Var Board : IPCB_Board;
Begin
  Board := PCBServer.GetCurrentPCBBoard;
  If Board = Nil Then Exit;
  Board.GraphicalView_ZoomRedraw;
  Client.SendMessage('PCB:Zoom', 'Action=All', 255, Client.CurrentView);
End;
Procedure T1B;
Var L : TStringList;
Begin
  L := TStringList.Create;
  L.Add('hello');
  L.SaveToFile('C:\Users\12502784\Documents\IoTSynthPatch\IoT_SynthPatch\hardware\Scripts\t1.txt');
  Client.SendMessage('PCB:Zoom', 'Action=All', 255, Client.CurrentView);
End;
