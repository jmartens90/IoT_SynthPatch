{ Exports vias, tracks and pads (with nets) so Claude can review routing and stitching.
  Run with the PCB open: File > Run Script > ExportPads.pas > ExportPads. }
Function NetOf(O : IPCB_Primitive) : String;
Begin
  If O.Net = Nil Then Result := '' Else Result := O.Net.Name;
End;

Function MM(C : TCoord) : String;
Begin
  Result := FloatToStr(CoordToMMs(C));
End;

Function Owner(O : IPCB_Pad) : String;
Begin
  If O.Component = Nil Then Result := '' Else Result := O.Component.Name.Text + ';' + O.Component.Pattern;
End;

Procedure ExportPads;
Var
  Board : IPCB_Board;
  Iter  : IPCB_BoardIterator;
  O     : IPCB_Primitive;
  L     : TStringList;
  X0, Y0 : TCoord;
Begin
  Board := PCBServer.GetCurrentPCBBoard;
  If Board = Nil Then Exit;
  X0 := Board.XOrigin;
  Y0 := Board.YOrigin;
  L := TStringList.Create;
  Iter := Board.BoardIterator_Create;
  Iter.AddFilter_ObjectSet(MkSet(eViaObject, eTrackObject, ePadObject));
  Iter.AddFilter_LayerSet(AllLayers);
  Iter.AddFilter_Method(eProcessAll);
  O := Iter.FirstPCBObject;
  While O <> Nil Do
  Begin
    If O.ObjectId = eViaObject Then
      L.Add('V;' + NetOf(O) + ';' + MM(O.X - X0) + ';' + MM(O.Y - Y0) + ';' + MM(O.Size) + ';' + MM(O.HoleSize))
    Else If O.ObjectId = eTrackObject Then
      L.Add('T;' + NetOf(O) + ';' + MM(O.X1 - X0) + ';' + MM(O.Y1 - Y0) + ';' + MM(O.X2 - X0) + ';' + MM(O.Y2 - Y0) + ';' + MM(O.Width) + ';' + IntToStr(O.Layer))
    Else
      L.Add('P;' + NetOf(O) + ';' + MM(O.X - X0) + ';' + MM(O.Y - Y0) + ';' + MM(O.TopXSize) + ';' + MM(O.TopYSize) + ';' + IntToStr(O.Layer) + ';' + O.Name + ';' + Owner(O));
    O := Iter.NextPCBObject;
  End;
  Board.BoardIterator_Destroy(Iter);
  L.SaveToFile('C:\Users\12502784\Documents\Altium_designs\IoT_SYNTHPATCH\Scripts\pads_export.txt');
  L.Free;
  ShowMessage('Export done');
End;
